#!/usr/bin/env python3
"""Copy data/style-library.json, catalog.json and progressions.json into web/semitone-studio.html, so the
interface stays one self-contained file. The data/ files stay the one source of truth: edit those, then run this.
Usage: embed_data.py          update the HTML
       embed_data.py --check  fail if the HTML is out of date (build-mac.sh and CI run this)"""
import json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HTML = os.path.join(ROOT, "web", "semitone-studio.html")
FILES = {"styleLibrary": "style-library.json", "styleCatalog": "catalog.json", "styleProgressions": "progressions.json", "soundLibrary": "sound-library.json", "pluginMaps": "plugin-maps.json", "partRoles": "part-roles.json", "songShapes": "song-shapes.json", "coach": "coach.json"}

def block(el, name):
    with open(os.path.join(ROOT, "data", name), encoding="utf-8") as f:
        js = json.dumps(json.load(f), ensure_ascii=False, separators=(",", ":")).replace("</", "<\\/")
    return f'<script type="application/json" id="{el}">{js}</script>'

with open(HTML, encoding="utf-8") as f:
    html = f.read()
new = html
for el, name in FILES.items():
    pat = re.compile(r'<script type="application/json" id="' + el + r'">.*?</script>', re.S)
    b = block(el, name)
    if pat.search(new):
        new = pat.sub(lambda _: b, new)
    else:
        anchor = '<script src="https://cdn.jsdelivr.net/npm/vexflow'
        assert new.count(anchor) == 1
        new = new.replace(anchor, b + "\n" + anchor)
if "--check" in sys.argv:
    if new != html:
        sys.exit("web/semitone-studio.html is out of date with data/: run python3 tools/embed_data.py")
    print("embedded data is up to date")
elif new != html:
    with open(HTML, "w", encoding="utf-8") as f:
        f.write(new)
    print("embedded data updated")
