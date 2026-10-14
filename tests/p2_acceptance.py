"""P2 总体验收入口：复用现有测试，保存命令输出与可执行文件校验值。"""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
dest = root / 'build/debug/p2_acceptance'
dest.mkdir(parents=True, exist_ok=True)
env = dict(os.environ)
env['PATH'] = 'C:/mingw64/bin;' + env.get('PATH', '')
commands = [
    ('build', ['cmake', '--build', '--preset', 'local-debug']),
    ('ctest', ['ctest', '--preset', 'local-debug', '--verbose']),
]
for name in ('p1_acceptance', 'p2_friend_acceptance', 'p2_recommendation_acceptance',
             'p2_group_acceptance', 'p2_group_operations_acceptance'):
    commands.append((name, [sys.executable, '-X', 'utf8', str(root / 'tests' / (name + '.py')),
                            '--output', str(dest / name)]))
for name in ('p2_subgroup_mode_acceptance', 'p1_console_native_test', 'p2_interaction_native_test'):
    commands.append((name, [sys.executable, '-X', 'utf8', str(root / 'tests' / (name + '.py'))]))

results = []
for name, command in commands:
    result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=180)
    log = result.stdout.decode('utf-8', errors='replace') + result.stderr.decode('utf-8', errors='replace')
    (dest / (name + '.txt')).write_text(log, encoding='utf-8')
    results.append({'name': name, 'command': command, 'returncode': result.returncode})
    report = {'results': results}
    exe = root / 'build/debug/bin/jlu_im.exe'
    if exe.exists():
        report['executable_sha256'] = hashlib.sha256(exe.read_bytes()).hexdigest()
    (dest / 'summary.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(('PASS ' if result.returncode == 0 else 'FAIL ') + name, flush=True)
    if result.returncode:
        print(log)
        raise SystemExit(result.returncode)
print('P2 overall acceptance passed.')
