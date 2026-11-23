"""Human #121968: a won in-place Training session ends once the dead boss body is removed, so the menu returns to the first page and
'Train here' / 'Train in a boss arena' come back. Arena sessions, pending summons and live/native bosses keep the session."""
from pathlib import Path
import re
s=(Path(__file__).resolve().parents[1]/'src/training.c').read_text()
m=re.search(r'static int TrainWonEndsSession\(int active, int arena, int won, int bodyGone, int spawnPending, int slotAlive, int nativeSlots\) \{\s*return ([^;]+);',s)
assert m,'rule missing'
f=eval('lambda active,arena,won,bodyGone,spawnPending,slotAlive,nativeSlots: '+m.group(1).replace('&&',' and ').replace('!',' not '))
assert f(1,0,1,1,0,0,0)
for k in range(7):
    a=[1,0,1,1,0,0,0]; a[k]^=1
    assert not f(*a),('must keep session',k)
assert 'TrainStop("boss defeated, body removed")' in s
assert s.count('TrainWonBodyGone();')==2,'both body-removal paths end the session'
assert 'deleting the faded boss body", g_tr.round); Despawn(g_nsp - 1); } TrainWonBodyGone(); }' in s
assert 'else { g_tr.bodyGone = 1; Despawn(g_nsp - 1); TrainWonBodyGone(); }' in s
assert s.index('static void TrainStop(const char* why) {')<s.index('static void TrainWonBodyGone(void)')
print('kill_ends_session_test PASS')
