#!/usr/bin/env python3
"""boss_join.py - join every notes\\boss-*.csv / battle-scrolls*.csv into notes\\boss-master.csv (+ boss-master.md).

Static data only. Re-run after new notes land:  py re\\scripts_re\\boss_join.py
Missing input files are skipped (columns stay empty).

Row universe:
  kind=script  one row per boss mission script (union of every per-script table; 110 today)
  kind=online  one row per web-listed (mission, boss) fight that no script row claimed
                (sub-missions, demons, Battle Scroll/Rift/field bosses with no script match yet)

Join keys:
  script hash -> duelflow/fights/missions/ai/bgm/arenas/script-args/romaji
  duelflow.boss_char_id == enemy-params.key64 (+0x64 record key, NOT +0x08 char_id) -> real char_id, name, HP, group
  english name (normalized) -> web mission list, names-online, text index (ja), loot, rewards, achievements
  mission flag ids -> achievements
"""
import csv, os, re, sys, collections

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
N = os.path.join(ROOT, 'notes')
csv.field_size_limit(1 << 30)


def load(name, delim=','):
    p = os.path.join(N, name)
    if not os.path.exists(p):
        print('missing', name, file=sys.stderr)
        return []
    with open(p, encoding='utf-8-sig', newline='') as f:
        return list(csv.DictReader(f, delimiter=delim))


def norm(s):
    s = re.sub(r'\(.*?\)', ' ', s or '')
    s = s.lower().replace('yakotsu', 'jakotsu')
    return re.sub(r'[^a-z0-9]+', ' ', s).strip()


def uniq(seq):
    out = []
    for x in seq:
        if x and x not in out:
            out.append(x)
    return out


def by(rows, key):
    d = collections.defaultdict(list)
    for r in rows:
        d[r.get(key, '')].append(r)
    return d


# ---------------------------------------------------------------- load
duel = by(load('boss-map-duelflow.csv'), 'script')
fights = by(load('boss-fights-table.csv'), 'script')
miss = by(load('boss-missions.csv'), 'script')
ai = by(load('boss-ai.csv'), 'script')
bgm = by(load('boss-bgm-events.csv'), 'script')
arenas = by(load('boss-arenas.csv'), 'script')
sargs = by(load('boss-script-args.csv'), 'script')
romaji = by(load('boss-romaji-match.csv'), 'script')
mapdata = by(load('boss-map-data.csv'), 'script')
params_rows = load('boss-enemy-params.csv')
params_by_key = {r['key64']: r for r in params_rows}
params_by_group = by(params_rows, 'group')
loot_by_char = {r['char_id']: r for r in load('boss-loot.csv')}
rewards = load('boss-rewards.csv')
achs = load('boss-achievements.csv')
online_names = load('boss-names-online.csv')
per_mission = load('boss-per-mission-online.csv')
scrolls = load('battle-scrolls-online.csv')
text_index = load('boss-text-index.csv')

# ---- v2 sources
mapall = load('boss-map-all.csv')
mapall_by_script = by(mapall, 'script')
dchecks = load('boss-defeat-checks.csv')
bscrolls = load('battle-scrolls.csv')
bscroll_by_id = {r['mission_id_dec']: r for r in bscrolls}
missions_csv = load('missions.csv')
mission_by_key = {r['mission_key_u16(+158)']: r for r in missions_csv}
missions_main = load('missions-main.csv')
tokens = [t for t in load('boss-token-resolution.csv') if t['status'] in ('exact', 'likely') and t['name_en']
          and 'not boss' not in t['name_en']]
drops = by(load('boss-drops.csv'), 'char_id')
items_named = load('items-boss-named.csv')
codex = [r for r in load('boss-codex.csv') if r['row_kind'] == 'entry' and r['english_name']]
mission_by_name = {}
for r in missions_csv:
    if r['name_en']:
        mission_by_name.setdefault(norm(r['name_en']), r['mission_key_u16(+158)'])
for r in missions_main:
    if r['name_en']:
        mission_by_name.setdefault(norm(r['name_en']), r['main_key_u16'])
for r in mapall:
    if r['mission_name']:
        mission_by_name.setdefault(norm(r['mission_name']), r['mission_id'])
CLASS_TYPE = {'main': 'main', 'prologue': 'main', 'main-title-record': 'main', 'sub-kitan': 'sub',
              'optional-crucible-story': 'sub', 'dojo-way': 'sub', 'emaki-picture-scroll': 'emaki',
              'tutorial': 'tutorial'}
CONF_RANK = {'low': 1, 'medium-low': 2, 'medium': 3, 'medium-high': 4, 'high': 5}


def ev_conf(e):
    m = re.search(r'(medium-low|medium-high|low|medium|high)\.?\s*$', e or '')
    return m.group(1) if m else 'low'


def scroll_status(mid):
    b = bscroll_by_id.get(mid)
    if not b:
        return 'no', ''
    if b['battle_scroll_candidate'] == '1':
        return 'yes', b['menu_cat']
    if 'NOT listed' in b['boss_or_content']:
        return 'hidden-duel', b['menu_cat']
    return 'no', b['menu_cat']

ja_by_name = {}
for r in text_index:
    for k in (r.get('name_en'), r.get('atom'), r.get('boss_text')):
        if k and r.get('name_ja'):
            ja_by_name.setdefault(norm(k), (r['name_ja'], 'boss-text-index'))
for r in online_names:
    if r.get('japanese_name_if_found'):
        ja_by_name.setdefault(norm(r['english_name']), (r['japanese_name_if_found'], 'boss-names-online'))

# web fight list: (mission, type, region, order, boss)
web = []
BADBOSS = ('none', 'unverified', 'unnamed', 'training', 'no named', 'a yokai', 'ascetic', 'boss waves')
for r in per_mission:
    for b in re.split(r';|\+', r['bosses']):
        b0 = re.sub(r'\(.*?\)', '', b).strip(' .')
        if not b0 or any(b0.lower().startswith(x) for x in BADBOSS):
            continue
        web.append(dict(mission=r['mission_name'], mtype=r['mission_type'], region=r['region'],
                        order=r['order'], boss=b0, src='boss-per-mission-online',
                        conflicts=r.get('conflicts', ''), sources=r.get('sources', '')))
web_by_name = by([dict(w, nk=norm(w['boss'])) for w in web], 'nk')
for w in web:
    w['nk'] = norm(w['boss'])


def base_title(m):
    return norm(re.split(r'\(obj|\(objs', m)[0])


REGION_TOK = [('SEN', 'Warring States 1572'), ('SENGOKU', 'Warring States 1572'), ('HEI', 'Heian 1190'),
              ('KOD', 'Antiquity 247'), ('BAK', 'Bakumatsu 1864'), ('EDO', 'Edo 1622'), ('DLC', 'DLC1 Hell Rising')]


def region_from_text(*texts):
    for t in texts:
        t = (t or '').upper()
        m = re.search(r'(?:^|[_;])S0?(\d)_(SEN|HEI|KOD|BAK|EDO)|CH\d+_(?:WORLD_)?(SENGOKU|HEI|BAKU?|KOD|EDO)|S90_DLC', t)
        if m:
            tok = next(g for g in m.groups() if g) if any(m.groups()) else 'DLC'
            if tok.isdigit():
                tok = m.group(2)
            for k, v in REGION_TOK:
                if tok.startswith(k):
                    return v
            if 'DLC' in t:
                return 'DLC1 Hell Rising'
    return ''


def ints(s):
    return re.findall(r'\d+', s or '')


COLS = ['row_kind', 'english_name', 'japanese_name', 'mission_name', 'mission_type', 'region', 'story_order',
        'script_file', 'stage_id', 'boss_char_id', 'param_char_id', 'placement_id', 'all_placements',
        'enemy_record_key', 'enemy_table_lfm', 'base_hp', 'variant_group', 'variant_siblings',
        'defeat_check_kind', 'defeat_check_value', 'achievement', 'arena_box_ids', 'gate_gadget_ids',
        'clear_gadget_ids', 'bgm_boss_ids', 'bgm_mission_ids', 'rewards_known', 'refightable_in_game',
        'mission_id', 'mission_class', 'in_battle_scrolls', 'refight_route', 'codex_entry',
        'column_confidence', 'column_sources', 'confidence', 'sources']

rows = []
claimed = set()  # id(web fight) claimed by a script row

scripts = sorted(set(duel) | set(fights) | set(miss) | set(ai) | set(bgm) | set(arenas) | set(sargs))
scripts = [s for s in scripts if s] + [s for s in mapall_by_script if s and s not in scripts]
script_variants = []
for s in sorted(set(scripts)):
    mv = mapall_by_script.get(s, [])
    withb = [x for x in mv if x['boss_placement_id']]
    for ma_ in (withb or mv[:1] or [None]):
        script_variants.append((s, ma_))

for s, ma in script_variants:
    cc, cs = {}, {}
    r = {c: '' for c in COLS}
    r['row_kind'] = 'script'
    r['script_file'] = s
    src = []
    d = duel.get(s, [None])[0]
    f = fights.get(s, [None])[0]
    m = miss.get(s, [None])[0]
    a = ai.get(s, [None])[0]
    b = bgm.get(s, [None])[0]
    conf = []

    # ---- ids
    if ma and ma['boss_placement_id']:
        src.append('boss-map-all')
        r['boss_char_id'] = ma['boss_char_id']
        r['placement_id'] = ma['boss_placement_id']
        r['enemy_record_key'] = ma['boss_name']
        r['enemy_table_lfm'] = ma['placement_file']
        allp = [x for x in ((d or {}).get('script_boss_consts') or '').split(';') if x]
        r['all_placements'] = ';'.join(allp)
        r['mission_id'] = ma['mission_id']
        conf.append('ids:mapall-' + ('agrees-duelflow' if ma['agrees_prev'] == 'yes' else 'new' if not ma['agrees_prev'] else 'corrects-duelflow'))
        cc['ids'] = 'high' if (ma['boss_candidates'] == '1' or ma['agrees_prev'] == 'yes') else 'medium'
        cs['ids'] = 'boss-map-all'
    elif ma and ma['agrees_prev'] == 'NO':
        src.append('boss-map-all')
        conf.append('ids:none(mapall-no-flag4-record)')
    elif d:
        cc['ids'] = d['confidence']; cs['ids'] = 'boss-map-duelflow'
        src.append('boss-map-duelflow')
        r['boss_char_id'] = d['boss_char_id']
        r['placement_id'] = d['boss_placement_id']
        r['enemy_record_key'] = d['boss_record_name']
        r['enemy_table_lfm'] = d['enemy_table_lfm']
        allp = [x for x in (d.get('script_boss_consts') or '').split(';') if x]
        r['all_placements'] = ';'.join(allp)
        conf.append('ids:' + d['confidence'])
    pr = params_by_key.get(r['boss_char_id']) if r['boss_char_id'] else None
    names = []
    if pr:
        cc['hp'] = 'high'; cs['hp'] = 'boss-enemy-params'
        src.append('boss-enemy-params')
        r['param_char_id'] = pr['char_id']
        r['base_hp'] = pr['hp_0x18']
        r['variant_group'] = pr['group']
        sib = params_by_group.get(pr['group'], [])
        r['variant_siblings'] = ';'.join('%s:hp%s' % (x['char_id'], x['hp_0x18']) for x in sib)
        if pr['name']:
            names.append(pr['name'])
            cc['name'] = 'high'; cs['name'] = 'boss-enemy-params(key64)'
    rj = romaji.get(s, [])
    rj_names = [x['english_name'] for x in rj if x['match_confidence'] in ('exact', 'likely')
                and x['english_name'] and 'unverified' not in x['english_name']]
    if rj:
        src.append('boss-romaji-match')
    if not names:
        names = rj_names[:]
    if not names and ma and ma['boss_name']:
        bl = ma['boss_name'].lower()
        tk = [t for t in tokens if t['token'] in bl]
        if tk:
            names = [tk[0]['name_en']]; src.append('boss-token-resolution')
            cc['name'] = 'medium'; cs['name'] = 'boss-token-resolution'
    if not names and ma:
        bsx = bscroll_by_id.get(ma['mission_id'])
        if bsx and bsx['boss_or_content'] and not re.search(r'no boss|^\?|NOT listed|formidable|rush|all enemies', bsx['boss_or_content']):
            nm = re.sub(r'\(.*?\)|\[.*?\]', '', bsx['boss_or_content']).strip(' ?')
            if nm:
                names = [nm]; cc['name'] = 'low'; cs['name'] = 'battle-scrolls.csv boss_or_content'
    if names and 'name' not in cc:
        cc['name'] = 'medium'; cs['name'] = 'boss-romaji-match'
    names = uniq(names)

    # ---- mission
    stage = ''
    mname = ''
    if m:
        src.append('boss-missions')
        stage = m['mission_id']
        if not m['mission_name_en'].startswith('unresolved'):
            mname = m['mission_name_en']
        r['mission_type'] = '' if m['type'] == 'unknown' else m['type']
        r['region'] = '' if m['region'] == 'unknown' else m['region']
    if f:
        src.append('boss-fights-table')
        if not stage and f['stages']:
            stage = f['stages'].split(';')[0]
    r['stage_id'] = stage
    if ma:
        r['mission_id'] = ma['mission_id']
        r['mission_class'] = ma['mission_class']
        if ma['mission_name'] and ma['mission_name'] != 'Dummy':
            mname = ma['mission_name']
        if not r['mission_type'] or r['mission_type'] in ('unknown',):
            r['mission_type'] = CLASS_TYPE.get(ma['mission_class'], ma['mission_class'])
        bsr0 = bscroll_by_id.get(ma['mission_id'])
        if bsr0 and not r['region'] and bsr0['era']:
            r['region'] = bsr0['era']
        cc['mission'] = 'high'; cs['mission'] = 'boss-map-all+missions.csv'
        src.append('missions.csv')
    if not r['region']:
        r['region'] = region_from_text(stage, r['enemy_record_key'])

    # ---- web mission join by boss name
    cand = []
    for n in names:
        cand += web_by_name.get(norm(n), [])
    pick = None
    if cand:
        if mname:
            bt = [w for w in cand if base_title(w['mission']) and base_title(w['mission']) in norm(mname)]
            if bt:
                pick = bt[0]
        if not pick and len(cand) == 1 and not mname:
            pick = cand[0]
            conf.append('mission:name-only')
    if pick:
        claimed.update(id(w) for w in cand if w['mission'] == pick['mission'])
        src.append(pick['src'])
        if not mname:
            mname = pick['mission']
        if not r['mission_type']:
            r['mission_type'] = pick['mtype']
        if not r['region']:
            r['region'] = pick['region']
        r['story_order'] = ('M' if pick['mtype'] == 'main' else 'S' if pick['mtype'] == 'sub' else '') + pick['order']
    r['mission_name'] = mname
    if not names and f and f.get('name_tokens'):
        pass
    r['english_name'] = ' / '.join(names)

    # ---- japanese
    ja = []
    for n in names:
        j = ja_by_name.get(norm(n))
        if j:
            ja.append(j[0]); src.append(j[1])
    for x in rj:
        if (x['japanese_name'] and x['match_confidence'] in ('exact', 'likely') and '(' not in x['japanese_name']
                and norm(x['english_name']) in [norm(n) for n in names]):
            ja.append(x['japanese_name'])
    r['japanese_name'] = ' / '.join(uniq(ja))

    # ---- defeat check
    kinds, vals = [], []
    pid = r['placement_id']
    if f:
        if int(f['is_alive_boss'] or 0) and pid:
            kinds.append('Duel::IsAliveBoss'); vals.append('placement=' + pid)
        elif int(f['is_alive_boss'] or 0):
            kinds.append('Duel::IsAliveBoss'); vals.append('placement=?')
        if f['get_boss_placement_args']:
            kinds.append('GetBossPlacementID(arg const)'); vals.append(f['get_boss_placement_args'])
        if int(f['boss_clear_gadget_calls'] or 0):
            kinds.append('ClearGadget')
        if f['flags_in_boss_fns']:
            kinds.append('MissionFlag'); vals.append('flags=' + f['flags_in_boss_fns'])
        if int(f['hell_clear_calls'] or 0):
            kinds.append('HellClear')
        if int(f['set_clear_flag_calls'] or 0):
            kinds.append('SetMissionClearFlag x' + f['set_clear_flag_calls'])
    if a and a.get('hp_thresholds'):
        kinds.append('HpRate'); vals.append('thresholds=' + a['hp_thresholds'])
    if a and re.search(r'wave', a.get('fns_using_ai', '') + a.get('tokens', '')):
        kinds.append('WaveBattle')
    r['defeat_check_kind'] = '+'.join(kinds)
    r['defeat_check_value'] = ' | '.join(vals)

    # ---- defeat checks v2 (boss-defeat-checks.csv)
    dcm = []
    nset = set(norm(n) for n in names)
    for x in dchecks:
        if norm(x['boss']) not in nset:
            continue
        mtxt = x['mission']
        base = norm(re.sub(r'\(obj:.*', '', mtxt))
        objm = re.search(r'\(obj:\s*(.*?)\)?$', mtxt)
        obj = norm(objm.group(1)) if objm else ''
        mn = norm(mname)
        idhit = bool(r['mission_id']) and x['value_or_id'] == 'mission:' + r['mission_id']
        if idhit or (mn and ((base and (base in mn or mn in base)) or (obj and (obj in mn or mn in obj)))):
            dcm.append(x)
    dcm = [x for x in dcm if x['check_kind'] != 'trophy']
    if dcm:
        src.append('boss-defeat-checks')
        best = max(dcm, key=lambda x: CONF_RANK[ev_conf(x['evidence'])])
        cc['defeat'] = ev_conf(best['evidence']); cs['defeat'] = 'boss-defeat-checks'
        for x in dcm:
            if x['check_kind'] == 'clear_map':
                kinds.append('QuestClear'); vals.append('QuestClear=' + x['value_or_id'])
            elif x['check_kind'] == 'progress':
                kinds.append('StoryProgress'); vals.append('StoryProgress' + x['value_or_id'])
            elif x['check_kind'] == 'flag':
                kinds.append('MissionFlag'); vals.append('flag ' + x['value_or_id'][:80])
            else:
                kinds.append(x['check_kind']); vals.append(x['value_or_id'][:80])
        r['defeat_check_kind'] = '+'.join(uniq(kinds))
        r['defeat_check_value'] = ' | '.join(vals)
    elif kinds:
        cc['defeat'] = 'low'; cs['defeat'] = 'script constants (boss-fights-table)'

    # ---- arena / gates
    ar = arenas.get(s, [])
    if ar:
        src.append('boss-arenas')
    boxes = uniq(x['id'] for x in ar if x['kind'] == 'box')
    gates = uniq(x['id'] for x in ar if x['kind'] == 'gadget' and 'SetDoorControl' in x['call'])
    clears = uniq(x['id'] for x in ar if 'WaitClearGadgetAccess' in x['call'])
    sa = sargs.get(s, [])
    if sa:
        src.append('boss-script-args')
    clears += [x['ids_hex'] for x in sa if x['native'] == 'Function::CreateClearGadgetBossPosition' and x['ids_hex']]
    r['arena_box_ids'] = ';'.join(boxes)
    r['gate_gadget_ids'] = ';'.join(gates)
    r['clear_gadget_ids'] = ';'.join(uniq(clears))
    if clears and 'ClearGadget' not in r['defeat_check_kind']:
        r['defeat_check_kind'] += ('+' if r['defeat_check_kind'] else '') + 'ClearGadget'
    if clears:
        r['defeat_check_value'] += (' | ' if r['defeat_check_value'] else '') + 'clear_gadget=' + ';'.join(uniq(clears))

    # ---- bgm
    if b:
        src.append('boss-bgm-events')
    r['bgm_boss_ids'] = ';'.join(uniq(x['ids_hex'] for x in sa if x['native'] == 'Sound::PlayBossBGM' and x['ids_hex']))
    r['bgm_mission_ids'] = ';'.join(uniq(x['ids_hex'] for x in sa if x['native'].startswith('Sound::PlayMissionBGM') and x['ids_hex']))

    # ---- achievements (by name, or by mission-flag id)
    flagset = set(ints(f['flags_all'] if f and f.get('flags_all') else '')) | \
        set(x['id'] for x in ar if x['kind'] == 'flag')
    ach = []
    for ac in achs:
        t = ac['boss_or_target'].lower()
        hit = any(norm(n) and norm(n) in norm(t) for n in names) and t not in ('-', 'any master')
        for h in re.findall(r'flags? (?:0x[0-9a-f]+ \()?(\d+)|0x([0-9a-f]+)', ac['trigger'], re.I):
            for x in h:
                if x:
                    v = str(int(x, 16)) if re.search(r'[a-f]', x, re.I) or ac['trigger'].count('0x') else x
                    if 'flag' in ac['trigger'] and v in flagset:
                        hit = True
        if hit:
            ach.append('%s %s' % (ac['ach_api_name'], ac['steam_name']))
    r['achievement'] = '; '.join(uniq(ach))
    if ach:
        src.append('boss-achievements'); cc['achievement'] = 'medium'; cs['achievement'] = 'boss-achievements'

    # ---- loot / rewards
    rw = []
    lt = loot_by_char.get(r['param_char_id'])
    if lt:
        src.append('boss-loot')
        rw.append(lt['soul_core_item_inferred'] + (' + ' + lt['drop_items_by_name_match_inferred'].strip() if lt['drop_items_by_name_match_inferred'].strip() else ''))
    for x in rewards:
        if names and any(norm(n) == norm(x['boss']) for n in names):
            rw.append('%s: %s' % (x['reward_type'], x['item_name']))
            src.append('boss-rewards')
    for dr in drops.get(r['param_char_id'], []):
        if dr['item_name']:
            rw.append('drop: %s (rewardset %s, %s)' % (dr['item_name'], dr['reward_set_id'], dr['confidence']))
            src.append('boss-drops')
    for it in items_named:
        if names and norm(it['boss_atom']) in [norm(n) for n in names]:
            rw.append('%s: %s' % (it['category'], it['name_en']))
            src.append('items-boss-named')
    r['rewards_known'] = ' ; '.join(uniq(rw))
    if r['rewards_known']:
        cc['rewards'] = 'low'; cs['rewards'] = 'boss-loot/boss-drops(inferred)/items-boss-named(name match)'
    cx = [c for c in codex if norm(c['english_name']) in [norm(n) for n in names]]
    r['codex_entry'] = ';'.join(uniq(c['key'] for c in cx))

    # ---- battle scroll + refight route
    if not r['mission_id'] and mname:
        mk = mission_by_name.get(norm(mname)) or mission_by_name.get(base_title(mname))
        if mk:
            r['mission_id'] = mk
    if r['mission_id']:
        r['in_battle_scrolls'], _ = scroll_status(r['mission_id'])
        cc['scroll'] = 'high'; cs['scroll'] = 'battle-scrolls.csv'
    else:
        r['in_battle_scrolls'] = 'no'
        cc['scroll'] = 'low'; cs['scroll'] = 'no mission id; assumed'
    mt = r['mission_type']
    bsr = bscroll_by_id.get(r['mission_id'])
    if r['in_battle_scrolls'] == 'yes':
        route = 'Battle Scroll' + (' (Eternal Rift tab)' if bsr and bsr['era'] == 'Eternal Rift' else '')
    elif r['in_battle_scrolls'] == 'hidden-duel':
        route = 'Emaki override (or un-hide flag 0x40)'
    elif mt in ('main', 'sub') and r['mission_id']:
        route = 'mission replay'
    elif r['placement_id']:
        route = 'Emaki override'
    else:
        route = 'none'
    r['refight_route'] = route
    cc['refight'] = 'medium' if r['in_battle_scrolls'] != 'no' or route == 'none' else 'low'
    cs['refight'] = 'derived: battle-scrolls.csv + eternal-rift.md + boss-rematch-paths.md'

    # ---- refightable
    refs = []
    if r['mission_type'] in ('main', 'sub', 'main/sub', 'sub/main', 'sub?', 'dlc'):
        refs.append('mission-replay(shrine)')
        refs.append("NG+(Shogun's Journey, fresh flag slot)")
    for sc in scrolls:
        if names and any(norm(n) in norm(sc['boss']) for n in names):
            if sc['in_eternal_rift'].startswith('yes'):
                refs.append('Eternal-Rift')
            if sc['in_battle_scrolls'] not in ('n/a', 'no', ''):
                refs.append('BattleScroll(%s)' % sc['in_battle_scrolls'].split(' ')[0])
    if not any('BattleScroll' in x for x in refs) and r['mission_type'] in ('main', 'sub', 'main/sub'):
        refs.append('BattleScroll(likely; web summary)')
    r['refightable_in_game'] = ';'.join(uniq(refs))

    if not conf:
        conf.append('ids:none')
    if not mname:
        conf.append('mission:unresolved')
    if mname and 'mission' not in cc:
        cc['mission'] = 'low'; cs['mission'] = 'web name-only'
    if ma and ma['mission_class'] == 'tutorial':
        r['row_kind'] = 'nonboss'
    r['column_confidence'] = ';'.join('%s=%s' % kv for kv in cc.items())
    r['column_sources'] = ';'.join('%s=%s' % kv for kv in cs.items())
    r['confidence'] = ' '.join(conf)
    r['sources'] = ';'.join(uniq(src))
    rows.append(r)

# ---------------------------------------------------------------- online-only rows
for w in web:
    if id(w) in claimed:
        continue
    r = {c: '' for c in COLS}
    r['row_kind'] = 'online'
    r['english_name'] = w['boss']
    j = ja_by_name.get(w['nk'])
    r['japanese_name'] = j[0] if j else ''
    r['mission_name'] = w['mission']
    r['mission_type'] = w['mtype']
    r['region'] = w['region']
    r['story_order'] = ('M' if w['mtype'] == 'main' else 'S' if w['mtype'] == 'sub' else '') + w['order']
    refs = []
    if w['mtype'] in ('main', 'sub'):
        refs += ['mission-replay(shrine)', "NG+(Shogun's Journey, fresh flag slot)"]
    for sc in scrolls:
        if w['nk'] and w['nk'] in norm(sc['boss']):
            if sc['in_eternal_rift'].startswith('yes'):
                refs.append('Eternal-Rift')
            if sc['in_battle_scrolls'] not in ('n/a', 'no', ''):
                refs.append('BattleScroll(%s)' % sc['in_battle_scrolls'].split(' ')[0])
    pr = next((p for p in params_rows if p['name'] and norm(p['name']) == w['nk']), None)
    if pr:
        r['param_char_id'] = pr['char_id']; r['base_hp'] = pr['hp_0x18']; r['variant_group'] = pr['group']
        r['variant_siblings'] = ';'.join('%s:hp%s' % (x['char_id'], x['hp_0x18']) for x in params_by_group.get(pr['group'], []))
        lt = loot_by_char.get(pr['char_id'])
        if lt:
            r['rewards_known'] = lt['soul_core_item_inferred']
    ach = [('%s %s' % (ac['ach_api_name'], ac['steam_name'])) for ac in achs
           if w['nk'] and w['nk'] in norm(ac['boss_or_target']) and ac['boss_or_target'] not in ('-',)]
    r['achievement'] = '; '.join(ach)
    mk = mission_by_name.get(norm(w['mission'])) or mission_by_name.get(base_title(w['mission']))
    cc, cs = {'mission': 'low', 'name': 'low'}, {'mission': 'web', 'name': 'boss-per-mission-online'}
    if mk:
        r['mission_id'] = mk
        r['in_battle_scrolls'], _ = scroll_status(mk)
        r['mission_class'] = (mission_by_key.get(mk) or {}).get('class', '')
        cc['scroll'] = 'medium'; cs['scroll'] = 'battle-scrolls.csv via mission-name match'
    else:
        r['in_battle_scrolls'] = 'no'; cc['scroll'] = 'low'; cs['scroll'] = 'no mission id; assumed'
    if r['in_battle_scrolls'] == 'yes':
        r['refight_route'] = 'Battle Scroll'
    elif r['in_battle_scrolls'] == 'hidden-duel':
        r['refight_route'] = 'Emaki override (or un-hide flag 0x40)'
    elif w['mtype'] in ('main', 'sub') and mk:
        r['refight_route'] = 'mission replay'
    else:
        r['refight_route'] = 'none'
    cc['refight'] = 'low'; cs['refight'] = 'derived'
    r['codex_entry'] = ';'.join(uniq(c['key'] for c in codex if norm(c['english_name']) == w['nk']))
    for dr in drops.get(r['param_char_id'], []):
        if dr['item_name']:
            r['rewards_known'] = (r['rewards_known'] + ' ; ' if r['rewards_known'] else '') + 'drop: %s (%s)' % (dr['item_name'], dr['confidence'])
    r['column_confidence'] = ';'.join('%s=%s' % kv for kv in cc.items())
    r['column_sources'] = ';'.join('%s=%s' % kv for kv in cs.items())
    r['refightable_in_game'] = ';'.join(uniq(refs))
    r['confidence'] = 'web-only' + (' ' + w['conflicts'] if w['conflicts'] and w['conflicts'] != 'none' else '')
    r['sources'] = ';'.join(uniq([w['src']] + (['boss-enemy-params'] if pr else []) + (['boss-achievements'] if ach else [])))
    rows.append(r)

# ---------------------------------------------------------------- write
out = os.path.join(N, 'boss-master.csv')
with open(out, 'w', encoding='utf-8', newline='') as fh:
    w = csv.DictWriter(fh, fieldnames=COLS)
    w.writeheader()
    w.writerows(rows)

# ---------------------------------------------------------------- md
def cov(kind=None):
    rs = [r for r in rows if kind is None or r['row_kind'] == kind]
    return len(rs), {c: sum(1 for r in rs if r[c]) for c in COLS}

nall, call = cov()
nsc, csc = cov('script')
non, con = cov('online')
nnb, _cnb = cov('nonboss')
nsc_all = nsc
lines = ['# Boss master table (generated by re\\scripts_re\\boss_join.py; do not hand-edit)', '',
         'CSV: `notes\\boss-master.csv`. %d rows = %d script rows (one per boss fight that has a mission script; boss-map-all.csv supplies mission id, placement and boss char id, one row per (script, boss mission)) + %d non-boss script rows (tutorials, no boss) + %d web-only rows (no script matched yet). v2 joins: boss-map-all, boss-defeat-checks, battle-scrolls, missions(-main), boss-token-resolution, boss-drops, items-boss-named, boss-codex.' % (nall, nsc, nnb, non),
         'Static data only; game not run. Regenerate: `py re\\scripts_re\\boss_join.py`.', '',
         '## Join keys',
         '- script hash joins duelflow, fights-table, missions, ai, bgm, arenas, script-args, romaji.',
         '- `duelflow.boss_char_id` equals enemy-params `key64` (record key at +0x64), NOT `char_id`; `param_char_id` is the real +0x08 char_id (loot/names key).',
         '- English name (normalized) joins the web mission list, names-online, text index (Japanese), loot, rewards, achievements; mission-flag ids join achievements.',
         '- Row confidence column: `ids:` = duelflow confidence; `mission:name-only` = mission picked only by boss name; `mission:unresolved`; `web-only`.', '',
         '## Coverage (rows with the column filled)', '',
         '| column | all (%d) | script (%d) | web-only (%d) |' % (nall, nsc, non), '|---|---|---|---|']
for c in COLS:
    lines.append('| %s | %d | %d | %d |' % (c, call[c], csc[c], con[c]))
lines += ['']

sc_rows = [r for r in rows if r['row_kind'] == 'script']
# ---- v2 summaries
def counter(rs, c):
    return collections.Counter(r[c] or '(empty)' for r in rs)
lines += ['## v2 summary (script rows; web-only rows in brackets)', '']
for c, title in (('in_battle_scrolls', 'in_battle_scrolls'), ('refight_route', 'refight_route'), ('mission_class', 'mission_class')):
    on = counter([r for r in rows if r['row_kind'] == 'online'], c)
    lines.append('- %s: ' % title + ', '.join('%s %d [%d]' % (k, v, on.get(k, 0)) for k, v in counter(sc_rows, c).most_common()))
cconf = collections.defaultdict(collections.Counter)
for r in sc_rows:
    for kv in r['column_confidence'].split(';'):
        if '=' in kv:
            k, v = kv.split('=', 1)
            cconf[k][v] += 1
lines += ['', 'Per-column confidence (script rows; `column_confidence` / `column_sources` hold it per row):', '',
          '| column | n with value | confidence mix |', '|---|---|---|']
for k in sorted(cconf):
    lines.append('| %s | %d | %s |' % (k, sum(cconf[k].values()), ', '.join('%s %d' % kv for kv in cconf[k].most_common())))
lines += ['', 'Defeat-check kinds present (script rows): ' + ', '.join('%s %d' % (k, sum(1 for r in sc_rows if k in r['defeat_check_kind']))
          for k in ('QuestClear', 'StoryProgress', 'MissionFlag', 'ClearGadget', 'Duel::IsAliveBoss', 'HpRate')),
          'Refight route caveats: `Battle Scroll` = mission is in the 62 listable Battle Scroll rows (battle-scrolls.csv, high); `Emaki override (or un-hide flag 0x40)` = the 3 data-hidden duels (Tadakatsu, Yoshitsune, Jubei/Chuya); `mission replay` = main/sub mission not in the scroll list; `Emaki override` = no replay route, but the boss placement id is known so Emaki 0xCC96 +0x10 could host it (boss-rematch-paths.md); `Rift` rows are the 9 Eternal Rift teacher missions, which are Battle Scroll entries (Rift = era tab, no own spawner: eternal-rift.md). `none` = no mission id and no placement.', '']

lines += ['## Remaining gaps (script rows)', '']
gaps = [
    ('no placement/char id (no flag-4 boss record, skeleton or teacher/dojo/wave scripts)', lambda r: not r['placement_id']),
    ('no english_name', lambda r: not r['english_name']),
    ('english_name only low/medium confidence (token or scroll-text derived)', lambda r: 'name=low' in r['column_confidence'] or 'name=medium' in r['column_confidence']),
    ('no mission_name (unnamed/internal stage)', lambda r: not r['mission_name']),
    ('no mission_id', lambda r: not r['mission_id']),
    ('no defeat check from boss-defeat-checks (script constants only)', lambda r: not any(k in r['defeat_check_kind'] for k in ('QuestClear', 'StoryProgress'))),
    ('defeat check confidence below medium', lambda r: 'defeat=low' in r['column_confidence'] or 'defeat=medium-low' in r['column_confidence']),
    ('no BGM id', lambda r: not (r['bgm_boss_ids'] or r['bgm_mission_ids'])),
    ('no arena box id', lambda r: not r['arena_box_ids']),
    ('no gate/clear gadget id', lambda r: not (r['gate_gadget_ids'] or r['clear_gadget_ids'])),
    ('no known reward', lambda r: not r['rewards_known']),
    ('no achievement', lambda r: not r['achievement']),
    ('refight route is a derived guess (confidence low)', lambda r: 'refight=low' in r['column_confidence']),
]
for t, fn in gaps:
    lines.append('- %d / %d: %s' % (sum(1 for r in sc_rows if fn(r)), len(sc_rows), t))
lines += ['',
          '## Gaps the in-game BossLog will fill (`[Debug] BossLog=1`, ASI 1.7.18+; see mod\\Nioh3EngineFix\\src\\bosslog.cpp)',
          'BossLog logs, per real fight (grep `BossLog` in the mod log): fight number and duration (BossBattleMode start/end), the last 16 `Character::Create` placement ids (before the fight) and those spawned inside it, every `SetMissionFlag` id+value during the fight and 20 s after, the HP-bar boss actor id (`ShowBossGauge`), and the mission id passed to `SetMissionClearFlag`.',
          'Maps to columns:',
          '- `mission_name` / `stage_id` for the remaining unnamed/internal-stage scripts: the logged mission id at clear (join to the 130-record mission DB `archive_03\\52\\BD29D455...`).',
          '- `placement_id` / `all_placements` for skeleton scripts and low-confidence rows: logged Character::Create ids confirm which record is the boss; BossBattleMode order gives multi-phase sequence.',
          '- `defeat_check_value` flag ids: the SetMissionFlag ids set at boss death (the real per-boss defeat flag, e.g. the 296-298 demon flags).',
          '- `variant_group` / `param_char_id`: ShowBossGauge actor id plus placement resolves which variant (phase/difficulty copy) actually spawns.',
          '- Also from RematchLog: whether Battle Scroll / hidden-duel / Emaki routes actually start the fight (refight_route is derived, not observed), and the real defeat flag/quest-clear at death (confirms the medium/low defeat-check rows). Not filled by either log: BGM names (hashes only), Japanese names for unmatched bosses, reward tables (rewards_known is name-match inferred), `story_order` of sub-missions.',
          '', '## Other known gaps',
          '- web-only rows have no ids at all: 1:1 name join to a script failed or the fight has no script in the 110 boss scripts (Rift NPCs, field bosses, mid-mission fights).',
          '- Battle Scroll membership is now data-derived (battle-scrolls.csv, mission id) for rows with a mission id; web-only rows are matched by mission name and default to `no` when unmatched. `refightable_in_game` (v1) stays a hypothesis; use `refight_route`.',
          '- Multi-boss missions (e.g. Yagyu Principles = Jubei + Chuya, Best of Five) have one placement row; secondary bosses appear only as web-only rows.',
          '- `boss_char_id` for low/medium duelflow rows may be a world-table boss (see boss-duel-flow.md section 5 caveat).',
          '']
with open(os.path.join(N, 'boss-master.md'), 'w', encoding='utf-8') as fh:
    fh.write('\n'.join(lines))
print('rows', nall, 'script', nsc, 'online', non)
