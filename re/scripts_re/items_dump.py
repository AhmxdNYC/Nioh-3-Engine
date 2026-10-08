r"""Decode the item master table (archive_03 2E/D4E1DB3B...) -> D:\nioh3-re\items.tsv + notes\items-boss-named.csv. Read-only, stdlib."""
import sys,struct,glob,re,csv,os
sys.stdout.reconfigure(encoding='utf-8')
ROOT=os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
f=glob.glob(ROOT+'/data/extract/lfm/archive_03/2E/D4E1DB3B*')[0]
d=open(f,'rb').read()
n=struct.unpack_from('<I',d,4)[0]
w=struct.unpack_from('<%dI'%(len(d)//4),d)
R=[w[2+104*k:2+104*(k+1)] for k in range(n)]
def load(p):
    T={}
    for l in open(p,encoding='utf8'):
        q=l.rstrip('\n').split('\t')
        if len(q)>=4 and q[1].isdigit():T[int(q[1])]=q[3]
    return T
E_=load('D:/nioh3-re/text_en.tsv');J=load('D:/nioh3-re/text_ja.tsv')
mk=re.compile(r'\^[0-9A-Fa-f]{2}~[^~]*~')
def clean(s):return mk.sub('',s).replace('\n',' ').strip()
LAB={1:'weapon',2:'bow/firearm',3:'armor',4:'consumable',5:'material',6:'key/scroll/box',7:'talisman/ninjutsu',8:'ammo',9:'soul core? (unnamed)',10:'skill',11:'appearance/collection',12:'martial-arts handbook/skill',13:'ninjutsu/onmyo skill',14:'ninja tool',16:'skill (dummy)',17:'unnamed misc',18:'picture scroll',19:'document/note',0:'misc'}
def lab(c):
    if c<300000000: return 'weapon' if c<200000000 else 'bow/firearm' if c<300000000 else '?'
    if c<306000000:
        return {301:'armor:head',302:'armor:body',303:'armor:hands',304:'armor:waist',305:'armor:feet'}[c//1000000]
    if c==306000000:return 'accessory(charm)'
    return LAB.get(c//100000000,'?')
LAB[3]='armor/accessory'
rows=[]
for k,r in enumerate(R):
    c=r[25];nid=r[26];en=clean(E_.get(nid,''));ja=clean(J.get(nid,''))
    dummy=int(nid in(17844063,18269809) or en=='' and ja in('DUMMY',''))
    rows.append((k,c,lab(c),nid,r[27],en,ja,'%08x'%r[84],r[0],r[1],r[33],r[34],r[36],r[37],r[38],r[44],r[64],dummy))
with open('D:/nioh3-re/items.tsv','w',encoding='utf8',newline='') as o:
    o.write('idx\tcat_code\tcategory\tname_id\tdesc_id\tname_en\tname_ja\trec_hash\tkey_off0\tkey_off1\tf33\tprice_f34\tf36\tf37\tf38\tflags_f44\tstack_f64\tdummy\n')
    for x in rows:o.write('\t'.join(str(v).replace('\t',' ') for v in x)+'\n')
# boss match
atoms=set()
for r in csv.DictReader(open(ROOT+'/notes/boss-names-online.csv',encoding='utf8')):
    for p in re.split(r'[;()&]',r['english_name']):
        p=p.strip()
        if len(p)>=4 and p.lower() not in('final','human','monster','mounted','2nd'):atoms.add(p)
        if '-' in p:atoms.add(p.replace('-',' '))
pats=[(a,re.compile(r'(?<![A-Za-z])'+re.escape(a)+r'(?![A-Za-z])',re.I)) for a in sorted(atoms)]
out=[]
for x in rows:
    for a,p in pats:
        if x[5] and p.search(x[5]):out.append((a,)+x[:6]+(x[7],))
with open(ROOT+'/notes/items-boss-named.csv','w',encoding='utf8',newline='') as o:
    cw=csv.writer(o);cw.writerow(['boss_atom','idx','cat_code','category','name_id','desc_id','name_en','rec_hash'])
    cw.writerows(out)
print(len(rows),len(out),len(atoms))
