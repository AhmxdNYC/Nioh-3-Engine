import os, sys, time, re
G = 'C:/Program Files (x86)/Steam/steamapps/common/Nioh3/'
LOG, CMD = G + 'Nioh3Training.log', G + 'Nioh3Training.cmd'
todo = [l.split('\t') for l in open('C:/Users/User/projects/nioh3-engine/captures/pc2-24140/todo.tsv', encoding='utf-8').read().splitlines()]
out = open('C:/Users/User/projects/nioh3-engine/captures/pc2-24140/drive.out', 'a', encoding='utf-8')
def size(): return os.path.getsize(LOG)
for chr_, name in todo:
    pos = size()
    with open(CMD, 'a') as f: f.write('summon %s\r\n' % chr_)
    t0 = time.time(); res = None
    while time.time() - t0 < 75:
        time.sleep(2)
        with open(LOG, 'rb') as f:
            f.seek(pos); txt = f.read().decode('utf-8', 'replace')
        m = re.search(r'AUTOBIND spawn #d+ chr %s: (FINAL.*)' % chr_, txt)
        if m: res = m.group(1)[:200]; break
    out.write('%s\t%s\t%s\n' % (chr_, name, res)); out.flush()
    time.sleep(3)
out.write('DONE\n'); out.flush()
