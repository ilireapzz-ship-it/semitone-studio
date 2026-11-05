#!/usr/bin/env python3
"""Build each profile's chord-transition weights (the style 'm' tables) from data/progressions.json.

For every section progression, count chord-to-chord moves (looping the last chord back to the first, as
sections loop), separately for major-family keys (ionian/mixolydian/lydian) and minor-family keys
(aeolian/dorian). Writes mMajor, mMinor, modeShare and the main 'm' into data/style-library.json.
"""
import json, re, statistics, sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ROMAN = {'i': 0, 'ii': 1, 'iii': 2, 'iv': 3, 'v': 4, 'vi': 5, 'vii': 6}
NUM = re.compile(r'^(b|#)?(vii|vi|iv|v|iii|ii|i)', re.I)

def family(label, key):
    m = re.search(r'\(([^)]*)\)', label)
    k = (m.group(1) if m else key.split('/')[0]).lower()
    return 'minor' if ('minor' in k or 'dorian' in k or 'aeolian' in k) else 'major'

def degree(tok, fam):
    m = NUM.match(tok)
    if not m: return None
    acc, r = m.group(1), m.group(2).lower()
    d = ROMAN[r]
    if fam == 'major':
        if acc == 'b' and d == 6: return 6          # bVII = mixolydian VII
        if acc == 'b': return None                   # bIII, bVI: borrowed, not in the mode
    return d

def build(prog_list):
    counts = {'major': defaultdict(lambda: defaultdict(float)), 'minor': defaultdict(lambda: defaultdict(float))}
    secs = {'major': 0, 'minor': 0}
    for song in prog_list:
        for label, chords in song['sections'].items():
            fam = family(label, song['key'])
            ds = [degree(t, fam) for t in chords.split()]
            secs[fam] += 1
            n = len(ds)
            if n < 2: continue
            for j in range(n):
                a, b = ds[j], ds[(j + 1) % n]
                if a is None or b is None or a == b: continue
                counts[fam][a][b] += 1.0 / n            # each section weighs the same
    out = {}
    for fam, c in counts.items():
        top = max([w for row in c.values() for w in row.values()] or [1])
        m = {}
        for a in range(7):
            row = {str(b): max(1, round(6 * w / top)) for b, w in sorted(c[a].items(), key=lambda x: -x[1])} if a in c else {}
            m[str(a)] = row or {"0": 1}
        out[fam] = m
    total = secs['major'] + secs['minor']
    share = {k: round(v / total, 2) for k, v in secs.items()} if total else {}
    bpms = [s['bpm'] for s in prog_list if 'bpm' in s]
    return out, share, bpms

def main():
    progs = json.loads((ROOT / 'data/progressions.json').read_text())['profiles']
    libp = ROOT / 'data/style-library.json'
    lib = json.loads(libp.read_text())
    for prof in lib['profiles']:
        pl = progs.get(prof['id'])
        if not pl: continue
        mats, share, bpms = build(pl)
        st = prof['style']
        st['mMajor'], st['mMinor'], st['modeShare'] = mats['major'], mats['minor'], share
        main_fam = 'minor' if share.get('minor', 0) > share.get('major', 0) else 'major'
        st['m'] = mats[main_fam]
        prof['songsAnalysed'] = [{'song': s['song'], **({'album': s['album']} if 'album' in s else {}), 'key': s['key'], **({'bpm': s['bpm']} if 'bpm' in s else {}),
                                  **({'c': s['c']} if 'c' in s else {}), 'src': s['src']} for s in pl]
        print(f"{prof['id']:15} songs {len(pl)}  sections major {share.get('major',0):.2f} minor {share.get('minor',0):.2f}  bpm {sorted(bpms)}  main {main_fam}")
    # the original styles in web/semitone-studio.html: same tables, applied to STYLES by id
    fam_of = {'aeolian': 'minor', 'dorian': 'minor', 'phrygian': 'minor', 'harmonic': 'minor', 'minpent': 'minor', 'locrian': 'minor'}
    current = {}
    html = (ROOT / 'web/semitone-studio.html').read_text()
    for m in re.finditer(r"\{id:'([a-z0-9]+)',meter:'[^']*',comp:'[a-z]+',swing:\d+,name:'[^']*',lane:'[^']*',desc:'[^']*',mode:'([a-z]+)'", html):
        current[m.group(1)] = m.group(2)
    ex = json.loads((ROOT / 'data/progressions.json').read_text()).get('existingStyles', {})
    lib['existingStyles'] = {'about': 'Chord habits for the original styles, rebuilt from many songs. Apply to STYLES in web/semitone-studio.html by id: m (for the style mode), mMajor, mMinor, modeShare. Keep each style mode; modeShare lets a new song switch family.'}
    for sid, pl in ex.items():
        mats, share, bpms = build(pl)
        fam = fam_of.get(current.get(sid, 'ionian'), 'major')
        artists = {}
        for x in pl: artists[x.get('artist', '?')] = artists.get(x.get('artist', '?'), 0) + 1
        lib['existingStyles'][sid] = {'m': mats[fam], 'mMajor': mats['major'], 'mMinor': mats['minor'], 'modeShare': share, 'styleMode': current.get(sid),
                                       'songsAnalysed': len(pl), 'byArtist': artists, 'songs': [x['song'] for x in pl]}
        print(f"{sid:10} mode {current.get(sid,'?'):9} songs {len(pl):2} {artists}  major {share.get('major',0):.2f} minor {share.get('minor',0):.2f}")
    check_catalog(progs)
    libp.write_text(json.dumps(lib, ensure_ascii=False, indent=2) + '\n')

def check_catalog(progs):
    """data/catalog.json lists every song by album with tags; songs marked chords:true must have chord data in their era."""
    cp = ROOT / 'data/catalog.json'
    if not cp.exists(): return
    cat = json.loads(cp.read_text())
    vocab = {t for v in cat['tagVocab'].values() for t in v}
    problems = []
    for artist, a in cat['artists'].items():
        for al in a['albums']:
            names = {s['song'] for s in progs.get(al['era'], [])}
            for s in al['songs']:
                problems += [f"{artist} / {s['title']}: unknown tag '{t}' (add it to tagVocab)" for t in s['tags'] if t not in vocab]
                if s.get('chords') and s['title'] not in names: problems.append(f"{artist} / {s['title']}: chords:true but not in progressions.json '{al['era']}'")
    for p in problems: print('catalog:', p)
    if problems: sys.exit(1)

if __name__ == '__main__':
    main()
