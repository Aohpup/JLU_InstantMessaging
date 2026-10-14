"""检查第三批实际菜单；--demo 演示启动与 --empty 启动分别验证。"""
import argparse
import os
from pathlib import Path
import subprocess

parser=argparse.ArgumentParser()
parser.add_argument('--project',type=Path,default=Path(__file__).resolve().parents[1])
parser.add_argument('--output',type=Path)
args=parser.parse_args()
root=args.project.resolve()
dest=args.output or root/'build/debug/p2_batch3_acceptance'
dest.mkdir(parents=True,exist_ok=True)
env=dict(os.environ)
env['PATH']='C:/mingw64/bin;'+env.get('PATH','')

def run(name,script,options=(),code=0):
    result=subprocess.run([str(root/'build/debug/bin/jlu_im.exe'),*(options or ['--demo'])],
                          input=script.encode('utf-8'),capture_output=True,timeout=15,env=env)
    text=result.stdout.decode('utf-8').replace('\r\n','\n')
    errors=result.stderr.decode('utf-8').replace('\r\n','\n')
    (dest/(name+'.txt')).write_text(text+errors,encoding='utf-8')
    assert result.returncode==code,(name,errors)
    return text+errors

# A 在三个服务中都有六个群，同号群资料必须切换为对应服务。
script='1\n1\n10001\nDemoOnly123!\n9\n'
for number in range(1001,1007):script+=f'{number}\n2\n1\n'
script+='11wds\n1001\n2\n1\n9999\n0\n4\n2\n1\n9\n'
for number in range(1001,1007):script+=f'{number}\n2\n1\n'
script+='0\n4\n3\n1\n9\n'
for number in range(1001,1007):script+=f'{number}\n2\n1\n'
script+='0\n0\n'
# 默认演示数据之后的正常注册仍有效，新用户不会自动参加演示群。
script+='2\n1\n新同学\n2000-01-01\n吉林\n新昵称\nDemoOnly123!\nDemoOnly123!\n'
script+='1\n1\n10006\nDemoOnly123!\n9\n1001\n2\n0\n0\n0\n'
out=run('演示群查询',script)
for label in ['6 名人员、18 个群','演示F','身份：群主','身份：管理员','身份：普通成员',
              '当前服务中不存在该群号','当前账号不是该群成员','当前服务暂无加入的群','请输入 0 至']:
    assert label in out,label
for service in ['QQ','微信','微博']:
    for number in range(1001,1007):assert f'群名：{service}演示群{number}' in out
assert out.count('操作失败：')==2,out
# 密码仅作为演示凭据显示一次，输入不会被重复输出。
assert out.count('DemoOnly123!')==1
assert '>>>按任意键继续' not in out
print('PASS demo: 18 groups, service isolation, roles, invalid input, new registration and non-member rejection.')

out=run('空平台启动','2\n1\n空平台甲\n2000-01-01\n吉林\n甲\nDemoOnly123!\nDemoOnly123!\n'
        '1\n1\n10001\nDemoOnly123!\n9\n0\n0\n0\n',['--empty'])
assert '当前服务暂无加入的群' in out and '注册成功。QQ 登录号码：10001' in out
assert '演示A' not in out and 'DemoOnly123!' not in out and '操作失败' not in out
print('PASS empty: original account numbering and blank group container.')
out=run('无效启动参数','',['--unknown'],1)
assert '用法：jlu_im.exe [--empty | --demo | --database 路径 | --help]' in out
print('PASS invalid arguments: no silent fallback.')
