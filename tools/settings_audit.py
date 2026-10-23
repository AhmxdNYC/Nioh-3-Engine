"""Offline single-setting audit planner. Writes copies only; never runs a game or edits an installation."""
from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
import hashlib
import json
import math
import statistics
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INI = ROOT / 'mod/Nioh3EngineFix/Nioh3EngineFix.ini'
DEVICES = {'pc': 'Windows / RX 9070', 'pc2': 'Windows / RX 6650 XT', 'ally': 'Linux / Proton / ROG Ally'}
PATCH_KEYS = '''MW RD WorkerCap AS0 AS L1OnWine FGOnWine FGC P12 B PP FPC AABB M1 M2 M2S
J1 X1 E1 S1 IK ED WG TG DS DQ P1E7 LK MA BPO W1B RPA CP SPF PT1 DL RWb ESM1 MPCS TR RTP
PCM CBS CBF VSMA LPVR SKYR FOGT CLDR PJ PJH PL GL1 LC LW LoadOnlyCpuFixes RC AQ TSY SH PF GIMerge'''.split()
GPU_KEYS = 'Barriers FrameOverlap TileBatch DropUndoPairs R2a R2b R1 AnyCaller CbvMirror CbvMirrorAllSites SceneBufMirror'.split()
EXTRA_KEYS = 'XCLD XFOG XGI XSHD XSHF XLOD XTER XPRT XAO XPOST'.split()
AUTO_KEYS = {('Patches', k) for k in ('W6', 'GF', 'MQ', 'EP', 'SR')} | {('Latency', 'MaxFrameLatency')}
CANDIDATES = ({('Patches', k) for k in PATCH_KEYS} | {('GpuOpt', k) for k in GPU_KEYS}
              | {('ExtraPerf', k) for k in EXTRA_KEYS} | AUTO_KEYS
              | {('VRR', 'MenuQuantize'), ('Latency', 'VrrPresent'), ('Tuning', 'KL_Div'),
                 ('Memory', 'LM'), ('Debug', 'SkipDebugString')})
DIAGNOSTIC_PATCHES = {'TS', 'WakeStats', 'CpuStats', 'SPR', 'JT'}
PATCH_PARAMETERS = {'PJ_MaxMB', 'PJ_QuietMs', 'PJ_KeepMax', 'PL_MaxMB', 'RCMB', 'RCKB',
                    'RCMaxReadKB', 'AQThreads', 'PFThreads', 'PFLeadMB', 'PFTraces', 'PFMatchPct', 'GIMergeQualify'}
GPU_PARAMETERS = {'MergeAcrossBinds', 'DropDuplicateUav', 'MaxBatch', 'DeferThreads',
                  'TraceDelaySec', 'InstallDelaySec', 'CbvMirrorMB', 'CbvMirrorSites',
                  'CbvMirrorQueue', 'CbvMirrorComputeCopy', 'SceneBufMirrorQueue'}
ON_VALUES = {('Patches', 'WorkerCap'): '10', ('Patches', 'TSY'): '4',
             ('Tuning', 'KL_Div'): '2', ('Latency', 'MaxFrameLatency'): '2'}
PROTON_ONLY = {('Patches', k) for k in ('L1OnWine', 'FGOnWine', 'SH', 'SR')} | {('GpuOpt', k) for k in ('R2a', 'R2b', 'R1')}
WINDOWS_ONLY = {('Patches', 'TR'), ('Patches', 'PL'), ('Latency', 'VrrPresent'),
                ('GpuOpt', 'CbvMirror'), ('GpuOpt', 'CbvMirrorAllSites'), ('GpuOpt', 'SceneBufMirror')}
LOAD_KEYS = {'LoadOnlyCpuFixes', 'EP', 'SR', 'RC', 'AQ', 'TSY', 'SH', 'PF', 'LC', 'LW', 'PL'}
VISUAL_KEYS = {'E1', 'TG', 'ESM1', 'VSMA', 'LPVR', 'SKYR', 'FOGT', 'CLDR', 'R2b', 'R1', 'FrameOverlap', 'AS0', 'AS'}


@dataclass(frozen=True)
class Setting:
    section: str
    key: str
    value: str
    line: int

    @property
    def identity(self):
        return self.section, self.key


def parse_ini(text: str) -> list[Setting]:
    result, seen, section = [], set(), ''
    for number, line in enumerate(text.splitlines(), 1):
        line = line.strip().lstrip('\ufeff')
        if not line or line.startswith((';', '#')):
            continue
        if line.startswith('['):
            match = re.fullmatch(r'\[([^]]+)\]\s*(?:[;#].*)?', line)
            if not match:
                raise ValueError(f'Invalid section at line {number}')
            section = match[1]
        elif '=' in line and section:
            key, value = line.split('=', 1)
            key = key.strip()
            identity = section.casefold(), key.casefold()
            if identity in seen:
                raise ValueError(f'Duplicate setting [{section}] {key}')
            seen.add(identity)
            result.append(Setting(section, key, re.split(r'\s+[;#]', value.strip(), maxsplit=1)[0], number))
        else:
            raise ValueError(f'Invalid INI assignment at line {number}')
    return result


def edit_ini(text: str, edits: dict[tuple[str, str], str]) -> str:
    """Preserve comments/other keys and edit only existing section-qualified assignments."""
    settings = {s.identity: s for s in parse_ini(text)}
    lines = text.splitlines(keepends=True)
    for identity, value in edits.items():
        if identity not in settings:
            raise ValueError(f'Missing required setting {identity}')
        setting = settings[identity]
        old = lines[setting.line - 1]
        ending = '\r\n' if old.endswith('\r\n') else '\n' if old.endswith('\n') else ''
        match = re.match(r'([^=]+=\s*)(.*?)(\s+[;#].*)?$', old.rstrip('\r\n'))
        assert match
        lines[setting.line - 1] = match[1] + value + (match[3] or '') + ending
    return ''.join(lines)


def classification(s: Setting) -> str:
    sec, key = s.identity
    if s.identity in CANDIDATES:
        if s.value == '0':
            return 'default-off optimization'
        if s.identity in AUTO_KEYS and s.value == '-1':
            return 'auto-default / latency probe'
        return 'optimization already enabled or non-default'
    if sec in ('Profiler', 'ShaderHot') or (sec == 'Patches' and key in DIAGNOSTIC_PATCHES) or (sec == 'GpuOpt' and key == 'TraceRecords'):
        return 'diagnostic (excluded)'
    if sec == 'ShaderSwap':
        return 'shader replacement infrastructure / diagnostics (not a standalone optimization)'
    if sec == 'Patches':
        if key == 'VRROnline':
            return 'online compatibility policy (excluded)'
        if key in PATCH_PARAMETERS:
            return 'parameter (not an off switch)'
        if s.value in ('0', '-1'):
            raise ValueError(f'Unclassified patch [{sec}] {key}; review new INI key before generating')
        return 'enabled optimization / platform policy'
    if sec == 'GpuOpt':
        if key not in GPU_PARAMETERS:
            raise ValueError(f'Unclassified GPU option {key}; review before generating')
        return 'parameter / mode (zero is not OFF)'
    if sec == 'ExtraPerf' and key not in ('Enabled', 'Preset'):
        raise ValueError(f'Unclassified ExtraPerf key {key}; review before generating')
    if sec in ('Tuning', 'Memory', 'VRR', 'ExtraPerf'):
        return 'parameter / parent switch / safety override (excluded)'
    return 'UI / diagnostic / compatibility / harness control (excluded)'


def catalog(text: str):
    return [dict(section=s.section, key=s.key, value=s.value, line=s.line, classification=classification(s))
            for s in parse_ini(text)]


def recipe(s: Setting, device: str):
    """Pair-specific prerequisites apply identically to BOTH arms, preserving single-key attribution."""
    sec, key = s.identity
    common = {('General', 'Enabled'): '1'}
    notes, status = [], 'planned'
    if (s.identity in PROTON_ONLY and device != 'ally') or (s.identity in WINDOWS_ONLY and device == 'ally'):
        status = 'not_applicable'
        notes.append('Platform gate: do not record this as a measured no-gain result.')
    if s.identity in AUTO_KEYS and key != 'MaxFrameLatency' and device == 'ally':
        status = 'already_auto_on'
        notes.append('Shipped -1 already enables this on Proton; not a default-off treatment on Ally.')
    if key in ('FGC', 'FGOnWine'):
        notes.append('Use the SAME supported in-game frame generation ON setting in both arms; confirm generated frames.')
    if key == 'FGOnWine':
        common[('Patches', 'FG')] = '1'
    if key == 'L1OnWine':
        common[('Patches', 'L1')] = '1'
    if key in ('TG', 'DS'):
        common[('Patches', 'L1')] = '1'
        if device == 'ally':
            common[('Patches', 'L1OnWine')] = '1'
    if key == 'TG':
        common[('Patches', 'VRR')] = '1'
    if key in ('DropUndoPairs', 'AnyCaller') or (sec == 'GpuOpt' and key in ('R1', 'R2a', 'R2b')):
        common[('GpuOpt', 'Barriers')] = '1'
        notes.append('Compare against paired baseline with Barriers=1, not the global baseline.')
    if key == 'CbvMirrorAllSites':
        common[('GpuOpt', 'CbvMirror')] = '2'
        notes.append('Forced CbvMirror=2 in BOTH arms; verify it is active before measuring coverage toggle.')
    if key in ('CbvMirror', 'SceneBufMirror'):
        notes.append('ON=1 is auto; gated off on UMA/Proton or if upload heaps are local. Require actual activation; otherwise NOT_APPLICABLE.')
    if key == 'M2S':
        common[('Patches', 'M2')] = '0'
    if key in ('AS', 'AS0'):
        common[('Patches', 'AS0' if key == 'AS' else 'AS')] = '0'
    if key == 'PJ':
        common[('Patches', 'PJH')] = '0'
    if key in ('PJ', 'PJH', 'PF', 'PL'):
        notes.append('Needs a prepared journal/trace/library; perform unmeasured training launch, then restore identical cache snapshot before each arm. First launch is not evidence.')
    if key == 'PJH':
        common[('Patches', 'PJ')] = '1'
    if key == 'MPCS':
        status = 'requires_fixture'
        notes.append('Requires hash-verified mpc shader package, identical in BOTH arms; reject measurement if patch refuses to activate.')
    if key in ('CBS', 'CBF'):
        common[('GpuOpt', 'CbvMirror')] = '0'
    if key == 'PL':
        common.update({('ShaderSwap', 'Enabled'): '0', ('Profiler', 'GPUProf'): '0',
                       ('Debug', 'GIProbe'): '0', ('Patches', 'GIMerge'): '0'})
    if sec == 'ExtraPerf':
        common.update({('ExtraPerf', 'Enabled'): '1', ('ExtraPerf', 'Preset'): '0'})
        notes.append('ON=1 tests the least aggressive nonzero level; visual trade-off, not automatically a same-picture default.')
    if key in LOAD_KEYS:
        notes.append('Keep the same boss fight, but also time matched cold/warm entry or boot as appropriate; steady-fight FPS alone cannot rule out load benefit.')
    if key == 'WorkerCap':
        notes.append('ON=10 workers; documented handheld candidate, may reduce desktop parallel width. This is not a boolean.')
    if key == 'TSY':
        notes.append('ON=4 threads versus stock 3; value 1 clamps to 3 and would not be a valid treatment.')
    if key == 'MaxFrameLatency':
        notes.append('Baseline -1 is auto (native queue value 1, explicit 1 under Proton); treatment 2 tests queue depth and may increase latency.')
    if key == 'MenuQuantize':
        common[('Patches', 'VRR')] = '1'
        notes.append('Menu-specific: capture a fixed menu interval too; a fight-only null result is inconclusive for this key.')
    if key in VISUAL_KEYS or sec == 'ExtraPerf':
        notes.append('Check motion/visual correctness alongside frame times before any default recommendation.')
    off = s.value if s.identity in AUTO_KEYS else '0'
    return common, off, ON_VALUES.get(s.identity, '1'), status, notes


def sha(text):
    return hashlib.sha256(text.encode('utf-8')).hexdigest()


def assignments(edits):
    return [f'[{section}] {key} = {value}' for (section, key), value in sorted(edits.items())]


def make_plan(source: str, baselines: dict[str, str], boss="Yoritomo-R7B35-fight-now-60s-PresentMon-v1", seconds=60, warmup=30):
    if boss != FIXTURE or seconds != 60:
        raise ValueError("Audit requires the fixed Yoritomo R7B35 fight-now fixture and 60-second capture")
    inventory = catalog(source)
    candidates = [s for s in parse_ini(source) if classification(s) in ('default-off optimization', 'auto-default / latency probe')]
    pairs, files, runs = [], {}, []
    owners = split_keys(candidates)
    for device_index, device in enumerate(DEVICES):
        base = baselines.get(device, source)
        base_settings = {s.identity: s.value for s in parse_ini(base)}
        if not all(s.identity in base_settings for s in candidates):
            raise ValueError(f'{device} baseline lacks one or more candidate keys')
        files[f'{device}/source-baseline.ini'] = base
        for index, setting in enumerate(candidates):
            if owners[setting.identity] != device:
                continue
            common, off, on, status, notes = recipe(setting, device)
            key_id = f'{setting.section}.{setting.key}'
            prefix = f'{device}/{key_id}'
            changes = {'OFF': {**common, setting.identity: off}, 'ON': {**common, setting.identity: on}}
            arms = {arm: edit_ini(base, edits) for arm, edits in changes.items()}
            decoded = {arm: {s.identity: s.value for s in parse_ini(text)} for arm, text in arms.items()}
            diff = {key for key in decoded['OFF'] if decoded['OFF'][key] != decoded['ON'][key]}
            if diff != {setting.identity}:
                raise ValueError(f'Invalid single-key comparison for {key_id}: {diff}')
            for arm, text in arms.items():
                files[f'{prefix}/{arm}.ini'] = text
                files[f'{prefix}/{arm}-edits.txt'] = '\n'.join(assignments(changes[arm])) + '\n'
            pair = dict(device=device, platform=DEVICES[device], key=key_id, source_line=setting.line,
                        shipped_default=setting.value, supplied_baseline_value=base_settings[setting.identity],
                        off=off, on=on, status=status, notes=notes, common_edits=assignments(common),
                        off_edits=assignments(changes['OFF']), on_edits=assignments(changes['ON']),
                        off_sha256=sha(arms['OFF']), on_sha256=sha(arms['ON']), boss=boss,
                        capture_seconds=seconds, warmup_seconds=warmup, runs_per_arm=1)
            pairs.append(pair)
    plan = dict(schema=2, stage=1, source_sha256=sha(source), boss=boss, devices=DEVICES, candidate_count=len(candidates),
                explicit_default_off_count=sum(s.value=='0' for s in candidates),
                auto_or_latency_count=sum(s.value!='0' for s in candidates),
                pairs=pairs, inventory=inventory)
    plan['runs'] = schedule(pairs, stage=1, warmup=warmup)
    plan['split'] = {d: [p['key'] for p in pairs if p['device']==d] for d in DEVICES}
    return plan, files


def read(path):
    with Path(path).open(encoding='utf-8-sig', newline='') as stream:
        return stream.read()


FIXTURE = 'Yoritomo-R7B35-fight-now-60s-PresentMon-v1'
METRICS = ('avg_fps', 'low1_fps', 'frametime_ms')


def split_keys(candidates):
    """Balance 32/31/31, assigning restricted keys first to a usable device."""
    counts = dict.fromkeys(DEVICES, 0)
    targets = {d: len(candidates)//3 + (i < len(candidates)%3) for i,d in enumerate(DEVICES)}
    def eligible(s):
        choices = [d for d in DEVICES if recipe(s,d)[3] in ('planned','requires_fixture')]
        return choices or ['ally']  # SR: inventory it, but auto-ON/refused leaves no real default-off screen.
    owners = {}
    for s in sorted(candidates, key=lambda s: (len(eligible(s)), s.section, s.key)):
        choices = eligible(s)
        available = [d for d in choices if counts[d] < targets[d]] or choices
        device = min(available, key=lambda d: (counts[d], list(DEVICES).index(d)))
        owners[s.identity] = device
        counts[device] += 1
    return owners


def schedule(pairs, stage, warmup=30):
    runs = []
    for device in DEVICES:
        owned = [p for p in pairs if p['device']==device]
        if not owned:
            continue
        local_sequence = 0
        def add(pair=None, arm='BASELINE', replicate=0):
            nonlocal local_sequence
            local_sequence += 1
            key = pair['key'] if pair else '__baseline__'
            runs.append(dict(run_id=f's{stage}-{device}-{local_sequence:04d}', device=device, key=key,
                replicate=replicate, arm=arm, boss=FIXTURE, collector='PresentMon', capture_seconds=60,
                warmup_seconds=warmup, restart_required=True, status=pair['status'] if pair else 'planned',
                ini=f"{device}/{key}/{arm}.ini" if pair else f'{device}/source-baseline.ini', result='UNMEASURED'))
        add()  # anchor for measured drift
        for index,pair in enumerate(owned):
            for replicate in range(1, (1 if stage==1 else 3)+1):
                for arm in (('OFF','ON') if (index+replicate)%2 else ('ON','OFF')):
                    add(pair,arm,replicate)
            if (index+1)%8==0:
                add()
        if len(owned)%8:
            add()  # closing anchor bounds drift for the final partial block
    return runs


def evaluate(plan, rows):
    """Strict run-id join; percent baseline range/mean measures drift independently per device/metric."""
    expected = {r['run_id']:r for r in plan['runs']}
    observations = {}
    seen = set()
    for row in rows:
        rid = row['run_id']
        if rid not in expected or rid in seen:
            raise ValueError(f'Unknown/duplicate result run_id: {rid}')
        seen.add(rid)
        if row.get('valid','').lower() not in ('1','true'):
            continue
        run = expected[rid]
        if row.get('fixture') != FIXTURE or row.get('ini_sha256') != plan['ini_hashes'][run['ini']]:
            raise ValueError(f'Fixture/INI hash mismatch: {rid}')
        values = {m:float(row[m]) for m in METRICS}
        if any(not math.isfinite(v) or v<=0 for v in values.values()):
            raise ValueError(f'Invalid positive metric: {rid}')
        observations[rid] = values
    verdicts = []
    for pair in plan['pairs']:
        device,key = pair['device'],pair['key']
        baselines = [r for r in plan['runs'] if r['device']==device and r['arm']=='BASELINE']
        relevant = [r for r in plan['runs'] if r['device']==device and r['key']==key]
        record = dict(device=device,key=key,verdict='INCOMPLETE',delta_pct={},spread_pct={})
        if pair['status'] not in ('planned','requires_fixture'):
            record['verdict']='NOT_APPLICABLE'
        elif len(baselines)>=2 and all(r['run_id'] in observations for r in baselines+relevant):
            spread = {m:100*(max(observations[r['run_id']][m] for r in baselines)-min(observations[r['run_id']][m] for r in baselines))/statistics.mean(observations[r['run_id']][m] for r in baselines) for m in METRICS}
            means = {arm:{m:statistics.mean(observations[r['run_id']][m] for r in relevant if r['arm']==arm) for m in METRICS} for arm in ('OFF','ON')}
            delta = {m:100*(means['ON'][m]-means['OFF'][m])/means['OFF'][m]*( -1 if m=='frametime_ms' else 1) for m in METRICS}
            record.update(delta_pct=delta,spread_pct=spread,means=means)
            if plan['stage']==1:
                record['verdict']='ADVANCE' if any(delta[m]>spread[m] for m in METRICS) else 'DROP_SCREEN'
            else:
                record['verdict']='KEEP' if any(delta[m]>2*spread[m] for m in METRICS) and delta['low1_fps']>=0 else 'DROP'
        verdicts.append(record)
    return verdicts


def confirmation(screen, decisions, folder):
    winners = {(r['device'],r['key']) for r in decisions if r['verdict']=='ADVANCE'}
    pairs = [dict(p,runs_per_arm=3) for p in screen['pairs'] if (p['device'],p['key']) in winners]
    plan = dict(screen,stage=2,pairs=pairs,screen_decisions=decisions)
    plan['runs'] = schedule(pairs,stage=2)
    plan['split'] = {d:[p['key'] for p in pairs if p['device']==d] for d in DEVICES}
    names = {r['ini'] for r in plan['runs']}
    files = {}
    for name in names:
        text = read(folder/name)
        if sha(text)!=screen['ini_hashes'][name]:
            raise ValueError(f'Frozen Stage-1 INI changed: {name}')
        files[name]=text
    for p in pairs:
        for arm in ('OFF','ON'):
            files[f"{p['device']}/{p['key']}/{arm}-edits.txt"]='\n'.join(p[arm.lower()+'_edits'])+'\n'
    return plan,files


def write_plan(out, plan, files):
    out.mkdir(parents=True,exist_ok=False)
    plan['ini_hashes'] = {name:sha(text) for name,text in files.items() if name.endswith('.ini')}
    for name,text in files.items():
        path=out/name
        path.parent.mkdir(parents=True,exist_ok=True)
        with path.open('w',encoding='utf-8',newline='') as stream:
            stream.write(text)
    (out/'manifest.json').write_text(json.dumps(plan,indent=2)+'\n',encoding='utf-8')
    for name,rows in [('schedule.csv',plan['runs']),('inventory.csv',plan['inventory'])]:
        with (out/name).open('w',newline='',encoding='utf-8') as stream:
            fields=list(rows[0]) if rows else ['run_id']
            writer=csv.DictWriter(stream,fieldnames=fields)
            writer.writeheader();writer.writerows(rows)
    with (out/'results-template.csv').open('w',newline='',encoding='utf-8') as stream:
        writer=csv.DictWriter(stream,fieldnames=['run_id','fixture','ini_sha256','valid',*METRICS])
        writer.writeheader()
        writer.writerows(dict(run_id=r['run_id'],fixture=FIXTURE,ini_sha256=plan['ini_hashes'][r['ini']],valid=0) for r in plan['runs'])


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ini',type=Path,default=DEFAULT_INI)
    parser.add_argument('--baseline',action='append',default=[],metavar='DEVICE=FILE')
    parser.add_argument('--out',required=True,type=Path)
    parser.add_argument('--stage',choices=('screen','confirm','evaluate'),default='screen')
    parser.add_argument('--plan',type=Path,help='existing screen/confirmation plan directory')
    parser.add_argument('--results',type=Path,help='filled results-template.csv')
    args=parser.parse_args(argv)
    if args.stage=='screen':
        baselines={}
        for value in args.baseline:
            device,sep,path=value.partition('=')
            if not sep or device not in DEVICES or device in baselines:
                parser.error('baseline must be a unique pc=FILE, pc2=FILE or ally=FILE')
            baselines[device]=read(path)
        plan,files=make_plan(read(args.ini),baselines)
    else:
        if not args.plan or not args.results:
            parser.error('--plan and --results are required')
        plan=json.loads(read(args.plan/'manifest.json'))
        with args.results.open(newline='',encoding='utf-8-sig') as stream:
            decisions=evaluate(plan,list(csv.DictReader(stream)))
        if args.stage=='evaluate':
            args.out.mkdir(parents=True,exist_ok=False)
            (args.out/'decisions.json').write_text(json.dumps(decisions,indent=2)+'\n',encoding='utf-8')
            print({v:sum(r['verdict']==v for r in decisions) for v in sorted({r['verdict'] for r in decisions})})
            return 0
        if plan['stage']!=1:
            parser.error('confirmation requires a Stage-1 screen manifest')
        plan,files=confirmation(plan,decisions,args.plan)
    write_plan(args.out,plan,files)
    print(f"Stage {plan['stage']}: {len(plan['pairs'])} keys, {len(plan['runs'])} captures including baseline drift anchors; "
          + ', '.join(f'{d}={len(keys)} keys' for d,keys in plan['split'].items()))
    return 0


if __name__=='__main__':
    raise SystemExit(main())
