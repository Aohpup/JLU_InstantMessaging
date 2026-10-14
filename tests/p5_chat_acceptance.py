"""第二批真实进程验收；原始 Socket 对端用于制造分段、错误握手和丢失 ACK。"""
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
    args_parser = argparse.ArgumentParser()
    args_parser.add_argument('--exe', required=True)
    args_parser.add_argument('--output', required=True)
    args = args_parser.parse_args()
    processes, records = [], []
    report = {'status': 'failed', 'cases': records}

    def start(role='listen', port=0, own='10001', peer='10002', hold=True):
        command = [str(Path(args.exe).resolve()), role, '127.0.0.1', str(port), own, peer]
        if hold:
            command.append('hold')
        proc = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, encoding='utf-8',
                                creationflags=subprocess.CREATE_NO_WINDOW)
        lines, events = [], queue.Queue()

        def read():
            try:
                for line in proc.stdout:
                    lines.append(line)
                    events.put(line)
            finally:
                events.put(None)
        reader = threading.Thread(target=read, daemon=True)
        reader.start()
        item = (proc, lines, events, reader)
        processes.append(item)
        return item

    def wait_line(item, prefix, timeout=10):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            try:
                line = item[2].get(timeout=max(0.01, end - time.monotonic()))
            except queue.Empty:
                raise AssertionError('Timed out waiting for ' + prefix + ': ' + ''.join(item[1])[:1000])
            if line is None:
                raise AssertionError('Process closed before ' + prefix + ': ' + ''.join(item[1])[:1000])
            if line.startswith(prefix):
                return line
        raise AssertionError('Timed out: ' + prefix)

    def finish(item, timeout=5):
        item[0].wait(timeout=timeout)
        item[3].join(timeout=2)
        return {'pid': item[0].pid, 'exit_code': item[0].returncode, 'output': ''.join(item[1])}

    def stop(item):
        begin = time.monotonic()
        item[0].stdin.write('stop\n')
        item[0].stdin.flush()
        result = finish(item)
        result['stop_ms'] = round((time.monotonic() - begin) * 1000, 1)
        assert result['stop_ms'] < 1000, result
        return result

    def raw_peer():
        proc = start()
        port = int(wait_line(proc, 'LISTENING ').split()[1])
        raw = socket.create_connection(('127.0.0.1', port), timeout=3)
        read_frame(raw)  # 程序发出的 HELLO。
        return proc, raw

    def exact(raw, count):
        result = b''
        while len(result) < count:
            part = raw.recv(count - len(result))
            assert part, 'Peer closed before complete frame'
            result += part
        return result

    def read_frame(raw):
        size = struct.unpack('!I', exact(raw, 4))[0]
        assert 0 < size <= 65536
        fields = exact(raw, size).decode('utf-8').split('\n', 8)
        assert len(fields) == 9
        return fields

    def frame(kind, number='-', body='', sender='10002', version='2', token='raw-b2', service='0'):
        data = f'{version}\n{kind}\n{service}\n{token}\n{number}\n{sender}\n10001\n123\n{body}'.encode('utf-8')
        return struct.pack('!I', len(data)) + data

    def greet(raw):
        raw.sendall(frame('HELLO'))

    def read_texts(raw, acknowledge=False):
        messages = []
        while len(messages) < 3:
            fields = read_frame(raw)
            if fields[1] == 'TEXT':
                messages.append(fields)
                if acknowledge:
                    raw.sendall(frame('ACK', fields[4]))
        return messages

    try:
        a = start(hold=False)
        port = int(wait_line(a, 'LISTENING ').split()[1])
        b = start('connect', port, '10002', '10001', False)
        out_a, out_b = finish(a, 12), finish(b, 12)
        assert out_a['pid'] != out_b['pid']
        for output, sender in [(out_a, '10002'), (out_b, '10001')]:
            assert output['exit_code'] == 0 and 'SUMMARY received=3 ack=3' in output['output']
            assert output['output'].count('INCOMING ') == 3
            assert f'{sender}：第一条中文消息。\n保留第二行。🙂' in output['output']
        records.append({'case': 'two_real_processes_three_messages_and_ack', 'A': out_a, 'B': out_b})

        proc, raw = raw_peer()
        with raw:
            hello = frame('HELLO')
            raw.sendall(hello[:2]); time.sleep(0.03); raw.sendall(hello[2:])
            wait_line(proc, 'READY ')
            first = frame('TEXT', 'raw-b2-10002-1', '原始对端中文🙂\n第二行')
            # 拆开长度、头部及 UTF-8 字节，再连续合并发送两条完整帧。
            for part in [first[:3], first[3:35], first[35:-2], first[-2:]]:
                raw.sendall(part)
                time.sleep(0.01)
            raw.sendall(frame('TEXT', 'raw-b2-10002-2', '连续第二条') + frame('TEXT', 'raw-b2-10002-3', '连续第三条'))
            text_count = ack_count = 0
            while text_count < 3 or ack_count < 3:
                fields = read_frame(raw)
                if fields[1] == 'TEXT':
                    text_count += 1
                    raw.sendall(frame('ACK', fields[4]))
                elif fields[1] == 'ACK':
                    ack_count += 1
            # 等待事件被演示主线程展示，确认没有把 send 成功当作收到 ACK。
            end = time.monotonic() + 3
            while ''.join(proc[1]).count('DELIVERY 0 对端已接收') < 3 and time.monotonic() < end:
                time.sleep(0.01)
            output = stop(proc)
            assert output['exit_code'] == 0 and 'SUMMARY received=3 ack=3' in output['output']
            records.append({'case': 'fragmented_and_coalesced_frames_active_cancel', **output})

        for name, invalid, reason in [
            ('wrong_service_handshake', frame('HELLO', service='2'), '服务不匹配'),
            ('wrong_qq_handshake', frame('HELLO', sender='10003'), '身份不匹配'),
            ('wrong_version_handshake', frame('HELLO', version='1'), '版本不匹配')]:
            proc, raw = raw_peer()
            with raw:
                raw.sendall(invalid)
                output = finish(proc)
            assert output['exit_code'] == 1 and reason in output['output'] and 'READY ' not in output['output']
            records.append({'case': name, **output})

        proc, raw = raw_peer()
        with raw:
            greet(raw)
            wait_line(proc, 'READY ')
            raw.sendall(frame('TEXT', 'old-token-10002-1', '不应进入新会话', token='old-token'))
            output = finish(proc)
        assert output['exit_code'] == 1 and '不属于本次已握手连接' in output['output']
        assert 'INCOMING ' not in output['output']
        records.append({'case': 'stale_connection_frame_rejected', **output})

        proc, raw = raw_peer()
        with raw:
            greet(raw)
            read_texts(raw)
            raw.sendall(frame('ACK', 'old-connection-10001-1'))
            time.sleep(0.15)
            assert 'DELIVERY 0 对端已接收' not in ''.join(proc[1])
        output = finish(proc)
        assert output['output'].count('DELIVERY 0 结果未知') == 3
        records.append({'case': 'unmatched_ack_ignored_disconnect_unknown', **output})

        proc, raw = raw_peer()
        with raw:
            greet(raw)
            read_texts(raw)
            output = finish(proc, 13)
        assert '确认超时' in output['output'] and output['output'].count('DELIVERY 0 结果未知') == 3
        records.append({'case': 'ack_timeout_unknown_no_retry', **output})

        proc, raw = raw_peer()
        with raw:
            output = finish(proc, 8)
        assert output['exit_code'] == 1 and '握手超时' in output['output']
        records.append({'case': 'handshake_timeout', **output})

        proc, raw = raw_peer()
        with raw:
            output = stop(proc)
        assert output['exit_code'] == 0 and 'READY ' not in output['output']
        records.append({'case': 'cancel_during_handshake', **output})

        proc = start()
        wait_line(proc, 'LISTENING ')
        output = stop(proc)
        assert output['exit_code'] == 0
        records.append({'case': 'cancel_during_listening', **output})

        # 用本机满连接队列制造尚未完成的 connect，不访问外部地址。
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
            server.bind(('127.0.0.1', 0))
            server.listen(1)
            fillers = []
            try:
                for _ in range(8):
                    filler = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    filler.setblocking(False)
                    filler.connect_ex(server.getsockname())
                    fillers.append(filler)
                time.sleep(0.15)
                proc = start('connect', server.getsockname()[1])
                wait_line(proc, 'CONNECTING ')
                time.sleep(0.15)
                assert 'HANDSHAKING ' not in ''.join(proc[1]) and proc[0].poll() is None, 'Backlog did not hold connect pending'
                output = stop(proc)
                assert output['exit_code'] == 0
                records.append({'case': 'cancel_pending_connect_local_backlog', **output})
            finally:
                for filler in fillers:
                    filler.close()
        # 原始对端强制发出 RST，验证突然断线不是成功收尾。
        proc, raw = raw_peer()
        greet(raw)
        read_texts(raw, acknowledge=True)
        raw.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('hh', 1, 0))
        raw.close()
        output = finish(proc, 8)
        assert output['exit_code'] == 1 and 'ERROR ' in output['output'], output
        records.append({'case': 'peer_reset_abrupt_disconnect', **output})
        report['status'] = 'passed'
        print(f'P5 batch 2: {len(records)} real-process scenarios passed.')
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        for proc, _, _, reader in processes:
            if proc.poll() is None:
                proc.kill(); proc.wait(timeout=5)
            reader.join(timeout=2)
            proc.stdin.close(); proc.stdout.close()
        output = Path(args.output)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')


if __name__ == '__main__':
    main()
