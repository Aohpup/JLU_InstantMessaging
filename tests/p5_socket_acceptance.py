"""P5 第一批：启动真实进程验证 TCP，所有日志在 build 下，不访问数据库。"""
import argparse
import json
import queue
import socket
import struct
import subprocess
import threading
import time
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    executable = str(Path(args.exe).resolve())
    processes = []
    results = []

    def start(role, port, message):
        process = subprocess.Popen(
            [executable, role, '127.0.0.1', str(port), message],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, encoding='utf-8', errors='strict',
            creationflags=subprocess.CREATE_NO_WINDOW)
        lines = []
        events = queue.Queue()

        def read_output():
            try:
                for line in process.stdout:
                    lines.append(line)
                    events.put(line)
            finally:
                events.put(None)

        reader = threading.Thread(target=read_output, daemon=True)
        reader.start()
        item = (process, lines, events, reader)
        processes.append(item)
        return item

    def ready(item):
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            line = item[2].get(timeout=max(0.01, deadline - time.monotonic()))
            if line is None:
                raise AssertionError('监听未就绪：' + ''.join(item[1]))
            if line.startswith('LISTENING '):
                return int(line.split()[1])
        raise AssertionError('监听启动超时')

    def finish(item):
        item[0].wait(timeout=10)
        item[3].join(timeout=2)
        if item[3].is_alive():
            raise AssertionError('进程已结束，但日志读取未结束')
        return {'pid': item[0].pid, 'exit_code': item[0].returncode,
                'output': ''.join(item[1])}

    def require(condition, message):
        if not condition:
            raise AssertionError(message)

    try:
        # 双端都是项目编译出的独立程序，互相核对实际收到的中文和换行。
        text_a = 'A端：你好，Socket！\n这是第二行。'
        text_b = 'B端：已收到，我也可以发送。'
        a = start('listen', 0, text_a)
        port = ready(a)
        b = start('connect', port, text_b)
        out_a, out_b = finish(a), finish(b)
        results.append({'case': '双进程中文双向交换', 'port': port, 'A': out_a, 'B': out_b})
        require(out_a['pid'] != out_b['pid'], '必须使用两个独立进程')
        require(out_a['exit_code'] == out_b['exit_code'] == 0, '双向收发未正常结束')
        require('收到对端：' + text_b in out_a['output'], 'A 未收到 B 的原文')
        require('收到对端：' + text_a in out_b['output'], 'B 未收到 A 的原文')

        # 重复绑定应报错，原监听仍能继续完成交换。
        a = start('listen', 0, '原监听仍可工作')
        port = ready(a)
        duplicate = finish(start('listen', port, '重复监听'))
        require(duplicate['exit_code'] != 0 and '绑定地址失败' in duplicate['output'],
                '端口占用未给出明确错误')
        b = start('connect', port, '确认原监听正常')
        out_a, out_b = finish(a), finish(b)
        results.append({'case': '端口占用及原监听继续工作', 'duplicate': duplicate, 'A': out_a, 'B': out_b})
        require(out_a['exit_code'] == out_b['exit_code'] == 0, '重复绑定影响了原监听')

        # 对端读完本端发送内容后异常关闭，验证接收报错和进程自行退出。
        a = start('listen', 0, '测试接收中断')
        port = ready(a)
        with socket.create_connection(('127.0.0.1', port), timeout=5) as raw:
            while raw.recv(4096):
                pass
            raw.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('HH', 1, 0))
        interrupted = finish(a)
        results.append({'case': '接收期间对端异常断开', 'listener': interrupted})
        require(interrupted['exit_code'] != 0 and '接收失败' in interrupted['output'],
                '接收中断未报错或未正常退出')

        # 占住一个未监听的端口，确保不会误连其他程序。
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as reserved:
            reserved.bind(('127.0.0.1', 0))
            refused = finish(start('connect', reserved.getsockname()[1], '连接拒绝测试'))
        results.append({'case': '连接被拒绝', 'client': refused})
        require(refused['exit_code'] != 0 and '连接被拒绝' in refused['output'],
                '连接拒绝未给出说明')
        report = {'status': 'passed', 'cases': results}
        print('P5 第一批：4 个关键场景通过（真实双进程收发、端口占用、异常断开、连接拒绝）。')
    except Exception as error:
        report = {'status': 'failed', 'error': str(error), 'cases': results}
        raise
    finally:
        # 仅清理本次脚本自己创建的进程，避免失败后留下等待中的监听。
        for process, _, _, reader in processes:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)
            reader.join(timeout=2)
            process.stdout.close()
        output = Path(args.output)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
