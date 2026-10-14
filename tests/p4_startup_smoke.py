"""验证实际 GUI 程序的 Windows 启动和正常关闭，仅使用 build 内的隔离数据库。"""
from pathlib import Path
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
import sqlite3
import subprocess
import time

root = Path(__file__).resolve().parents[1]
folder = root / 'build/gui-debug/p4_batch1'
folder.mkdir(parents=True, exist_ok=True)
exe = root / 'build/gui-debug/bin/jlu_im_gui.exe'
database = folder / '实际窗口启动.db'
database.unlink(missing_ok=True)
formal = root / 'data/jlu_im.db'
before = hashlib.sha256(formal.read_bytes()).hexdigest() if formal.exists() else None
user32 = ctypes.WinDLL('user32', use_last_error=True)
callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
user32.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.SendMessageTimeoutW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM,
                                    wintypes.LPARAM, wintypes.UINT, wintypes.UINT, ctypes.c_void_p]
user32.SendMessageTimeoutW.restype = ctypes.c_ssize_t
results = []

def run(args):
    # 清除开发 Qt 路径，核对 exe 目录中的 DLL 和 qwindows 插件足够启动。
    env = dict(os.environ)
    env['PATH'] = str(Path(os.environ['SYSTEMROOT']) / 'System32')
    for key in list(env):
        if key.startswith('QT_'):
            del env[key]
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen([str(exe), *args], cwd=folder, env=env, startupinfo=startup)
    windows = []
    @callback_type
    def inspect(hwnd, _):
        pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value == process.pid:
            title = ctypes.create_unicode_buffer(256)
            user32.GetWindowTextW(hwnd, title, len(title))
            if title.value == 'JLU 即时通信 · 图形界面':
                windows.append(hwnd)
        return True
    try:
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline and not windows and process.poll() is None:
            user32.EnumWindows(inspect, 0)
            time.sleep(0.1)
        assert windows, f'窗口未正常启动: {args}, exit={process.poll()}'
        # 只向本测试启动的窗口发送关闭请求，触发真实 closeEvent 和保存。
        assert user32.SendMessageTimeoutW(windows[0], 0x0010, 0, 0, 2, 5000, None)
        assert process.wait(timeout=8) == 0
        results.append({'arguments': args, 'started_and_closed': True})
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)

run(['--demo'])
run(['--empty'])
run(['--database', str(database)])
run(['--database', str(database)])
with sqlite3.connect(database) as connection:
    assert connection.execute('SELECT count(*) FROM persons').fetchone()[0] == 6
    assert connection.execute('SELECT count(*) FROM service_accounts').fetchone()[0] == 12
    assert connection.execute('SELECT count(*) FROM chat_groups').fetchone()[0] == 18
after = hashlib.sha256(formal.read_bytes()).hexdigest() if formal.exists() else None
assert before == after, '正式数据库被修改'
report = {'date': '2026-10-05', 'result': 'PASS', 'runs': results,
          'database_counts': {'persons': 6, 'accounts': 12, 'groups': 18},
          'formal_database_unchanged': True}
(folder / 'startup-check.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print('PASS: actual GUI demo/empty/new database/existing database startup and normal close; formal database unchanged.')

