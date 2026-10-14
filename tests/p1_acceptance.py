"""通过实际可执行文件验收 P1 菜单，并检查源码组织；仅使用虚构账号。"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parents[1])
parser.add_argument('--output', type=Path)
args = parser.parse_args()
root = args.project.resolve()
destination = args.output or root / 'build/debug/acceptance'
destination.mkdir(parents=True, exist_ok=True)
exe = root / 'build/debug/bin/jlu_im.exe'
env = dict(os.environ)
env['PATH'] = 'C:/mingw64/bin;' + env.get('PATH', '')
results = []

def run(name, script, required, forbidden=('操作失败',)):
    completed = subprocess.run([str(exe), '--empty'], input=script.encode('utf-8'), capture_output=True,
                               env=env, timeout=20)
    output = completed.stdout.decode('utf-8').replace('\r\n', '\n')
    (destination / (name + '.txt')).write_text(output, encoding='utf-8')
    assert completed.returncode == 0, (name, completed.stderr)
    for value in required:
        assert value in output, (name, 'missing', value)
    for value in (*forbidden, 'DemoOnly123!', '>>>按任意键继续'):
        assert value not in output, (name, 'unexpected', value)
    results.append({'scenario': name, 'result': 'PASS'})
    print('PASS', name)
    return output

# 验证用户报告的分次登录路径，并串联三个服务及资料修改。
full = run('01_qq_wechat_independent_login',
    '2\n1\n测试甲\n11wds\n2000-02-29\n长春\nQQ昵称\nshort\nDemoOnly123!\nDemoOnly123!\n'
    '1\n1\n10001\nDemoOnly123!\n'
    '3\n2\n微信昵称\n3\n3\n微博昵称\n0\n'
    '1\n2\nwx_20001\nDemoOnly123!\n7\n2\n'
    '5\n测试甲新\n2000-02-29\n吉林\n6\n微信新昵称\n1\n'
    '4\n3\n0\n4\n3\n1\n1\n0\n0\n',
    ['请重新输入生日', '请重新输入密码', '注册成功', '微信与 QQ 绑定成功',
     'QQ：待确认登录 | 号码：10001', '微博：待确认登录 | 号码：10001',
     '微信绑定 QQ：10001', '姓名：测试甲新', '所在地：吉林', '用户昵称：微信新昵称',
     '已取消确认', '已进入微博', '注册年龄：0 年', '已统一注销全部服务'])

# 微信先注册，再开通 QQ，分别登录后回到微信仍可绑定。
run('02_wechat_first_return_and_bind',
    '2\n2\n测试乙\n2001-01-01\n吉林\nWX乙\nDemoOnly123!\nDemoOnly123!\n'
    '1\n2\nwx_20001\nDemoOnly123!\n3\n1\nQQ乙\n0\n'
    '1\n1\n10001\nDemoOnly123!\n0\n'
    '1\n2\nwx_20001\nDemoOnly123!\n7\n2\n0\n0\n',
    ['注册成功。微信 登录号码：wx_20001', '微信与 QQ 绑定成功', '微信绑定 QQ：10001'])

# 尚未开通、尚未验证、错误密码与重复绑定都不得被错误放行。
negative = run('03_rejections_and_current_session_binding',
    '2\n1\n测试丙\n2001-01-01\n吉林\nQQ丙\nDemoOnly123!\nDemoOnly123!\n'
    '1\n1\n10001\nWrong1234\n1\n1\n10001\nDemoOnly123!\n'
    '7\n4\n2\n3\n2\nWX丙\n7\n4\n2\n0\n7\n4\n2\n1\n7\n7\n0\n0\n',
    ['账号或密码错误', '请先开通本人的 QQ 和微信', '尚未开通', '请先分别用密码登录',
     '已取消确认', '微信与 QQ 绑定成功', '微信账号已绑定'], forbidden=())
assert negative.count('微信与 QQ 绑定成功') == 1

# 新进程不保留账号，确认目前确实为内存版本。
run('04_restart_is_empty', '1\n1\n10001\nDemoOnly123!\n0\n',
    ['账号或密码错误', '程序已退出'], forbidden=('登录成功',))

# 检查正式代码中的类命名、独立文件、构建收录及公共接口注释。
classes = []
enums = []
for header in (root / 'src').rglob('*.h'):
    text = header.read_text(encoding='utf-8-sig')
    for name in re.findall(r'^class (\w+)(?:\s+final)?\s*(?::[^\n]+)?\s*\n\{', text, re.M):
        assert name.endswith('LJY') and header.stem == name, header
        assert header.with_suffix('.cpp').exists(), header
        classes.append(name)
    for name in re.findall(r'^enum class (\w+)', text, re.M):
        assert name.endswith('LJY') and header.stem == name, header
        enums.append(name)
    assert '//' in text, header
cmake = (root / 'CMakeLists.txt').read_text(encoding='utf-8-sig')
for source in (root / 'src').rglob('*.cpp'):
    assert source.relative_to(root).as_posix() in cmake, source
    assert '<cstdint>' not in source.read_text(encoding='utf-8-sig'), source
# 新阶段可增加类型，继续检查原有基线及每个新增类的命名和文件组织。
assert len(classes) >= 12 and len(enums) >= 4, (classes, enums)
results.append({'scenario': 'source_structure', 'result': 'PASS', 'classes': classes, 'enums': enums})
report = {'executable_sha256': hashlib.sha256(exe.read_bytes()).hexdigest(), 'results': results}
(destination / 'summary.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(f'PASS source structure: {len(classes)} independent classes, {len(enums)} enum classes, LJY suffixes, explicit CMake sources.')
