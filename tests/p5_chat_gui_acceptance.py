"""以两个真实 Qt 进程点击生产控件，使用同一基线的两个独立 SQLite 副本。"""
import argparse
import json
import os
from pathlib import Path
import queue
import shutil
import socket
import sqlite3
import subprocess
import tempfile
import threading


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', required=True)
    parser.add_argument('--output-dir', required=True)
    parser.add_argument('--service', type=int, default=0)
    args = parser.parse_args()
    root = Path(args.output_dir).resolve()
    root.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix='gui_pair_', dir=root))
    executable = str(Path(args.exe).resolve())
    env = dict(os.environ, QT_QPA_PLATFORM='offscreen', JLU_TEST_SERVICE=str(args.service))
    processes = []
    report = {'status': 'failed', 'directory': str(folder), 'platform': 'Qt offscreen, production MainWindow widgets', 'processes': []}
    try:
        prepared = subprocess.run([executable, '--prepare', str(folder/'base.db')], env=env,
                                  capture_output=True, text=True, encoding='utf-8', timeout=15,
                                  creationflags=subprocess.CREATE_NO_WINDOW)
        assert prepared.returncode == 0, prepared.stderr
        for name in ['A', 'B']:
            shutil.copyfile(folder/'base.db', folder/f'{name}.db')
        with socket.socket() as reserved:
            reserved.bind(('127.0.0.1', 0))
            port = reserved.getsockname()[1]
        report['port'] = port
        report['service'] = args.service
        report['address'] = '127.0.0.1'

        def start(role):
            process = subprocess.Popen([executable, '--listen' if role == 'A' else '--connect',
                                        str(folder/f'{role}.db'), str(port), str(folder)], env=env,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                       text=True, encoding='utf-8', errors='replace',
                                       creationflags=subprocess.CREATE_NO_WINDOW)
            lines, events = [], queue.Queue()
            def read():
                for line in process.stdout:
                    lines.append(line); events.put(line)
                events.put(None)
            reader = threading.Thread(target=read, daemon=True); reader.start()
            item = (process, lines, events, reader, role); processes.append(item)
            return item
        a = start('A')
        while True:
            line = a[2].get(timeout=20)
            assert line is not None, ''.join(a[1])
            if line.startswith('LISTENING'):
                break
        b = start('B')
        for item in [a, b]:
            item[0].wait(timeout=30); item[3].join(timeout=2)
            entry = {'role': item[4], 'pid': item[0].pid, 'exit_code': item[0].returncode, 'output': ''.join(item[1])}
            report['processes'].append(entry)
            (folder/f'{item[4]}.log').write_text(entry['output'], encoding='utf-8')
        assert all(item['exit_code'] == 0 for item in report['processes']), report['processes']
        assert a[0].pid != b[0].pid
        report['reload'] = []
        for role, owner in [('A', 'wx_20001' if args.service == 1 else '10001'), ('B', 'wx_20003' if args.service == 1 else ('10005' if args.service == 2 else '10002'))]:
            restored = subprocess.run([executable, '--reload-a' if role == 'A' else '--reload-b',
                                       str(folder/f'{role}.db'), '0', str(folder)], env=env,
                                      capture_output=True, text=True, encoding='utf-8', timeout=20,
                                      creationflags=subprocess.CREATE_NO_WINDOW)
            assert restored.returncode == 0, restored.stdout + restored.stderr
            with sqlite3.connect(folder/f'{role}.db') as db:
                records = db.execute('SELECT owner_account, body, delivery_state, service FROM chat_messages ORDER BY id').fetchall()
            assert len(records) == 6 and all(r[0] == owner and r[2] == 1 and r[3] == args.service for r in records), records
            report['reload'].append({'role': role, 'exit_code': restored.returncode, 'output': restored.stdout, 'rows': len(records)})
        report['status'] = 'passed' 
        print('PASS: two independent Qt processes, isolated A/B databases, real controls and TCP, screenshots saved.')
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        for process, lines, _, reader, role in processes:
            if process.poll() is None:
                process.kill(); process.wait(timeout=5)
            reader.join(timeout=2); process.stdout.close()
            (folder/f'{role}.log').write_text(''.join(lines), encoding='utf-8')
        (root/'gui_results.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
