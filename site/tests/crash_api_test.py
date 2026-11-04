"""Offline checks for site/api/crash.py. usage (from site/): CRASH_INBOX=<tmp dir> py tests/crash_api_test.py <crash record .json>"""
import json,sys,importlib.util,glob,os
spec=importlib.util.spec_from_file_location('c','api/crash.py'); c=importlib.util.module_from_spec(spec); spec.loader.exec_module(c)
rec=json.load(open(sys.argv[1],encoding='utf-8'))
def t(name, body, ip='1.1.1.1', want=None):
    code,p=c.handle(body,ip); print(name, code, p.get('error') or p.get('new')); assert want is None or code==want
b=lambda r: json.dumps(r).encode()
t('ok', b(rec), want=202)
t('repeat', b(rec), want=202)
r=dict(rec); r['id']='xyz'; t('bad id', b(r), want=400)
r=dict(rec); r['evil']=1; t('unknown', b(r), want=400)
r=dict(rec); r['fields']=dict(rec['fields'], module=r'C:\x\a.dll'); t('path module', b(r), want=400)
t('big', b'{'+b' '*70000+b'}', want=413)
t('notjson', b'hello', want=400)
r=dict(rec); r['log']={'EngineFix':[r'C:\Users\Bob Smith\x 76561198000000001']}; code,p=c.handle(b(r),'2.2.2.2'); print('scrub',code)
f=sorted(glob.glob(os.path.join(os.environ['CRASH_INBOX'],'*','*.json')),key=os.path.getmtime)[-1]; s=open(f,encoding='utf-8').read(); assert 'Bob' not in s and '7656119800' not in s; print('scrubbed ok')
for i in range(6): code,p=c.handle(b(rec),'3.3.3.%d'%i)
print('id rate', code); assert code==429
print('ALL PASSED')
