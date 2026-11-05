#!/usr/bin/env python3
"""Scan this computer's plug-ins, FL Studio plug-in database, Serum presets and Splice packs (NAMES ONLY) and
match them to data/sound-library.json. Writes the result locally (default: ~/Library/Application Support/Semitone Studio/);
nothing is uploaded. The desktop app should do the same natively; this script is the reference.
Usage: python3 tools/scan_library.py [--out DIR]"""
import json, os, re, sys, collections
from pathlib import Path
H = Path.home()
ROOTS = {'plugins': ['/Library/Audio/Plug-Ins', str(H / 'Library/Audio/Plug-Ins'), 'C:/Program Files/Common Files/VST3'],
         'fl': [str(H / 'Documents/Image-Line/FL Studio/Presets/Plugin database')],
         'presets': [str(H / 'Splice/presets'), str(H / 'Documents/Xfer/Serum 2 Presets'), str(H / 'Documents/Xfer/Serum Presets')],
         'samples': [str(H / 'Splice/sounds/packs')]}
def ls(p):
    try: return sorted(os.listdir(p))
    except OSError: return []
def scan():
    plugins = set()
    for r in ROOTS['plugins']:
        for sub in ('VST3', 'Components', 'VST', ''):
            for n in ls(os.path.join(r, sub)):
                if re.search(r'\.(vst3|component|vst)$', n): plugins.add(re.sub(r'\.(vst3|component|vst)$', '', n))
    fl = {'Generators': set(), 'Effects': set()}
    for r in ROOTS['fl']:
        for kind in fl:
            for root, _, fs in os.walk(os.path.join(r, kind)):
                fl[kind] |= {f[:-4] for f in fs if f.endswith('.fst')}
    presets = []
    for r in ROOTS['presets']:
        for root, _, fs in os.walk(r):
            presets += [{'pack': os.path.basename(root), 'preset': re.sub(r'\.(SerumPreset|fxp)$', '', f)} for f in fs if f.endswith(('.SerumPreset', '.fxp'))]
    packs = []
    for r in ROOTS['samples']:
        for p in ls(r):
            n = sum(1 for _, _, fs in os.walk(os.path.join(r, p)) for f in fs if f.lower().endswith(('.wav', '.aif', '.aiff', '.flac', '.mp3')))
            packs.append({'pack': p, 'files': n})
    return {'plugins': sorted(plugins | fl['Generators'] | fl['Effects']), 'presets': presets, 'samplePacks': packs}
def main():
    out = Path(sys.argv[sys.argv.index('--out') + 1]) if '--out' in sys.argv else H / 'Library/Application Support/Semitone Studio'
    out.mkdir(parents=True, exist_ok=True)
    inv = scan()
    snd = json.loads((Path(__file__).resolve().parent.parent / 'data/sound-library.json').read_text())
    names = [p.lower() for p in inv['plugins']]
    def owned(sw):  # software strings like "Arturia V Collection: Jun-6 V" or "Xfer Serum"
        hits = []
        for s in sw:
            for token in re.split(r'[:/,()]| or ', s):
                t = token.strip().lower()
                if len(t) > 3 and any(t in n or n in t for n in names): hits.append(s); break
        return hits
    res = {'about': 'Names only, made on this computer. Never uploaded.', 'inventory': inv,
           'instruments': {i['id']: {'owned': owned(i['software'])} for i in snd['instruments']},
           'effects': {e['id']: {'owned': owned(e['software'])} for e in snd['effects']}}
    (out / 'my-sound-library.json').write_text(json.dumps(res, ensure_ascii=False, indent=1))
    print(f"{len(inv['plugins'])} plug-ins, {len(inv['presets'])} presets, {len(inv['samplePacks'])} sample packs -> {out / 'my-sound-library.json'}")
if __name__ == '__main__':
    main()
