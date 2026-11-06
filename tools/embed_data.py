#!/usr/bin/env python3
"""Copy data/style-library.json, catalog.json and progressions.json into web/semitone-studio.html, so the
interface stays one self-contained file. The data/ files stay the one source of truth: edit those, then run this.
Usage: embed_data.py          update the HTML
       embed_data.py --check  fail if the HTML is out of date (build-mac.sh and CI run this)"""
import base64, json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HTML = os.path.join(ROOT, "web", "semitone-studio.html")
FILES = {"styleLibrary": "style-library.json", "styleCatalog": "catalog.json", "styleProgressions": "progressions.json", "soundLibrary": "sound-library.json", "pluginMaps": "plugin-maps.json", "partRoles": "part-roles.json", "songShapes": "song-shapes.json", "coach": "coach.json", "rhymes": "rhymes.json"}

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

# Themes: docs/design/merged/themes.json becomes one block of --st-* variables per theme, plus the bundled fonts.
def kebab(k):
    return re.sub(r"[A-Z]", lambda m: "-" + m.group(0).lower(), k)
UI = {"Inter Tight": "'Inter Tight',", "Inter": "Inter,", "Inter (SF Pro on Mac)": "-apple-system,BlinkMacSystemFont,Inter,", "Space Grotesk": "'Space Grotesk',"}
DISPLAY = {"Instrument Serif": ("'Instrument Serif',Georgia,serif", 400), "Inter 600": ("Inter,system-ui,sans-serif", 600), "Fraunces": ("Fraunces,Georgia,serif", 600),
           "Inter 700": ("-apple-system,BlinkMacSystemFont,Inter,system-ui,sans-serif", 700), "DM Serif Display": ("'DM Serif Display',Georgia,serif", 400)}
MONO = {"JetBrains Mono": "'JetBrains Mono',", "IBM Plex Mono": "'IBM Plex Mono',", "JetBrains Mono (SF Mono on Mac)": "ui-monospace,'SF Mono','JetBrains Mono',", "Space Mono": "'Space Mono',"}
def theme_css():
    with open(os.path.join(ROOT, "docs", "design", "merged", "themes.json"), encoding="utf-8") as f:
        T = json.load(f)
    out = []
    for tid, t in T["themes"].items():
        v = {}
        for k in T["tokens"]:
            if k in ("button", "light", "fontUi", "fontDisplay", "fontMono"): continue
            x = t[k]
            if k in ("radius", "radiusLg"): x = f"{x}px"
            if k == "noteOnClip" and x == "track": x = "var(--c,currentColor)"
            if k == "bgExtra" and not x: x = "none"
            v[kebab(k)] = x
        v["clip-pct"] = f"{round(t['clipAlpha'] * 100, 1)}%"
        v["switch-on"] = t.get("switchOn", t["accent"])
        v["highlight"] = t.get("highlight", t["accent"])
        v["font-ui"] = UI[t["fontUi"]] + "system-ui,sans-serif"
        v["font-display"], v["display-weight"] = DISPLAY[t["fontDisplay"]]
        v["font-mono"] = MONO[t["fontMono"]] + "ui-monospace,Menlo,Consolas,monospace"
        sel = f':root[data-theme="{tid}"]'
        if tid == T["default"]: sel = ":root," + sel
        body = ";".join(f"--st-{k}:{x}" for k, x in v.items())
        out.append(f"{sel}{{{body};color-scheme:{'light' if t['light'] else 'dark'}}}")
    return '<style id="stThemes">' + "\n".join(out) + "</style>"
def font_css():
    names = {"inter-tight": "Inter Tight", "instrument-serif": "Instrument Serif", "jetbrains-mono": "JetBrains Mono", "inter": "Inter", "fraunces": "Fraunces",
             "ibm-plex-mono": "IBM Plex Mono", "space-grotesk": "Space Grotesk", "dm-serif-display": "DM Serif Display", "space-mono": "Space Mono"}
    d = os.path.join(ROOT, "data", "fonts"); out = []
    for fn in sorted(os.listdir(d)):
        m = re.match(r"(.+)-latin-(\d+)-normal\.woff2$", fn)
        if not m: continue
        with open(os.path.join(d, fn), "rb") as f:
            b = base64.b64encode(f.read()).decode()
        out.append(f"@font-face{{font-family:'{names[m.group(1)]}';font-weight:{m.group(2)};font-style:normal;font-display:swap;src:url(data:font/woff2;base64,{b}) format('woff2')}}")
    return '<style id="stFonts">' + "".join(out) + "</style>"
with open(os.path.join(ROOT, "docs", "design", "merged", "themes.json"), encoding="utf-8") as f:
    themes_js = json.dumps(json.load(f)["themes"], ensure_ascii=False, separators=(",", ":")).replace("</", "<\\/")
for el, b in (("stThemes", theme_css()), ("stFonts", font_css())):
    pat = re.compile(r'<style id="' + el + r'">.*?</style>', re.S)
    if pat.search(new):
        new = pat.sub(lambda _: b, new)
    else:
        assert new.count("<style>") >= 1
        new = new.replace("<style>", b + "\n<style>", 1)
tb = f'<script type="application/json" id="themeData">{themes_js}</script>'
pat = re.compile(r'<script type="application/json" id="themeData">.*?</script>', re.S)
if pat.search(new):
    new = pat.sub(lambda _: tb, new)
else:
    anchor = '<script src="https://cdn.jsdelivr.net/npm/vexflow'
    new = new.replace(anchor, tb + "\n" + anchor)

if "--check" in sys.argv:
    if new != html:
        sys.exit("web/semitone-studio.html is out of date with data/: run python3 tools/embed_data.py")
    print("embedded data is up to date")
elif new != html:
    with open(HTML, "w", encoding="utf-8") as f:
        f.write(new)
    print("embedded data updated")
