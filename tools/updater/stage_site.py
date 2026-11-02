"""Stage immutable update payloads and manifest; never deploys anything."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
from urllib.parse import quote
import updater


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--site', required=True, type=Path)
    p.add_argument('--version', required=True)
    p.add_argument('--payload', action='append', required=True, type=Path)
    p.add_argument('--news', default='Smoother text, controller icons and camera settings.')
    a = p.parse_args()
    files = []
    for source in a.payload:
        files.append({'name': source.name, 'sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                      'url': 'https://nioh3-mod-feedback.vercel.app/releases/' + a.version + '/' + quote(source.name)})
    updater.publish_gate(source.name for source in a.payload)
    m = updater.validate({'schema': 1, 'version': a.version, 'news': a.news, 'files': files})
    target = a.site / 'releases' / a.version
    target.mkdir(parents=True, exist_ok=True)
    for source, entry in zip(a.payload, files):
        out = target / source.name
        if out.exists() and hashlib.sha256(out.read_bytes()).hexdigest() != entry['sha256']:
            raise ValueError('Immutable release path already contains different bytes: ' + str(out))
        shutil.copy2(source, out)
    (a.site / 'updates').mkdir(exist_ok=True)
    (a.site / 'updates/stable.json').write_text(json.dumps(m, indent=2) + '\n', encoding='utf-8')
    print(f'STAGED ONLY: {len(files)} files at {target}; no deployment performed')


if __name__ == '__main__':
    main()
