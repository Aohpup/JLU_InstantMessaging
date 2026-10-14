"""第四批实际菜单验证，使用默认演示数据，不连接外部服务。"""
import argparse
import os
from pathlib import Path
import subprocess

parser=argparse.ArgumentParser()
parser.add_argument('--project',type=Path,default=Path(__file__).resolve().parents[1])
parser.add_argument('--output',type=Path)
args=parser.parse_args();root=args.project.resolve()
dest=args.output or root/'build/debug/p2_batch4_acceptance';dest.mkdir(parents=True,exist_ok=True)
env=dict(os.environ);env['PATH']='C:/mingw64/bin;'+env.get('PATH','')
def login(service,number):return f'1\n{service}\n{number}\nDemoOnly123!\n9\n'
logout='0\n0\n'
# QQ：普通成员退群申请、管理员审批、角色限制、撤销管理员及转让后退群。
script=login(1,'10003')+'1001\n7\n1\n1001\n3\n0\n3\n1\n5\n'+logout
script+=login(1,'10002')+'1001\n6\n1\n0\n6\n1\n1\n'+logout
script+=login(1,'10001')+'1001\n9\n10003\n1\n1\n'+logout
script+=login(1,'10002')+'1001\n8\n10003\n'+logout
script+=login(1,'10001')+'1001\n9\n10003\n2\n1\n'+logout
script+=login(1,'10002')+'1001\n8\n10003\n1\n'+logout
script+=login(1,'10003')+'1001\n3\n1\n'+logout
script+=login(1,'10001')+'1001\n6\n2\n1\n10\n10002\n0\n10\n10002\n1\n7\n1\n'+logout
# 微信 1002：D 邀请 F 后离群，旧邀请失效；A 重新邀请，F 拒绝后再接受。
script+=login(2,'wx_20003')+'1002\n4\nwx_20004\n1\n7\n1\n'+logout
script+=login(2,'wx_20004')+'1002\n5\n6\n3\n3\n'+logout
script+=login(2,'wx_20001')+'1002\n4\nwx_20004\n1\n'+logout
script+=login(2,'wx_20004')+'1002\n6\n4\n2\n'+logout
script+=login(2,'wx_20001')+'1002\n4\nwx_20004\n1\n'+logout
script+=login(2,'wx_20004')+'1002\n6\n5\n1\n2\n'+logout
script+=login(2,'wx_20001')+'1002\n8\n'+logout
script+=login(2,'wx_20002')+'1002\n9\n10\nwx_20004\n1\n7\n1\n'+logout
script+=login(2,'wx_20004')+'1002\n8\nwx_20001\n1\n7\n2\n5\n'+logout+'0\n'
result=subprocess.run([str(root/'build/debug/bin/jlu_im.exe'), '--demo'],input=script.encode('utf-8'),
                      capture_output=True,timeout=20,env=env)
out=result.stdout.decode('utf-8').replace('\r\n','\n')
(dest/'群操作完整流程.txt').write_text(out,encoding='utf-8')
assert result.returncode==0,result.stderr
for text in ['管理员不可移除管理员','该请求已处理或已失效','微信群不允许主动申请',
             '微信群不设管理员','群主请先转让群主','已拒绝该请求',
             '群主已转让，原群主成为普通成员','已失效','已拒绝','已同意','成员数量：1']:
    assert text in out,text
assert out.count('操作失败：')==6,out
assert out.count('申请已提交，请求编号：')==2
assert out.count('邀请已发送，请求编号：')==3
assert out.count('已同意，对方已入群。')==3
assert out.count('群主已转让，原群主成为普通成员。')==2
assert out.count('DemoOnly123!')==1 and '>>>按任意键继续' not in out
print('PASS actual QQ/WX group operations: approvals, invite invalidation, rejection/reinvite, role restrictions, admin changes, transfer, leave and cancellation.')
