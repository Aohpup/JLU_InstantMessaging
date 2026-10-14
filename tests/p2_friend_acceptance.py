"""通过实际程序验证第一批好友菜单，测试账号均为虚构。"""
import argparse
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parents[1])
parser.add_argument('--output', type=Path)
args = parser.parse_args()
root = args.project.resolve()
destination = args.output or root / 'build/debug/p2_batch1_acceptance'
destination.mkdir(parents=True, exist_ok=True)
env = dict(os.environ)
env['PATH'] = 'C:/mingw64/bin;' + env.get('PATH', '')
script = (
    '2\n1\n测试甲\n2000-01-01\n长春\n甲昵称\nDemoOnly123!\nDemoOnly123!\n'
    '2\n1\n测试乙\n2001-01-01\n吉林\n乙昵称\nDemoOnly123!\nDemoOnly123!\n'
    '1\n1\n10001\nDemoOnly123!\n8\n'
    '3\n10002\n0\n1\n3\n10002\n1\n0\n0\n'
    '1\n1\n10002\nDemoOnly123!\n8\n9\n10001\n1\n0\n0\n'
    '1\n1\n10001\nDemoOnly123!\n8\n4\n10002\n甲眼中的乙\n2\n10002\n0\n0\n'
    '1\n1\n10002\nDemoOnly123!\n8\n1\n4\n10001\n乙眼中的甲\n2\n10001\n0\n0\n'
    '1\n1\n10001\nDemoOnly123!\n8\n1\n5\n10002\n0\n1\n5\n10002\n1\n1\n0\n0\n'
    '1\n1\n10002\nDemoOnly123!\n8\n1\n3\n10001\n1\n1\n0\n0\n'
    '1\n1\n10001\nDemoOnly123!\n8\n9\n10002\n1\n1\n0\n0\n0\n'
)
result = subprocess.run([str(root / 'build/debug/bin/jlu_im.exe'), '--empty'], input=script.encode('utf-8'),
                        capture_output=True, timeout=20, env=env)
output = result.stdout.decode('utf-8').replace('\r\n', '\n')
assert result.returncode == 0, result.stderr
for expected in ['已取消添加好友', '已取消删除好友', '好友已删除',
                 '好友号码：10002', '姓名：测试乙', '所在地：吉林',
                 '好友号码：10001', '我的备注：甲眼中的乙', '我的备注：乙眼中的甲',
                 '号码：10001 | 用户昵称：甲昵称 | 我的备注：未设置',
                 '号码：10002 | 用户昵称：乙昵称 | 我的备注：未设置']:
    assert expected in output, expected
assert output.count('已同意申请，双方已成为好友。') == 2
assert output.count('当前服务暂无好友。') >= 3
assert '操作失败' not in output and 'DemoOnly123!' not in output
(destination / '实际好友菜单流程.txt').write_text(output, encoding='utf-8')
print('PASS actual friend menu: registration, confirmation/cancel, bilateral visibility, independent remarks, profile query, reverse re-add, bilateral deletion and remark cleanup.')
