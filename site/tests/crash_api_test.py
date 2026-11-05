"""Offline checks for site/api/crash.py. usage (from site/): CRASH_INBOX=<tmp dir> py tests/crash_api_test.py <crash record .json>"""
import json,sys,importlib.util,glob,os
spec=importlib.util.spec_from_file_location('c','api/crash.py'); c=importlib.util.module_from_spec(spec); spec.loader.exec_module(c)
rec=json.load(open(sys.argv[1],encoding='utf-8'))
def t(name, body, ip='1.1.1.1', want=None):
    code,p=c.handle(body,ip); print(name, code, p.get('error') or p.get('new')); assert want is None or code==want
b=lambda r: json.dumps(r).encode()
# New fields are optional: previous clients remain accepted.
old = {k:v for k,v in rec.items() if k not in ('registers','stack_top','settings','training_tail')}
c.validate(old)
assert rec['registers']['Rdx'] == '0x123456789abcdef0'
assert len(rec['stack_top']) == 16
assert len(rec['log']['Training']) == 20
assert 'EVENT5 ' in rec['log']['Training'][0]
assert all('HP=125176/194938 END_EVENT' in line for line in rec['log']['Training'])
assert rec['training_tail']['included'] == 20
for field,value in [('stack_top', ['0x0']*17), ('registers', {'Rdx':'not-hex'}), ('settings', {'source':'bad'})]:
    r=dict(rec); r[field]=value; t('invalid '+field,b(r),want=400)
r=dict(rec); r['log']={'Training':['z'*2048]}; t('long Training',b(r),want=400)
r=dict(rec); r['log']={'EngineFix':['z'*241]}; t('long other log',b(r),want=400)
r=json.loads(b(rec)); r['settings']['values'][0]['key']='UserName'; t('private setting',b(r),want=400)
r=json.loads(b(rec)); r['settings']['values'][0]['value']='private-name'; t('nonnumeric setting',b(r),want=400)
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
