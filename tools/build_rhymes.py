#!/usr/bin/env python3
"""Build data/rhymes.json, the local rhyme dictionary behind the Words tab.
Pronunciations are from CMUdict (npm cmu-pronouncing-dictionary, ISC), kept to common words (npm wordlist-english, SCOWL, MIT).
Each key is the sound from the last stressed vowel onward; each entry is word:stress, for example "leaving:10".
Usage: build_rhymes.py <folder holding both unpacked packages>"""
import json, os, re, sys
src = sys.argv[1]
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
common = set()
for size in (10, 20, 35, 40, 50):
    for kind in ("english", "american"):
        f = os.path.join(src, "wordlist-english-1.2.1", "package", f"{kind}-words-{size}.json")
        if os.path.exists(f):
            common |= {w.lower() for w in json.load(open(f, encoding="utf-8"))}
text = open(os.path.join(src, "cmu-pronouncing-dictionary-3.0.0", "package", "index.js"), encoding="utf-8").read()
groups = {}
for w, ph in re.findall(r'"([a-z\']+)": "([A-Z0-9 ]+)"', text):
    if w not in common or len(w) > 12: continue
    p = ph.split()
    stress = "".join("1" if x[-1] in "12" else "0" for x in p if x[-1].isdigit())
    last = max((i for i, x in enumerate(p) if x[-1] in "12"), default=None)
    if last is None:
        last = next((i for i, x in enumerate(p) if x[-1].isdigit()), 0)
    key = "".join(re.sub(r"\d", "", x) for x in p[last:])
    groups.setdefault(key, []).append(f"{w}:{stress}")
out = {k: " ".join(sorted(v)) for k, v in sorted(groups.items())}
with open(os.path.join(ROOT, "data", "rhymes.json"), "w", encoding="utf-8") as f:
    json.dump({"about": "Rhymes and stress for the Words tab: CMUdict, common words only. tools/build_rhymes.py makes this.", "keys": out}, f, ensure_ascii=False, separators=(",", ":"))
print(len(out), "rhyme sounds", sum(len(v) for v in groups.values()), "words")
