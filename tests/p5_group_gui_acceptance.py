import argparse,os,pathlib,socket,subprocess,tempfile,time
p=argparse.ArgumentParser();p.add_argument('--exe',required=True);p.add_argument('--output-dir',required=True);p.add_argument('--service',type=int,default=0);p.add_argument('--group',type=int,default=1001);args=p.parse_args()
root=pathlib.Path(args.output_dir).resolve();root.mkdir(parents=True,exist_ok=True);folder=pathlib.Path(tempfile.mkdtemp(prefix='three_',dir=root))
with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
env=os.environ.copy();env['QT_QPA_PLATFORM']='offscreen';processes=[];logs=[]
def launch(role):
    own=role[-1];log=open(folder/(role+'.log'),'w',encoding='utf-8');logs.append(log)
    proc=subprocess.Popen([args.exe,role,str(args.service),str(folder/(own+'.db')),str(port),str(folder),str(args.group)],env=env,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW);processes.append(proc);return proc
try:
    host=launch('C')
    deadline=time.monotonic()+10
    while not (folder/'C.joined').exists() and host.poll() is None and time.monotonic()<deadline:time.sleep(.05)
    if not (folder/'C.joined').exists():raise RuntimeError('First ordinary member failed to open room')
    launch('A');launch('B')
    for proc in processes:
        if proc.wait(timeout=35)!=0:raise RuntimeError('GUI worker failed')
    for role in ['reloadA','reloadB','reloadC']:
        if launch(role).wait(timeout=12)!=0:raise RuntimeError('History reload failed')
    print('PASS three independent Qt processes, service',args.service,'group',args.group,'evidence',folder)
finally:
    for proc in processes:
        if proc.poll() is None:proc.terminate();proc.wait(timeout=5)
    for log in logs:log.close()
    for log in sorted(folder.glob('*.log')):
        text=log.read_text(encoding='utf-8',errors='replace');print(log.name, text[-4000:])
