"""P3 总体验收：实际程序多次重启和两项必要失败检查，只使用 build 内的数据库。"""
from pathlib import Path
import hashlib
import json
import os
import shutil
import sqlite3
import subprocess

root = Path(__file__).resolve().parents[1]
dest = root/'build/debug/p3_acceptance'
dest.mkdir(parents=True, exist_ok=True)
exe = root/'build/debug/bin/jlu_im.exe'
env = dict(os.environ)
env['PATH'] = 'C:/mingw64/bin;' + env.get('PATH', '')
formal = root/'data/jlu_im.db'
formal_before = hashlib.sha256(formal.read_bytes()).hexdigest() if formal.exists() else None
path = dest/'完整重启验收.db'
path.unlink(missing_ok=True)
results, checkpoints = [], []
tables = ['persons','service_accounts','credentials','account_bindings','friendships',
          'chat_groups','group_members','group_requests','id_state','friend_requests']

def rows(database, sql):
    with sqlite3.connect(database) as db:
        return db.execute(sql).fetchall()

def snapshot(database):
    return {table:rows(database, 'SELECT * FROM '+table+' ORDER BY 1,2') for table in tables}

def checkpoint(name):
    assert not rows(path, 'PRAGMA foreign_key_check')
    checkpoints.append({'name':name, 'counts':{table:rows(path,'SELECT count(*) FROM '+table)[0][0] for table in tables},
                        'id_state':rows(path,'SELECT * FROM id_state')[0],
                        'requests':rows(path,'SELECT request_id,group_number,type,state FROM group_requests ORDER BY request_id')})

def run(name, script, database=path, code=0, failures=0):
    (dest/(name+'_输入.txt')).write_text(script, encoding='utf-8')
    p = subprocess.run([str(exe),'--database',str(database)], input=script.encode('utf-8'),
                       cwd=exe.parent, env=env, capture_output=True, timeout=25)
    output = (p.stdout+p.stderr).decode('utf-8').replace('\r\n','\n')
    (dest/(name+'_输出.txt')).write_text(output, encoding='utf-8')
    assert p.returncode == code, (name,p.returncode,output)
    assert output.count('操作失败：') == failures, (name,output)
    results.append({'name':name,'returncode':p.returncode,'expected_returncode':code})
    return output

def login(service, number, password='DemoOnly123!'):
    return f'1\n{service}\n{number}\n{password}\n'

# 第一次运行：注册、开通与绑定、资料和独立备注、子群、申请和邀请都走菜单。
script = '2\n1\n验收同学\n2001-02-03\n长春\n验收QQ\nTestPass123!\nTestPass123!\n'
script += login(1,'10006','TestPass123!')
script += '3\n2\n验收微信\n3\n3\n验收微博\n4\n2\n1\n7\n5\n验收新姓名\n2001-02-03\n吉林\n6\n微信新昵称\n0\n'
script += login(1,'10001')+'8\n3\n10006\n1\n0\n0\n'
script += login(1,'10006','TestPass123!')+'8\n9\n10001\n1\n0\n0\n'
script += login(1,'10001')+"8\n4\n10006\n新同学 O'Brien\n0\n"
script += '9\n1001\n11\n2001\n验收子群\n10001\n10002\n10003\n0\n1\n0\n0\n'
script += login(1,'10006','TestPass123!')+'8\n4\n10001\n介绍人甲\n0\n0\n'
script += login(1,'10003')+'9\n1001\n7\n1\n1001\n3\n1\n0\n0\n'
script += login(1,'10004')+'9\n2001\n3\n1\n0\n0\n'
script += login(2,'wx_20004')+'9\n1001\n7\n1\n0\n0\n'
script += login(2,'wx_20001')+'9\n1001\n4\nwx_20004\n1\n0\n0\n4\n0\n'
out = run('01_完整保存',script)
assert '微信与 QQ 绑定成功' in out and '子群已创建' in out and out.count('保存成功') == 2
assert rows(path,'SELECT count(*) FROM persons') == [(7,)]
assert rows(path,'SELECT count(*) FROM service_accounts') == [(15,)]
assert rows(path,'SELECT count(*) FROM account_bindings') == [(4,)]
assert rows(path,'SELECT first_remark,second_remark FROM friendships WHERE second_remark=\'介绍人甲\'') == [("新同学 O'Brien",'介绍人甲')]
assert rows(path,'SELECT request_id,state FROM group_requests ORDER BY request_id') == [(1,0),(2,0),(3,0)]
assert rows(path,'SELECT group_id FROM chat_groups WHERE parent_group_id IS NOT NULL') == [(19,)]
checkpoint('首次完整保存')

# 第二次运行：登录状态重建，恢复后的请求能继续处理，切换主群保留子群及原请求。
script = login(2,'wx_20005','TestPass123!')+'1\n2\n4\n1\n1\n8\n1\n0\n0\n'
script += login(1,'10001')+'8\n1\n0\n9\n2001\n2\n5\n6\n2\n1\n1\n1001\n6\n1\n1\n13\n2\n1\n12\n0\n10\n0\n'
script += login(2,'wx_20004')+'9\n1001\n5\n6\n3\n1\n2\n0\n0\n0\n'
out = run('02_重启审批与保留',script)
for marker in ['请重新登录','姓名：验收新姓名','所在地：吉林','用户昵称：微信新昵称','微信绑定 QQ：10006',
               'QQ：待确认登录','微博：待确认登录','我的备注：介绍人甲',"我的备注：新同学 O'Brien",'子群号：2001']:
    assert marker in out,marker
assert out.count('已同意，对方已入群。') == 3
assert rows(path,'SELECT group_id FROM chat_groups WHERE parent_group_id IS NOT NULL') == [(19,)]
assert rows(path,'SELECT count(*) FROM group_members WHERE group_id=19') == [(3,)]
assert rows(path,'SELECT request_id,state FROM group_requests ORDER BY request_id') == [(1,1),(2,1),(3,1)]
assert rows(path,'SELECT style FROM chat_groups WHERE service=0 AND number=1001') == [(1,)]
assert rows(path,'SELECT last_group_id,last_group_request_id FROM id_state') == [(19,3)]
checkpoint('审批后保留子群')

# 第三次运行：已有子群继续保留，切回后新建第二子群，所有新编号从保存计数器继续。
script = login(1,'10001')+'9\n1001\n2\n12\n5\n13\n1\n1\n12\n11\n2002\n新增子群\n10001\n10002\n0\n1\n0\n0\n'
script += login(1,'10004')+'9\n2002\n3\n1\n0\n0\n'
script += '2\n1\n下一位同学\n2002-03-04\n长春\n下一位QQ\nNextPass123!\nNextPass123!\n'
script += login(1,'10007','NextPass123!')+'3\n2\n下一位微信\n3\n3\n下一位微博\n0\n0\n'
out = run('03_保留后重启与新编号',script)
assert out.count('子群号：2001') >= 2 and '当前群管理模式：微信' in out
assert '请求编号：4' in out and '注册成功。QQ 登录号码：10007' in out
assert rows(path,'SELECT group_id FROM chat_groups WHERE parent_group_id IS NOT NULL') == [(19,),(20,)]
assert rows(path,'SELECT last_person_id,last_account_id,last_shared_number,last_wechat_number,last_group_id,last_group_request_id FROM id_state') == [(8,18,10007,20006,20,4)]
checkpoint('保留原编号并继续分配')

# 第四次运行只查询，九表记录不应变化；新注册账号、管理员与新请求可恢复。
saved = snapshot(path)
script = login(1,'10007','NextPass123!')+'1\n2\n0\n'
script += login(1,'10001')+'9\n1001\n2\n1\n2002\n2\n5\n0\n0\n0\n'
out = run('04_最终重启查询',script)
for marker in ['姓名：下一位同学','微信：待确认登录 | 号码：wx_20006','微博：待确认登录 | 号码：10007',
               '身份：管理员','群名：新增子群','请求编号：4','待处理']:
    assert marker in out,marker
assert snapshot(path) == saved
checkpoint('最终查询无数据变化')

# 一次读取失败：破坏隔离副本中的绑定归属，加载失败后九表记录保持原样。
bad = dest/'读取失败副本.db'; shutil.copyfile(path,bad)
with sqlite3.connect(bad) as db:
    db.execute("UPDATE account_bindings SET qq_account_id=(SELECT account_id FROM service_accounts WHERE service=0 AND external_id='10007') WHERE wechat_account_id=(SELECT account_id FROM service_accounts WHERE service=1 AND external_id='wx_20005')")
damaged = snapshot(bad)
out = run('05_读取失败不覆盖','0\n',bad,code=1)
assert '数据库绑定必须属于同一人' in out and '主菜单' not in out and snapshot(bad) == damaged

# 一次保存中途失败：有完整数据的副本在写请求时失败，已经重写的表也必须回滚。
failed = dest/'保存失败副本.db'; shutil.copyfile(path,failed)
with sqlite3.connect(failed) as db:
    db.execute("CREATE TRIGGER fail_save BEFORE INSERT ON group_requests BEGIN SELECT RAISE(ABORT,'forced final save failure'); END")
original = snapshot(failed)
script = login(1,'10001')+'6\n这次不应落盘\n10\n0\n0\n2\n'
out = run('06_保存回滚与明确放弃',script,failed)
assert out.count('保存失败：') == 2 and '已放弃未保存的修改' in out and '保存成功' not in out
assert snapshot(failed) == original
formal_after = hashlib.sha256(formal.read_bytes()).hexdigest() if formal.exists() else None
assert formal_before == formal_after,'Formal database changed during tests.'
report = {'date':'2026-10-12','results':results,'checkpoints':checkpoints,'ten_table_final_roundtrip':'PASS',
          'load_failure_preserves_rows':'PASS','late_save_failure_rolls_back_all_tables':'PASS',
          'formal_database_unchanged':True,'formal_database_exists':formal.exists(),
          'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()}
(dest/'persistence_summary.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('PASS 4 real restart stages, binding and remarks, group preservation, continued IDs, 2 failure scenarios; formal database unchanged.')
