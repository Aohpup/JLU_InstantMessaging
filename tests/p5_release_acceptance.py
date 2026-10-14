"""复制完整 Release 到中文目录，清除开发 PATH，验证真实窗口及默认库位置。"""
from pathlib import Path
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
import shutil
import sqlite3
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parents[1]
folder = root / 'build/p5_final'
folder.mkdir(parents=True, exist_ok=True)
source = root / 'dist/JLU_InstantMessaging'
relocated = Path(tempfile.mkdtemp(prefix='移动目录 ', dir=folder)) / '即时通信'
shutil.copytree(source, relocated, ignore=shutil.ignore_patterns('data', '*.db', '*.db-*'))
exe = relocated / 'jlu_im_gui.exe'
default = relocated / 'data/jlu_im.db'
formal = root / 'data/jlu_im.db'
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None
before = digest(formal)
env = dict(os.environ)
env['PATH'] = str(Path(os.environ['SYSTEMROOT']) / 'System32')
for key in list(env):
    if key.startswith('QT_'): del env[key]
user32 = ctypes.WinDLL('user32', use_last_error=True)
callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
user32.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.SendMessageTimeoutW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM,
                                    wintypes.LPARAM, wintypes.UINT, wintypes.UINT, ctypes.c_void_p]
user32.SendMessageTimeoutW.restype = ctypes.c_ssize_t
results = []
def run_gui(args):
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen([str(exe), *map(str,args)], cwd=folder, env=env, startupinfo=startup)
    windows = []
    @callback_type
    def inspect(hwnd, _):
        pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value == process.pid:
            title = ctypes.create_unicode_buffer(256)
            user32.GetWindowTextW(hwnd, title, len(title))
            if title.value == 'JLU 即时通信 · 图形界面': windows.append(hwnd)
        return True
    try:
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline and not windows and process.poll() is None:
            user32.EnumWindows(inspect, 0)
            time.sleep(.1)
        assert windows, ('Release did not start', args, process.poll())
        assert user32.SendMessageTimeoutW(windows[0], 0x0010, 0, 0, 2, 5000, None)
        assert process.wait(timeout=8) == 0
        results.append({'entry':'GUI', 'args':list(map(str,args)), 'exit':0})
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)

def run_console(args, script=None):
    p = subprocess.run([str(relocated/'jlu_im.exe'),*map(str,args)], cwd=folder, env=env,
                       input=script.encode('utf-8') if script else None, capture_output=True, timeout=15)
    output = (p.stdout+p.stderr).decode('utf-8',errors='replace')
    assert p.returncode == 0, (p.returncode, output)
    results.append({'entry':'console', 'args':list(map(str,args)), 'exit':0})
    (folder / ('release-console-'+str(len(results))+'.log')).write_text(output,encoding='utf-8')
    return output

run_gui(['--demo'])
run_gui(['--empty'])
assert not default.exists(), 'memory modes created database'
run_gui([])
assert default.exists() and not (folder/'data/jlu_im.db').exists(), 'wrong default directory'
run_gui([])
custom = folder/'Release 指定库.db'
custom.unlink(missing_ok=True)
run_gui(['--database',custom])
run_gui(['--database',custom])
for path in [default, custom]:
    with sqlite3.connect(path) as db:
        assert db.execute('SELECT count(*) FROM persons').fetchone() == (6,)
        assert db.execute('SELECT count(*) FROM chat_groups').fetchone() == (18,)
        assert not db.execute('PRAGMA foreign_key_check').fetchall()
help_text = run_console(['--help'])
assert str(default).replace('\\','/') in help_text.replace('\\','/'), help_text
out = run_console([], '1\n1\n10001\nDemoOnly123!\n6\n发布版昵称\n0\n0\n')
assert '登录成功' in out and '操作失败：' not in out
with sqlite3.connect(default) as db:
    assert db.execute("SELECT nickname FROM service_accounts WHERE service=0 AND external_id='10001'").fetchone() == ('发布版昵称',)
run_gui([])
with sqlite3.connect(default) as db:
    assert db.execute("SELECT nickname FROM service_accounts WHERE service=0 AND external_id='10001'").fetchone() == ('发布版昵称',)
assert digest(formal) == before
import zipfile
with zipfile.ZipFile(root/'dist/JLU_InstantMessaging_P5_Windows_x64.zip') as package:
    assert not any('/data/' in n or '.db' in Path(n).name for n in package.namelist()), 'zip includes a database' 
report = {'date':'2026-10-13','result':'PASS','path':str(relocated),'environment_PATH':env['PATH'],
          'runs':results,'formal_database_unchanged':True,'zip_has_no_database':True,
          'files':{str(p.relative_to(source)):digest(p) for p in source.rglob('*') if p.is_file()}}
(folder/'release.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('PASS Release: 7 actual GUI startups, 2 console runs; relocated Chinese path, no development PATH, portable/default/custom DB, memory modes.')
