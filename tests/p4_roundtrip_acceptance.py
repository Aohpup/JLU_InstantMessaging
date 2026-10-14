"""P4 双入口验收：真实 GUI 控件 → 真实终端菜单 → 新 GUI 进程，只使用隔离库。"""
from pathlib import Path
import hashlib
import json
import os
import sqlite3
import subprocess

root = Path(__file__).resolve().parents[1]
folder = root / 'build/p4_final'
folder.mkdir(parents=True, exist_ok=True)
path = folder / '双入口往返.db'
path.unlink(missing_ok=True)
formal = root / 'data/jlu_im.db'
def digest(file):
    return hashlib.sha256(file.read_bytes()).hexdigest() if file.exists() else None
before = digest(formal)
env = dict(os.environ)
env['PATH'] = 'D:/Qt/Tools/mingw1310_64/bin;D:/Qt/6.8.3/mingw_64/bin;' + env.get('PATH', '')
env['QT_QPA_PLATFORM_PLUGIN_PATH'] = 'D:/Qt/6.8.3/mingw_64/plugins/platforms'
gui = root / 'build/gui-debug/bin/p4_roundtrip_gui_tests.exe'
results = []
def run(name, args, script=None, environment=env):
    result = subprocess.run(list(map(str, args)), input=script.encode('utf-8') if script else None,
                            cwd=folder, env=environment, capture_output=True, timeout=30)
    output = (result.stdout + result.stderr).decode('utf-8', errors='replace')
    (folder / (name + '.log')).write_text(output, encoding='utf-8')
    assert result.returncode == 0, (name, result.returncode, output)
    results.append({'step': name, 'returncode': result.returncode})
    return output

def snapshot():
    with sqlite3.connect(path) as db:
        assert not db.execute('PRAGMA foreign_key_check').fetchall()
        return {name: db.execute('SELECT * FROM ' + name + ' ORDER BY 1,2').fetchall()
                for (name,) in db.execute("SELECT name FROM sqlite_master WHERE type='table'").fetchall()}

run('01_GUI写入', [gui, '-platform', 'offscreen', '--write', path])
with sqlite3.connect(path) as db:
    assert db.execute('SELECT state FROM friend_requests').fetchall() == [(0,)]
    assert db.execute('SELECT style FROM chat_groups WHERE service=0 AND number=1001').fetchone() == (1,)
    assert db.execute('SELECT location,avatar_color FROM persons WHERE person_id=1').fetchone() == ('图形端城市',4)

script = '1\n1\n10005\nDemoOnly123!\n8\n9\n10001\n1\n0\n0\n'
script += '1\n1\n10001\nRoundtrip123!\n1\n2\n8\n1\n0\n9\n1001\n2\n0\n'
script += '5\n双入口姓名\n2000-01-01\n终端城市\n6\n终端昵称\n8\n4\n10002\n终端备注\n0\n'
script += '9\n1001\n13\n1\n1\n0\n10\n0\n0\n'
(folder / '02_终端输入.txt').write_text(script, encoding='utf-8')
terminal_env = dict(os.environ)
terminal_env['PATH'] = 'C:/mingw64/bin;' + terminal_env.get('PATH', '')
output = run('02_终端读写', [root / 'build/debug/bin/jlu_im.exe', '--database', path], script, terminal_env)
assert '操作失败：' not in output
for marker in ['所在地：图形端城市', '我的备注：图形端备注', '微信绑定 QQ：10001',
               '当前群管理模式：微信', '已同意申请，双方已成为好友。', '群管理模式已切换为QQ', '保存成功']:
    assert marker in output, marker
with sqlite3.connect(path) as db:
    assert db.execute('SELECT state FROM friend_requests').fetchall() == [(1,)]
    assert db.execute('SELECT location,avatar_color FROM persons WHERE person_id=1').fetchone() == ('终端城市',4)
saved = snapshot()
run('03_GUI重新读取', [gui, '-platform', 'offscreen', '--read', path])
assert snapshot() == saved, 'GUI reload/save changed persisted rows'
assert before == digest(formal), 'formal database changed'
report = {'date':'2026-10-12', 'result':'PASS', 'processes':results,
          'coverage':['资料昵称头像','新密码登录','申请审批与好友备注','群模式','QQ微信绑定','十表再保存一致'],
          'table_count':len(saved), 'formal_database_unchanged':True}
(folder / 'roundtrip.json').write_text(json.dumps(report,ensure_ascii=False,indent=2), encoding='utf-8')
print('PASS GUI -> actual console -> fresh GUI, ten-table snapshot unchanged after reload/save.')
