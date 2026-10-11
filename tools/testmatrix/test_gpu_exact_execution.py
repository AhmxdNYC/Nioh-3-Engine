"""Guard native triage against unrelated/new or missing PSO evidence."""
from pc_gpu_report import exact_compute_ran, EXACT_COMPUTE_PROOF

checks=0
def check(value):
    global checks
    checks+=1
    assert value,checks

for item,digest in EXACT_COMPUTE_PROOF.items():
    good={'gp2':{'rows':{'cs='+digest:0.1}}}
    other={'gp2':{'rows':{'cs='+'f'*32:0.1}}}
    check(exact_compute_ran(item,[good]))
    check(not exact_compute_ran(item,[other]))
    check(not exact_compute_ran(item,[good,other]))
    check(not exact_compute_ran(item,[good,{'gp2':None}]))
    check(not exact_compute_ran(item,[]))
    check(not exact_compute_ran(item,[{'gp2':{'rows':{'ps='+digest:0.1}}}]))
    check(not exact_compute_ran(item,[{'gp2':{'rows':{'cs='+digest+'bad':0.1}}}]))
    check(exact_compute_ran(item,[{'gp2':{'rows':{'vs=123 cs='+digest+' [marker]':0.1}}}]))
print(checks,'exact replacement execution checks PASS')
