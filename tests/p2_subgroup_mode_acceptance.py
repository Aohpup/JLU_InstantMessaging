"""第五批实际菜单主线：创建、切换保留、父子成员同步。"""
import os
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[1]
env=dict(os.environ);env['PATH']='C:/mingw64/bin;'+env.get('PATH','')
script=('1\n1\n10001\nDemoOnly123!\n9\n1001\n'
        '11\n2001\n测试讨论组\n10001\n10002\n10003\n0\n1\n12\n'
        '13\n2\n0\n13\n2\n1\n12\n1\n2001\n'
        '2\n1\n1001\n13\n1\n1\n12\n'
        '9\n10002\n2\n1\n8\n10002\n1\n'
        '1\n2001\n2\n0\n0\n0\n')
r=subprocess.run([str(root/'build/debug/bin/jlu_im.exe'), '--demo'],input=script.encode('utf-8'),
                 capture_output=True,env=env,timeout=15)
out=r.stdout.decode('utf-8').replace('\r\n','\n')
dest=root/'build/debug/p2_batch5_acceptance';dest.mkdir(parents=True,exist_ok=True)
(dest/'子群与模式切换.txt').write_text(out,encoding='utf-8')
assert r.returncode==0,r.stderr
for marker in ['子群已创建，群号：2001','子群号：2001','当前群管理模式：微信','保留已有子群',
               '群管理模式已切换为QQ','成员已移除','成员数量：2']:
    assert marker in out,marker
assert out.count('操作失败：')==0,out
assert out.count('群管理模式已切换为微信')==1
last=out.rsplit('成员数量：2',1)[1]
assert '号码：10003' in last and '号码：10002' not in last
assert out.count('DemoOnly123!')==1 and '>>>按任意键继续' not in out
print('PASS actual subgroup creation, cancelled switch, subgroup preservation and parent-child member removal.')


# 阶段演示主线：子群群主直接离开父群被拒绝，转让后退出并同步移除。
script=('1\n1\n10001\nDemoOnly123!\n9\n1001\n'
        '11\n2002\n转让讨论组\n10003\n10001\n0\n1\n0\n0\n'
        '1\n1\n10003\nDemoOnly123!\n9\n1001\n7\n'
        '1\n2002\n10\n10001\n1\n1\n1001\n7\n1\n0\n0\n'
        '1\n1\n10001\nDemoOnly123!\n9\n2002\n2\n0\n0\n0\n')
r=subprocess.run([str(root/'build/debug/bin/jlu_im.exe'), '--demo'],input=script.encode('utf-8'),
                 capture_output=True,env=env,timeout=15)
out=r.stdout.decode('utf-8').replace('\r\n','\n')
(dest/'子群群主转让后退群.txt').write_text(out,encoding='utf-8')
assert r.returncode==0,r.stderr
assert out.count('操作失败：')==1,out
assert '请先转让子群群主' in out and '群主已转让' in out and '已退出群' in out
last=out.rsplit('成员数量：1',1)
assert len(last)==2 and '号码：10001' in last[1] and '号码：10003' not in last[1],out
print('PASS actual child-owner leave rejection, ownership transfer and cascading leave.')
