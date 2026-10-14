"""用实际程序验证共同好友与跨服务推荐，全部使用本地虚构账号。"""
import argparse
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parents[1])
parser.add_argument('--output', type=Path)
args = parser.parse_args()
root = args.project.resolve()
destination = args.output or root / 'build/debug/p2_batch2_acceptance'
destination.mkdir(parents=True, exist_ok=True)
env = dict(os.environ)
env['PATH'] = 'C:/mingw64/bin;' + env.get('PATH', '')


# 脚本输入不走等待按键逻辑，每次调用启动独立的空平台。
def run(name, script, required, forbidden=()):
    result = subprocess.run([str(root / 'build/debug/bin/jlu_im.exe'), '--empty'], input=script.encode('utf-8'),
                            capture_output=True, timeout=40, env=env)
    output = result.stdout.decode('utf-8').replace('\r\n', '\n')
    (destination / (name + '.txt')).write_text(output, encoding='utf-8')
    assert result.returncode == 0, result.stderr
    for text in required:
        assert text in output, (name, 'missing', text)
    for text in (*forbidden, 'DemoOnly123!', '>>>按任意键继续'):
        assert text not in output, (name, 'unexpected', text)
    print('PASS', name)
    return output


# 注册成功不自动登录；保留程序发出的服务号码用于后续输入。
def register(name):
    return f'2\n1\n{name}\n2000-01-01\n长春\n{name}QQ\nDemoOnly123!\nDemoOnly123!\n'


def login(number, service=1):
    return f'1\n{service}\n{number}\nDemoOnly123!\n'


setup = register('测试甲') + register('测试乙')
setup += login('10001') + '3\n3\n甲微博\n8\n3\n10002\n1\n0\n0\n'
setup += login('10002') + '3\n3\n乙微博\n8\n9\n10001\n1\n0\n0\n'
script = setup + login('10001',3) + '8\n7\n1\n8\n1\n1\n0\n8\n1\n1\n1\n1\n0\n0\n'
script += login('10002',3) + '8\n9\n10001\n1\n1\n0\n0\n'
script += login('10001',3) + '8\n7\n1\n8\n1\n0\n0\n0\n'
full=run('01_推荐申请与同意',script,
    ['跨服务共同好友：0 人','可推荐好友：1 人','已取消推荐添加','推荐申请已发送',
     '已同意申请','跨服务共同好友：1 人','可推荐好友：0 人','程序已退出'],['操作失败'])
(destination/'summary.json').write_text(json.dumps({'result':'PASS','scenarios':['跨服务推荐先申请、收件人同意、再查询'],
    'scope':'实际程序；隔离内存；原映射边界在 C++ 单元测试保留'},ensure_ascii=False,indent=2),encoding='utf-8')
