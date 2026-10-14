"""P3 第四批：用独立进程验证启动、保存和隔离；只操作 build 内的验证库。"""
from pathlib import Path
import hashlib
import json
import os
import sqlite3
import subprocess

root = Path(__file__).resolve().parents[1]
dest = root/'build/debug/p3_batch4'
dest.mkdir(parents=True, exist_ok=True)
exe = root/'build/debug/bin/jlu_im.exe'
env = dict(os.environ)
env['PATH'] = 'C:/mingw64/bin;' + env.get('PATH', '')
formal = root/'data/jlu_im.db'
before = formal.read_bytes() if formal.exists() else None
results = []

def run(name, args, script='', cwd=root, code=0):
    p = subprocess.run([str(exe), *map(str, args)], input=script.encode('utf-8'), cwd=cwd,
                       env=env, capture_output=True, timeout=20)
    output = (p.stdout + p.stderr).decode('utf-8')
    (dest/(name+'.txt')).write_text(output, encoding='utf-8')
    assert p.returncode == code, (name, p.returncode, output)
    results.append({'name': name, 'returncode': p.returncode, 'expected_returncode': code})
    return output

def fresh(name):
    path = dest/name
    path.unlink(missing_ok=True)
    return path

def scalar(path, sql):
    with sqlite3.connect(path) as db:
        return db.execute(sql).fetchone()[0]

# 默认路径不随工作目录变化；帮助只显示路径，不创建或加载正式库。
for i, cwd in enumerate([root, exe.parent, dest]):
    out = run(f'help_{i}', ['--help'], cwd=cwd)
    assert '默认数据库：'+formal.as_posix() in out

path = fresh('中文 保存测试.db')
out = run('first_run', ['--database', path],
          '1\n1\n10001\nDemoOnly123!\n6\n持久化昵称\n10\n0\n0\n', cwd=exe.parent)
assert '新数据库已创建' in out and out.count('保存成功') == 2 and '退出后清空' not in out
assert scalar(path, "SELECT nickname FROM service_accounts WHERE service=0 AND external_id='10001'") == '持久化昵称'
out = run('restart', ['--database', path], '1\n1\n10001\nDemoOnly123!\n1\n0\n0\n', cwd=dest)
assert '用户昵称：持久化昵称' in out and '请重新登录' in out and '新数据库已创建' not in out
assert scalar(path, 'SELECT count(*) FROM persons') == 6
out = run('register_exit', ['--database', path],
          '2\n1\n新注册同学\n2001-02-03\n长春\n新用户\nTestPass123!\nTestPass123!\n0\n')
assert '注册成功' in out and scalar(path, 'SELECT count(*) FROM persons') == 7
out = run('registered_restart', ['--database', path], '1\n1\n10006\nTestPass123!\n1\n0\n0\n')
assert '姓名：新注册同学' in out

# 已有合法空库不再注入演示数据。
with sqlite3.connect(path) as db:
    for table in ['group_requests','group_members','chat_groups','friendships','account_bindings','credentials','service_accounts','persons']:
        db.execute('DELETE FROM '+table)
    db.execute('UPDATE id_state SET last_person_id=0,last_account_id=0,last_shared_number=10000,last_wechat_number=20000,last_group_id=0,last_group_request_id=0')
out = run('existing_empty', ['--database', path], '0\n')
assert '0 名人员，0 个账号，0 个群' in out and scalar(path, 'SELECT count(*) FROM persons') == 0
unchanged = path.read_bytes()
out = run('demo_isolated', ['--demo'], '0\n', cwd=dest)
assert '已加载课堂演示数据' in out and '内存测试模式' in out and '保存成功' not in out
out = run('empty_isolated', ['--empty'], '0\n', cwd=dest)
assert '已加载课堂演示数据' not in out and '内存测试模式' in out
assert path.read_bytes() == unchanged

# 损坏文件在启动时报错；没有回退到空平台并覆盖的行为。
bad = fresh('invalid.db'); bad.write_bytes(b'not a database')
out = run('read_failure', ['--database', bad], '0\n', code=1)
assert '程序启动失败' in out and bad.read_bytes() == b'not a database'

# 真实 SQL 写入失败后，菜单返回/手动保存/明确放弃和 EOF 均给出正确结果。
with sqlite3.connect(path) as db:
    db.execute("CREATE TRIGGER fail_save BEFORE DELETE ON id_state BEGIN SELECT RAISE(ABORT,'test save failure'); END")
out = run('save_failure_discard', ['--database', path], '0\n0\n4\n0\n2\n')
assert out.count('保存失败') == 3 and '已放弃未保存的修改' in out and '保存成功' not in out
out = run('save_failure_eof', ['--database', path], '', code=1)
assert '本次修改未保存，程序异常退出' in out
assert scalar(path, 'SELECT last_shared_number FROM id_state') == 10000
with sqlite3.connect(path) as db:
    db.execute('DROP TRIGGER fail_save')

after = formal.read_bytes() if formal.exists() else None
assert before == after, 'Tests changed the formal database.'
summary = {'results':results, 'formal_database_unchanged':True,
           'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()}
(dest/'startup_summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf-8')
print(f'PASS {len(results)} process runs: startup, restart, Unicode paths, isolation and save failure handling.')
