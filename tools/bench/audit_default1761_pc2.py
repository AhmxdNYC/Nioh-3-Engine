"""Read-only bridge231 audit; timing uses the reported +14h alignment."""
import csv
import datetime as dt
import hashlib
import io
import json
import math
import statistics as st
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ARCHIVE = ROOT / 'captures/pc2/default1761/pc2_default1761.zip'
DIGEST = '617b378790b399d70eb14fb1a6e87699c9f45cf0f2d20d29d33fa8941c3c7244'
ASI = '4734152ACAFDC13FA417C23EE60C8F6579B023D7F93FE73A763D862446A7AD00'
INI = '93CA1930165FB49073E0E4CB291C48164936B2B38041B14EE82486F2766B95B6'


def epoch(value):
    # CSV fractional seconds extend to ns; datetime supports microseconds.
    if ' ' in value:
        day, clock = value.split(' ')
        hour, minute, second = clock.split(':')
        return dt.datetime.fromisoformat(day).replace(tzinfo=dt.timezone.utc).timestamp() + int(hour)*3600 + int(minute)*60 + float(second)
    return dt.datetime.fromisoformat(value.replace('Z', '+00:00')).replace(tzinfo=dt.timezone.utc).timestamp()


def summary(rows, start, end):
    selected = [r for r in rows if start <= epoch(r['CPUStartDateTime']) + 14 * 3600 < end]
    assert len(selected) >= 500
    times = [float(r['FrameTime']) for r in selected]
    assert all(math.isfinite(t) and t > 0 for t in times)
    assert len({r['SwapChainAddress'] for r in selected}) == 1
    assert {r['PresentMode'] for r in selected} == {'Hardware Composed: Independent Flip'}
    assert {r['SyncInterval'] for r in selected} == {'0'}
    assert {r['AllowsTearing'] for r in selected} == {'1'}
    return {'frames': len(selected), 'fps': len(selected) / (end - start),
            'ft': st.mean(times), 'p99': sorted(times)[math.ceil(.99 * len(times)) - 1]}


def main():
    assert hashlib.sha256(ARCHIVE.read_bytes()).hexdigest() == DIGEST
    units = []
    with zipfile.ZipFile(ARCHIVE) as z:
        assert z.testzip() is None
        entries, common = [], []
        for index, arm in enumerate('ABBAA', 1):
            name = f'L{index}_{arm}'
            cell = json.loads(z.read(name + '/cell.json').decode('utf-8-sig'))
            assert cell['arm'] == arm
            entries.append(cell['entry'])
            installed = cell['installed']
            common.append({k: v for k, v in installed.items() if k not in ('asi', 'ini')})
            assert installed['config'] == cell['entry']['config']
            assert installed['asi'] == ('absent' if arm == 'A' else ASI)
            assert installed['ini'] == (cell['entry']['ini'] if arm == 'A' else INI)
            log = z.read(name + '/run.log').decode('utf-8-sig')
            if index == 5:
                assert cell['ok'] is False and not cell.get('windows')
                assert cell['restore'] == 'NOT RESTORED (game running)'
                assert 'TIMED OUT - left running' in cell['exit']
                assert 'lock release:' not in log
                assert not any(n.startswith('L6_') for n in z.namelist())
                print('L5: INVALID; archived cell/log predate reported recovery; no archived release proof')
                continue
            assert cell['ok'] and len(cell['windows']) == 2
            assert cell['restore'] == {'saves_equal': True, 'files_equal': True, 'mismatched': []}
            assert cell['exit'].startswith('graceful (CloseMainWindow)')
            assert 'lock release:' in log
            if arm == 'B':
                native = z.read(name + '/mod_run.log').decode('utf-8-sig')
                assert 'Nioh3EngineFix 1.7.61' in native
            rows = list(csv.DictReader(io.StringIO(z.read(name + '/presentmon.csv').decode('utf-8-sig'))))
            rows = [r for r in rows if None not in r and all(v is not None for v in r.values())]
            assert all(r['Application'] == 'Nioh3.exe' and r['ProcessID'] == str(cell['pid']) for r in rows)
            windows = []
            for wi, window in enumerate(cell['windows'], 1):
                assert window['valid'] and window['start'].startswith('field ') and window['end'].startswith('field ')
                start, end = epoch(window['t0_utc']), epoch(window['t1_utc'])
                assert 19.5 < end - start < 20.5
                assert abs(start - epoch(cell['field_utc']) - (60 if wi == 1 else 85)) < .5
                assert not any(start <= epoch(r['CPUStartDateTime']) < end for r in rows)
                for edge in ('start', 'end'):
                    assert z.read(f'{name}/win{wi}_{edge}.jpg').startswith(b'\xff\xd8')
                windows.append(summary(rows, start, end))
            unit = {k: st.mean(w[k] for w in windows) for k in ('fps', 'ft', 'p99')}
            units.append(unit)
            print(f'{name}: frames {[w["frames"] for w in windows]}, ' + json.dumps(unit))
        assert all(e == entries[0] for e in entries)
        assert all(c == common[0] for c in common)
        assert common[0] == {
            'dinput8': '6203C5A0BA1F8C5C77C7C12C33BE178A5057063EF1DA265EB0A546495C6E6A4E',
            'config': 'CBFEB90A0A28250A1BF47E2A65C56BA5DA49941F54581FBBA122915DA8A0BC67',
            'ghdm_0x0104840c': '129F03A0971E0A073B346224588E2EC0DBC41A4240E1A42EFD285693E5742CE9',
            'fogpar_0xa9910db8': '4784AABE40FDB10646BABDE1DF734123CE713DC56089A55B4EFA0DB43BF4F993'}
    for metric in ('fps', 'ft', 'p99'):
        differences = [units[1][metric] - units[0][metric], units[2][metric] - units[3][metric]]
        baseline = st.mean((units[0][metric], units[3][metric]))
        print(f'{metric}: B-A {st.mean(differences):+.6f}, SE {st.stdev(differences)/math.sqrt(2):.6f}, delta {100*st.mean(differences)/baseline:+.4f}%')
    print('PASS: archive/CRC, arm attestations, eight windows and two paired units. +14h alignment is reported/coverage-based, not independently calibrated. Recovery remains message1928 attestation; no complete protocol or visual/latency verdict.')


if __name__ == '__main__':
    main()
