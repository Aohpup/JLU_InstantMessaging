import ctypes as c
from ctypes import wintypes as w
import msvcrt
import os
from pathlib import Path
import subprocess
import sys
import threading
import time

if '--worker' not in sys.argv:
    r = subprocess.run([sys.executable, '-X', 'utf8', __file__, '--worker'],
                       creationflags=subprocess.CREATE_NO_WINDOW, capture_output=True, timeout=45)
    print(r.stdout.decode('utf-8', errors='replace'))
    print(r.stderr.decode('utf-8', errors='replace'))
    raise SystemExit(r.returncode)

k = c.WinDLL('kernel32', use_last_error=True)
k.CreateFileW.argtypes = [w.LPCWSTR,w.DWORD,w.DWORD,c.c_void_p,w.DWORD,w.DWORD,w.HANDLE]
k.CreateFileW.restype = w.HANDLE
k.SetConsoleCtrlHandler.argtypes = [c.c_void_p,w.BOOL]
k.GenerateConsoleCtrlEvent.argtypes = [w.DWORD,w.DWORD]
class Char(c.Union):
    _fields_=[('UnicodeChar',w.WCHAR),('AsciiChar',c.c_char)]
class Key(c.Structure):
    _fields_=[('down',w.BOOL),('repeat',w.WORD),('vk',w.WORD),('scan',w.WORD),('char',Char),('state',w.DWORD)]
class Event(c.Union):
    _fields_=[('key',Key),('padding',c.c_byte*16)]
class Record(c.Structure):
    _fields_=[('kind',w.WORD),('event',Event)]
k.WriteConsoleInputW.argtypes=[w.HANDLE,c.POINTER(Record),w.DWORD,c.POINTER(w.DWORD)]
h=k.CreateFileW('CONIN$',0xC0000000,3,None,3,0,None)
assert h and h != w.HANDLE(-1).value
fd=msvcrt.open_osfhandle(h,os.O_RDONLY)
env=dict(os.environ); env['PATH']='C:/mingw64/bin;'+env.get('PATH','')
project=Path(__file__).resolve().parents[1]
exe=str(project/'build/debug/bin/jlu_im.exe')
with os.fdopen(fd,'rb',buffering=0) as stdin:
    p=subprocess.Popen([exe, '--empty'],stdin=stdin,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env)
    # 子进程启动后才设置测试驱动的忽略标志，避免它继承标志而产生假通过。
    assert k.SetConsoleCtrlHandler(None,True)
    data=bytearray(); lock=threading.Lock()
    def reader():
        while part:=p.stdout.read(1):
            with lock: data.extend(part)
    t=threading.Thread(target=reader,daemon=True); t.start()
    def output():
        with lock:return bytes(data).decode('utf-8',errors='replace')
    def wait(text,count=1):
        deadline=time.monotonic()+6
        while time.monotonic()<deadline:
            if output().count(text)>=count:return
            if p.poll() is not None:raise AssertionError('Exited early: '+output())
            time.sleep(.02)
        raise AssertionError('Missing '+repr(text)+': '+output())
    def keys(text):
        events=[]
        for ch in text:
            for down in (True,False):
                r=Record();r.kind=1;r.event.key.down=down;r.event.key.repeat=1
                r.event.key.vk=13 if ch=='\r' else ord(ch.upper()) if ch.isascii() and ch.isalnum() else 0
                r.event.key.char.UnicodeChar=ch
                events.append(r)
        a=(Record*len(events))(*events);written=w.DWORD()
        assert k.WriteConsoleInputW(h,a,len(events),c.byref(written)) and written.value==len(events)
    def ctrl():
        assert k.GenerateConsoleCtrlEvent(0,0)
        time.sleep(.15)
        assert p.poll() is None,'Ctrl+C killed process: '+output()
    try:
        wait('0 退出');ctrl();keys('2\r');wait('选择服务');keys('1\r');wait('姓名：')
        ctrl();keys('测试甲\r');wait('生日（');ctrl();keys('2000-01-01\r')
        wait('所在地：');keys('长春\r');wait('用户昵称：');keys('测试昵称\r');wait('设置密码（')
        ctrl();keys('DemoOnly123!\r');wait('再次输入密码（');ctrl();keys('DemoOnly123!\r')
        wait('>>>按任意键继续');before=output().count('主菜单');ctrl()
        assert output().count('主菜单')==before,'Ctrl+C skipped pause'
        keys('\x03');time.sleep(.1);assert output().count('主菜单')==before,'Raw Ctrl+C skipped pause'
        keys('x');wait('主菜单',2);keys('1\r');wait('选择服务',2);keys('1\r')
        wait('对外号码');keys('10001\r');wait('密码（隐藏密码，* 号占位，0 返回）');ctrl();keys('DemoOnly123!\r')
        wait('登录成功');wait('>>>按任意键继续',2);keys('x');wait('6 修改当前服务')
        keys('1\r');wait('姓名：测试甲');wait('>>>按任意键继续',3);ctrl();keys('x')
        wait('6 修改当前服务',2);keys('0\r');wait('>>>按任意键继续',4);keys('x')
        wait('主菜单',3);keys('0\r');assert p.wait(timeout=5)==0;t.join(timeout=1)
        assert '操作失败' not in output() and 'DemoOnly123!' not in output(),output()
        logdir=project/'build/debug/acceptance'
        logdir.mkdir(parents=True,exist_ok=True)
        (logdir/'ctrl_c_native_output.txt').write_text(output(),encoding='utf-8')
        print('PASS native actual executable: Ctrl+C at menu, name, birthday, password, repeat, login password, pause; Chinese input; normal logout/exit; no secret echo.')
    finally:
        if p.poll() is None:p.kill();p.wait()
        k.SetConsoleCtrlHandler(None,False)

