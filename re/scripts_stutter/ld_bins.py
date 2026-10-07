# ld_bins.py <mangohud.csv> [t0] [t1] [bin_s]  - per-bin summary of a MangoHud CSV
import sys, csv
p=sys.argv[1]; t0=float(sys.argv[2]) if len(sys.argv)>2 else 0; t1=float(sys.argv[3]) if len(sys.argv)>3 else 1e9
b=float(sys.argv[4]) if len(sys.argv)>4 else 2
rows=list(csv.reader(open(p)))[3:]
h=['fps','frametime','cpu_load','cpu_power','gpu_load','cpu_temp','gpu_temp','gpu_core_clock','gpu_mem_clock','gpu_vram_used','gpu_power','ram_used','swap_used','process_rss','cpu_mhz','elapsed']
bins={}
for r in rows:
    d=dict(zip(h,map(float,r))); t=d['elapsed']/1e9
    if t<t0 or t>t1: continue
    bins.setdefault(int(t//b),[]).append(d)
print("t     n  ftavg ftmax cpu% cpuW gpu% gMHz memMHz gpuW  vramMB ramMB swapMB")
for k in sorted(bins):
    L=bins[k]; ft=[x['frametime'] for x in L]
    a=lambda f: sum(x[f] for x in L)/len(L)
    print(f"{k*b:5.0f} {len(L):3d} {sum(ft)/len(ft):6.1f} {max(ft):6.1f} {a('cpu_load'):4.0f} {a('cpu_power'):4.1f} {a('gpu_load'):4.0f} {a('gpu_core_clock'):5.0f} {a('gpu_mem_clock'):5.0f} {a('gpu_power'):4.1f} {a('gpu_vram_used')*1024:6.0f} {a('ram_used')*1024:6.0f} {a('swap_used')*1024:6.0f}")
