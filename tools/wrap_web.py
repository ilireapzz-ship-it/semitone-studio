#!/usr/bin/env python3
"""Wrap the single-file Semitone web app (published as a page fragment) into a complete HTML document
for the plugin's web view. Scripts loaded from a CDN are inlined at build time, so the plugin works
offline (music notation in Read music uses VexFlow). Usage: wrap_web.py <in.html> <out.html>"""
import os, re, sys, urllib.request

src, dst = sys.argv[1], sys.argv[2]
with open(src, encoding="utf-8") as f:
    body = f.read()

def inline(m):
    url = m.group(1)
    cache = os.path.join(os.path.dirname(dst), "cache_" + re.sub(r"[^A-Za-z0-9.]+", "_", url))
    try:
        if os.path.exists(cache):
            js = open(cache, encoding="utf-8").read()
        else:
            with urllib.request.urlopen(url, timeout=60) as r:
                js = r.read().decode("utf-8")
            os.makedirs(os.path.dirname(cache), exist_ok=True)
            open(cache, "w", encoding="utf-8").write(js)
        print(f"inlined {url} ({len(js)//1024} KB)")
        return "<script>" + js.replace("</script", "<\\/script") + "</script>"
    except Exception as e:  # offline build: keep loading it from the CDN
        print(f"warning: could not inline {url}: {e}")
        return m.group(0)

body = re.sub(r'<script src="(https://cdn\.jsdelivr\.net/[^"]+)"></script>', inline, body)

os.makedirs(os.path.dirname(dst), exist_ok=True)
head = ('<!doctype html><html lang="en"><head><meta charset="utf-8">'
        '<meta name="viewport" content="width=device-width,initial-scale=1">'
        '<script>window.SEMITONE_NATIVE=true;</script></head><body>')
with open(dst, "w", encoding="utf-8") as f:
    f.write(head + body + "</body></html>")
print(f"wrapped {len(body)//1024} KB -> {dst}")
