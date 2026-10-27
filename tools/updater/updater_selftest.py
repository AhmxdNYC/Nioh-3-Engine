"""Packaged executable self-test: generated temp copy, no network or real game access."""
import itertools
import json
from pathlib import Path
import tempfile
from unittest.mock import patch


def run(u):
    with tempfile.TemporaryDirectory(prefix='nioh3-updater-e2e-') as td:
        base = Path(td)
        steam = base / 'Steam'
        library = base / 'Second library'
        (steam / 'steamapps').mkdir(parents=True)
        (steam / 'steamapps/libraryfolders.vdf').write_text(
            '"libraryfolders" { "1" { "path" "' + str(library).replace('\\', '\\\\') + '" } }')
        game = library / 'steamapps/common/Nioh3'
        game.mkdir(parents=True)
        (game / 'Nioh3.exe').write_bytes(b'FAKE - never executable')
        names = ('Nioh3EngineFix.asi', 'Nioh3Training.asi')
        for name in names:
            (game / name).write_bytes(b'old fake 1.7.80')
        ini = game / 'Nioh3EngineFix.ini'
        ini.write_bytes(b'[Player]\nKeep=1\n')
        payload = b'fake 1.7.81 payload - not a game binary'
        manifest = {'schema': 1, 'version': '1.7.81', 'news': 'Local executable test only.', 'files': [
            {'name': n, 'sha256': u.digest(payload),
             'url': 'https://nioh3-mod-feedback.vercel.app/releases/1.7.81/' + n} for n in names]}
        fixture = base / 'stable.json'
        fixture.write_text(json.dumps(manifest))
        hidden = 'https://nioh3-mod-feedback.vercel.app/t/self-test/stable.json'
        config = base / 'Nioh3Updater.ini'
        config.write_text('[Update]\nManifestUrl=' + hidden)
        def fetch(url, limit=u.MAX_FILE):
            u.https(url)
            data = fixture.read_bytes() if url in (u.MANIFEST_URL, hidden) else payload
            if len(data) > limit:
                raise ValueError('Fixture exceeds size limit')
            return data
        with patch.object(u, 'steam_roots', return_value=[steam]), \
             patch.object(u, 'fetch', side_effect=fetch), \
             patch.object(u, 'game_running', side_effect=itertools.chain([True, False], itertools.repeat(False))), \
             patch.object(u.time, 'sleep') as sleep, patch.object(u, 'pause') as pause:
            assert u.main(['--config', str(config)]) == 0, 'Double-click entry point failed'
            assert sleep.call_count == 1, 'Did not wait for simulated running game'
            assert pause.call_count == 1, 'Missing final pause'
            assert all((game / n).read_bytes() == payload for n in names)
            assert ini.read_bytes() == b'[Player]\nKeep=1\n'
            journals = list((game / 'nioh3-update-backups').glob('*/journal.json'))
            assert len(journals) == 1
            backup_id = journals[0].parent.name
            assert u.main(['--game-dir', str(game), '--rollback', backup_id, '--no-pause']) == 0
            assert all((game / n).read_bytes() == b'old fake 1.7.80' for n in names)
            assert ini.read_bytes() == b'[Player]\nKeep=1\n'
            manifest['files'][0]['sha256'] = '0' * 64
            fixture.write_text(json.dumps(manifest))
            assert u.main(['--no-pause']) == 1, 'Corruption accepted'
            assert all((game / n).read_bytes() == b'old fake 1.7.80' for n in names)
        print('PASS: packaged EXE fake 1.7.81 update, Steam discovery, wait, pause, 2 files, settings preservation, backup, rollback, hash rejection.')
        print('Transport was a local fake manifest; no live service or game accessed.')
