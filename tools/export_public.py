#!/usr/bin/env python3
"""Public source export for AhmxdNYC/Nioh-3-Engine (DRY RUN by default; never pushes).

Same path as v1.7.87 (public deb036c4): the public repo has its own curated history, so a release is one new commit
on public/main that carries the mod SOURCE from the private release branch. Docs, README, release/ scripts,
tools/updater (public has the GitHub fallback 388ba7c8) and everything else stay as the public repo has them.

  python3 tools/export_public.py [--ref ally/release-aob] [--public public/main] [--out DIR] [--allow-soulcore]

1. temp worktree of --public (detached), 2. overlay the code files of the 4 mod folders from --ref,
3. gates: exclusions (binaries / build output / personal paths), public-newer files, soul-core code, versions,
4. commit locally in the temp worktree and print the stat + write the patch. Exit 1 if a gate fails.
Pushing is a separate manual step (printed at the end), only after the human's go.
"""
import argparse, fnmatch, os, re, subprocess, sys, tempfile

EXTRA = ['tools/check_fixed_rvas.py', 'tools/fixed_rvas_allow.txt']  # BossRematch build.bat calls ..\\..\\tools\\check_fixed_rvas.py
MODS = ['mod/Nioh3EngineFix', 'mod/Nioh3Training', 'mod/Nioh3BossRematch', 'mod/Nioh3CamKiProbe']
CODE = ('.c', '.cpp', '.h', '.hpp', '.inl', '.def', '.rc', '.json', '.py', '.bat', '.cmd', '.ps1', '.sh', '.ini',
        '.tsv', '.txt', '.hlsl', '.zig', '.cmake')
SKIP = ['*/build/*', '*/bin/*', '*/.github/*', '*/plug/*', '*.obj', '*.lib', '*.pdb', '*.exp', '*.asi', '*.dll', '*.exe',
        '*/build.log', '*.log', '*/tests/formhide4*', '*/soul_core_tuning_menu.h',  # soul-core dev only
        ]
# same scrub as the 1.7.86 export: the private checkout path becomes the neutral C:\nioh3-engine, home dirs become ~/
SCRUB = [(re.compile(rb'C:(\\\\|\\|/)Users(?:\\\\|\\|/)Ahmad Hamza(?:\\\\|\\|/)nioh3-engine'), rb'C:\1nioh3-engine'),
         (re.compile(rb'/home/bazzite/aob-dump/Nioh3\.vanilla\.bin'), b'Nioh3.dump.bin'),
         (re.compile(rb'/(?:var/)?home/bazzite/'), b'~/'),
         (re.compile(rb'7656119\d{10}'), b'76561190000000000')]  # Steam IDs (PUBLIC_EXCLUSIONS)
# public lines intentionally reworded on the private line (checked by hand, 10-31 #59153)
REWORDED = {'mod/Nioh3EngineFix/src/dllmain.cpp': ['unknown build: each patch is installed only when its own bytes match']}
PERSONAL = re.compile(r'Ahmad Hamza|/home/bazzite|/var/home/|C:\\\\Users\\\\|ahmadhamza', re.I)
# soul-core form code (dev line only; 1.7.87 public had 0 of these)
SOUL = re.compile(r'FormKey|FormHide|FormSlot0|FormFollow|FormPin|SealEnd|SealExit|OrbSkip|FastThrow|AttackCancel|'
                  r'soul_core_tuning|TenshinSetForm|FkAppear|SctAddHubRow')


def strip_soulcore(path, data):
    """Drop `#if BR_SOULCORE` .. matching `#endif` blocks (C/C++) and the [Spirit] / [SealHits] ini sections: the release build compiles
    them out (pc #59173: no soul-core code in the public source)."""
    txt = data.decode('utf-8', 'surrogateescape')
    nl = '\r\n' if '\r\n' in txt else '\n'
    out, depth, cut = [], 0, 0
    if path.endswith(('.c', '.cpp', '.h', '.hpp', '.inl')):
        for line in txt.split(nl):
            t = line.strip()
            if cut:
                if re.match(r'#\s*if', t):
                    depth += 1
                elif re.match(r'#\s*endif', t):
                    depth -= 1
                    if depth == 0:
                        cut = 0
                elif re.match(r'#\s*el', t) and depth == 1:
                    sys.exit(f'{path}: #else/#elif inside #if BR_SOULCORE is not supported')
                continue
            if re.match(r'#\s*if\s+BR_SOULCORE\s*$', t):
                cut, depth = 1, 1
                continue
            out.append(line)
        if cut:
            sys.exit(f'{path}: unterminated #if BR_SOULCORE')
    elif path.endswith('.ini'):
        for line in txt.split(nl):
            t = line.strip()
            if t.startswith('['):
                cut = t.lower() in ('[spirit]', '[sealhits]')
            if not cut:
                out.append(line)
    else:
        return data
    return nl.join(out).encode('utf-8', 'surrogateescape')


def git(*a, cwd=None, check=True):
    r = subprocess.run(['git', *a], cwd=cwd, capture_output=True, text=True, errors='replace')
    if check and r.returncode:
        sys.exit(f'git {" ".join(a)}: {r.stderr.strip()}')
    return r.stdout


def files(ref, *paths):
    return set(git('ls-tree', '-r', '--name-only', ref, '--', *paths).split())


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--ref', default='ally/release-aob')
    p.add_argument('--public', default='public/main')
    p.add_argument('--base', default='1fb1de9d', help='public commit the private line last synced from (1.7.86)')
    p.add_argument('--out', default=None)
    p.add_argument('--version', default='1.7.88')
    p.add_argument('--allow-soulcore', action='store_true', help='accept dormant (compiled-out) soul-core source')
    a = p.parse_args()
    repo = git('rev-parse', '--show-toplevel').strip()
    git('fetch', '-q', 'public', check=False, cwd=repo)
    out = a.out or tempfile.mkdtemp(prefix='pubexport-')
    wt = os.path.join(out, 'tree')
    git('worktree', 'add', '-q', '--detach', wt, a.public, cwd=repo)
    fails = []
    try:
        pub, src = files(a.public, *MODS), files(a.ref, *MODS)
        take = sorted(f for f in src if f.endswith(CODE) and not any(fnmatch.fnmatch(f, s) for s in SKIP))
        take += [f for f in EXTRA if f in files(a.ref, f)]
        pub |= files(a.public, *EXTRA)
        skipped = sorted(src - set(take) - pub)
        # public-newer: files the public repo changed after the sync base; the ref must already contain those lines
        newer = [f for f in git('diff', '--name-only', a.base, a.public, '--', *MODS, cwd=repo).split() if f in take]
        for f in newer:
            added = [l[1:] for l in git('diff', a.base, a.public, '--', f, cwd=repo).splitlines()
                     if l.startswith('+') and not l.startswith('+++') and l[1:].strip()]
            body = git('show', f'{a.ref}:{f}', cwd=repo)
            miss = [l for l in added if l not in body and 'VERSION' not in l  # version bumps are expected
                    and not any(r in l for r in REWORDED.get(f, []))]
            if miss:
                fails.append(f'public-newer {f}: {len(miss)} public line(s) missing in {a.ref}, e.g. {miss[0].strip()[:80]}')
        soul, personal, vers = {}, [], []
        for f in take:
            data = subprocess.run(['git', 'show', f'{a.ref}:{f}'], cwd=repo, capture_output=True).stdout
            data = strip_soulcore(f, data)
            for rx, rep in SCRUB:
                data = rx.sub(rep, data)
            if a.version in data.decode('utf-8', 'replace') and 'VERSION' in data.decode('utf-8', 'replace'):
                vers.append(f)
            dst = os.path.join(wt, f)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            with open(dst, 'wb') as fh:
                fh.write(data)
            txt = data.decode('utf-8', 'replace')
            n = len(SOUL.findall(txt))
            if n:
                soul[f] = n
            old = set(git('show', f'{a.public}:{f}', cwd=repo, check=False).splitlines()) if f in pub else set()
            for i, line in enumerate(txt.splitlines(), 1):
                if PERSONAL.search(line) and line not in old:  # lines public already has are not new leaks
                    personal.append(f'{f}:{i}: {line.strip()[:100]}')
        git('add', '-A', '--', *MODS, *[f for f in EXTRA if f in take], cwd=wt)
        stat = git('diff', '--cached', '--stat', cwd=wt)
        big = [l for l in git('diff', '--cached', '--numstat', cwd=wt).splitlines() if l.startswith('-\t')]
        print(f'== export {a.ref} -> {a.public} (temp worktree {wt})')
        print(stat.rstrip().splitlines()[-1] if stat.strip() else 'no changes')
        print(f'files taken {len(take)}, new in public: {len(set(take) - pub)}, not exported (build output/dev): {len(skipped)}')
        for f in skipped:
            print('   skip', f)
        print(f'version {a.version} found in:', ', '.join(vers) or 'NONE')
        if not vers:
            fails.append(f'version {a.version} not found next to a VERSION name')
        if big:
            fails.append(f'{len(big)} binary file(s) in the export: ' + ', '.join(l.split('\t')[2] for l in big))
        if personal:
            fails.append(f'{len(personal)} personal path line(s): ' + '; '.join(personal[:3]))
        if soul:
            tot = sum(soul.values())
            msg = f'soul-core code present ({tot} markers; public 1.7.87 had 0): ' + ', '.join(
                f'{k.split("/")[-1]} {v}' for k, v in sorted(soul.items(), key=lambda kv: -kv[1])[:6])
            (print('WARN', msg, '(allowed)') if a.allow_soulcore else fails.append(msg))
        git('-c', 'user.name=AhmxdNYC', '-c', 'user.email=104233015+AhmxdNYC@users.noreply.github.com', 'commit', '-q',
            '-m', f'Find every game address by its own bytes in all mods (version {a.version})', cwd=wt, check=False)
        patch = os.path.join(out, f'public-export-{a.version}.patch')
        with open(patch, 'w') as fh:
            fh.write(git('format-patch', '--stdout', '-1', 'HEAD', cwd=wt))
        print('patch:', patch)
        print('\n'.join('FAIL ' + f for f in fails) or 'GATES OK')
        print(f'publish (manual, after go): git -C {wt} push public HEAD:main   # then gh release create --target main')
    finally:
        if fails:
            git('worktree', 'remove', '--force', wt, cwd=repo, check=False)
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
