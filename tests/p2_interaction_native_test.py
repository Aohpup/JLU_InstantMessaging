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
    p=subprocess.Popen([exe, '--demo'],stdin=stdin,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env)
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
        def act(send, marker):
            before=output().count(marker)
            keys(send);wait(marker,before+1)
        wait('0 退出')
        act('1\r','选择服务');act('1\r','对外号码');act('10001\r','密码（')
        act('DemoOnly123!\r','>>>按任意键继续');act('x','9 群管理')
        act('9\r','选择群号');ctrl()
        act('9999\r','>>>按任意键继续')
        assert '当前服务中不存在该群号' in output()
        assert '目标账号号码' not in output() and '2 查询群资料和成员' not in output()
        act('x','选择群号');act('1001\r','10 转让群主')
        act('8\r','目标账号号码');ctrl()
        assert output().find('身份：管理员')<output().find('目标账号号码')
        act('10001\r','>>>按任意键继续');assert '不可移除自己' in output()
        assert '确认执行' not in output()
        before=output().count('10 转让群主');ctrl()
        assert output().count('10 转让群主')==before,'Ctrl+C skipped pause'
        act('x','10 转让群主');act('8\r','目标账号号码')
        act('10002\r','>>>按任意键继续')
        assert '请先取消其管理员身份再移除' in output() and '确认执行' not in output()
        act('x','10 转让群主');act('9\r','目标账号号码');act('10002\r','管理员：1 设置')
        act('2\r','确认执行');ctrl();act('0\r','>>>按任意键继续')
        act('x','10 转让群主');start=len(output());act('2\r','>>>按任意键继续')
        assert '号码：10002 | 用户昵称：演示BQQ | 身份：管理员' in output()[start:]
        # 使用真实控制台输入完成子群创建、取消切换、保留子群与主群权限恢复。
        act('x','10 转让群主');act('11\r','新子群群号')
        act('2001\r','子群名称');act('原生讨论组\r','子群群主账号号码')
        act('10001\r','成员号码');act('10002\r','成员号码')
        act('0\r','确认创建子群');ctrl();act('1\r','>>>按任意键继续')
        assert '子群已创建，群号：2001' in output()
        act('x','10 转让群主');act('13\r','目标管理模式')
        act('2\r','确认切换');act('0\r','>>>按任意键继续')
        assert '群管理模式已切换为微信' not in output()
        act('x','10 转让群主');act('13\r','目标管理模式')
        act('2\r','确认切换');act('1\r','>>>按任意键继续')
        act('x','10 转让群主');act('12\r','>>>按任意键继续')
        assert '子群号：2001' in output()
        act('x','10 转让群主');act('1\r','选择群号');start=len(output())
        act('2001\r','10 转让群主')
        assert '当前服务中不存在该群号' not in output()[start:]
        act('1\r','选择群号');act('1001\r','10 转让群主')
        act('13\r','目标管理模式');act('1\r','确认切换');act('1\r','>>>按任意键继续')
        assert '群管理模式已切换为QQ' in output()
        act('x','10 转让群主');start=len(output());act('2\r','>>>按任意键继续')
        assert '号码：10002 | 用户昵称：演示BQQ | 身份：管理员' in output()[start:]
        act('x','10 转让群主');pauses=output().count('>>>按任意键继续')
        act('0\r','9 群管理')
        assert output().count('>>>按任意键继续')==pauses,'Returning added pause'
        act('0\r','>>>按任意键继续');act('x','主菜单');keys('0\r')
        assert p.wait(timeout=5)==0;t.join(timeout=1)
        assert output().count('DemoOnly123!')==1,'Password echoed beyond demo legend'
        dest=project/'build/debug/interaction_0929';dest.mkdir(parents=True,exist_ok=True)
        (dest/'原生交互检查.txt').write_text(output(),encoding='utf-8')
        print('PASS native console: invalid group, early self/admin rejection, member listing, cancel demotion, Ctrl+C, pause/return, subgroup preservation and parent mode restoration.')
    finally:
        if p.poll() is None:p.kill();p.wait()
        k.SetConsoleCtrlHandler(None,False)

