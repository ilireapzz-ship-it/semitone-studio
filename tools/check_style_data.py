#!/usr/bin/env python3
"""Consistency check for data/*.json (style library, progressions, sound library, catalog). Run from the repo root:
python3 tools/check_style_data.py. Prints ISSUES (must be 0) and WARNINGS (review)."""
import json, re, sys
sys.path.insert(0,'tools'); import build_style_matrices as B
L=json.load(open('data/style-library.json')); P=json.load(open('data/progressions.json')); S=json.load(open('data/sound-library.json')); C=json.load(open('data/catalog.json'))
html=open('web/semitone-studio.html').read()
INSTR=set(re.findall(r"([a-z0-9]+):\{name:'", html[html.index('const INSTR={'):html.index('const INSTR={')+6000]))
def keys(name):
    i=html.find('const '+name+'=');  return set(re.findall(r"'([a-z0-9]+)'", html[i:i+400])) if i>=0 else set()
GROOVES=keys('STYLE_GROOVE')|set(re.findall(r"\n\s*([a-z0-9]+):\{name:'", html[html.index('const GROOVES={'):html.index('const GROOVES={')+4000])); KITS=keys('STYLE_KIT'); BASS=keys('STYLE_BASS')|set(re.findall(r"kind==='([a-z]+)'", html)); QUIRKS=keys('STYLE_QUIRKS')
issues=[]; warn=[]
def I(x): issues.append(x)
def W(x): warn.append(x)
req=['id','kind','tags','style','bass','groove','kit','instr','quirks','feel','sug','trans','art','arrangement','mixHint','dna']
sid={i['id'] for i in S['instruments']}; fid={e['id'] for e in S['effects']}; mid={m['move'] for m in S['automationMoves']}
for p in L['profiles']:
    for k in req:
        if k not in p: I(f"profile {p['id']}: missing {k}")
    st=p['style']
    if p['id'] in P['profiles'] and not st.get('mMajor'): I(f"{p['id']}: no matrices built")
    br=st.get('bpmRange')
    if br and not (br[0]<=st['bpm']<=br[1]): I(f"{p['id']}: bpm {st['bpm']} outside bpmRange {br}")
    ms=st.get('modeShare',{})
    fam='minor' if st['mode'] in ('aeolian','dorian','phrygian','harmonic','locrian') else 'major'
    if ms and ms.get(fam,0)<0.3: W(f"{p['id']}: style mode {st['mode']} ({fam}) but only {ms.get(fam,0)} of its sections are {fam}")
    for role,v in p['instr'].items():
        if v.startswith('i:') and v[2:] not in INSTR: I(f"{p['id']}: instrument {v} not in the app")
    if p['groove'] not in GROOVES: W(f"{p['id']}: groove '{p['groove'][:40]}' not an existing groove (new or stand-in)")
    if p['kit'] not in KITS: W(f"{p['id']}: kit '{p['kit']}' not an existing kit")
    if p['bass'] not in BASS: W(f"{p['id']}: bass '{p['bass']}' not an existing bass style")
    if p['quirks'] not in QUIRKS: W(f"{p['id']}: quirks '{p['quirks']}' not existing")
    for s in p.get('sounds',[]):
        if s not in sid: I(f"{p['id']}: sound '{s}' not in sound-library")
    for pt in p['dna'].get('points',[]):
        if pt.get('c') in ('high','med') and not pt.get('src'): W(f"{p['id']}: a {pt['c']}-confidence DNA point has no source")
    n=len(p.get('songsAnalysed',[]))
    stat=dict(p['dna'].get('stats',[]))
    for k,v in stat.items():
        if ('analysed' in k.lower()) and v.isdigit() and int(v)!=n and n: I(f"{p['id']}: DNA says {v} analysed, data has {n}")
# numerals
for pid,pl in list(P['profiles'].items())+list(P['existingStyles'].items()):
    for s in pl:
        for lab,ch in s['sections'].items():
            fam=B.family(lab,s['key'])
            toks=ch.split(); bad=[t for t in toks if B.NUM.match(t) is None]
            if bad: I(f"{pid}/{s['song']}/{lab}: unreadable chords {bad}")
            drop=[t for t in toks if B.NUM.match(t) and B.degree(t,fam) is None]
            if drop: W(f"{pid}/{s['song']}/{lab}: {drop} ignored in {fam} matrix (borrowed)")
            if len(toks)<2: W(f"{pid}/{s['song']}/{lab}: one chord only")
        if not s.get('src'): I(f"{pid}/{s['song']}: no source")
    names=[s['song'] for s in pl]
    d={n for n in names if names.count(n)>1}
    if d: I(f"{pid}: duplicate songs {d}")
# producers
for pr in L['producers']:
    for k in ('pass','fxStyle','layer','dna'):
        if k not in pr: I(f"producer {pr['id']}: missing {k}")
    for m in pr['pass']['moves']:
        if not m.get('src'): I(f"{pr['id']}/{m['id']}: no source")
        if m['level'] not in ('sound','mix','arrangement','structure','writing'): I(f"{pr['id']}/{m['id']}: bad level {m['level']}")
        if m['level']=='writing' and m.get('onByDefault'): I(f"{pr['id']}/{m['id']}: writing move on by default")
    for s in pr['fxStyle']['sounds']:
        if s not in sid: I(f"{pr['id']}: fx sound {s} missing")
# sound library
for e,r in S['emotionRules']['emotions'].items():
    for m in r['use']+r['avoid']:
        if m not in mid and m not in fid: I(f"emotion {e}: move '{m}' undefined")
    if set(r['use'])&set(r['avoid']): I(f"emotion {e}: same move in use and avoid")
cat_em=set(C['tagVocab']['emotion'])
for v,d in S['vibes']['tags'].items():
    for s in d['sounds']:
        if s not in sid: I(f"vibe {v}: sound {s} missing")
    for f in d['fx']:
        if f not in fid: I(f"vibe {v}: fx {f} missing")
    for e in d['emotions']:
        if e not in S['emotionRules']['emotions']: I(f"vibe {v}: emotion '{e}' has no rule")
for e in S['emotionRules']['emotions']:
    if e not in cat_em: W(f"emotion rule '{e}' not in catalog emotion tags")
for i in S['instruments']:
    for u in i['usedBy']:
        if not u['src']: I(f"{i['id']}: usedBy without source")
# catalog
for a in C['artists'].values():
    for al in a['albums']:
        for v in al.get('vibes',[]):
            if v not in C['tagVocab']['vibe']: I(f"album {al['album']}: vibe {v} not in vocab")
        if al['era'] not in [p['id'] for p in L['profiles']]+[o['id'] for o in L['orchestral']]: I(f"album {al['album']}: era profile {al['era']} missing")
for k,v in L.get('artistEras',{}).items():
    if k=='about': continue
    for e in v['eras']:
        if e not in [p['id'] for p in L['profiles']]: I(f"artistEras {k}: {e} missing")
# part roles (data/part-roles.json)
R=json.load(open('data/part-roles.json'))
pids=[p['id'] for p in L['profiles']]; prids=[p['id'] for p in L['producers']]
for k,v in R['styleParts'].items():
    if k not in pids: I(f"part-roles styleParts: {k} is not a profile")
    for x in v['extras']:
        if x not in R['roles']: I(f"part-roles styleParts {k}: role {x} unknown")
for k in pids:
    if k not in R['styleParts']: W(f"part-roles: profile {k} has no styleParts")
for k in R['producerParts']:
    if k not in prids: I(f"part-roles producerParts: {k} is not a producer")
import re as _re
for k,v in list(R['styleParts'].items())+list(R['producerParts'].items()):
    for m in v['moves']:
        g=_re.match(r'(add|drop|thin):(\w+)',m)
        if g and g.group(2) not in R['roles']: I(f"part-roles {k}: move {m} uses unknown role")
        if not _re.match(r'(add|drop|thin):\w+(:[\w-]+)?@[\w,-]+$|layer:\w+:[\w:-]+@[\w,-]+$',m): I(f"part-roles {k}: move {m} doesn't match the grammar")
for e in R['emotion']:
    if e not in S['emotionRules']['emotions']: W(f"part-roles emotion {e} has no emotion rule")
print('ISSUES',len(issues)); [print(' -',x) for x in issues]
print('WARNINGS',len(warn)); [print(' -',x) for x in warn]
sys.exit(1 if issues else 0)
