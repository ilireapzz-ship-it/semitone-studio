#!/usr/bin/env python3
"""Wrap the single-file Semitone web app (published as a page fragment) into a complete HTML document
for the plugin's web view. Usage: wrap_web.py <in.html> <out.html>"""
import os, sys

src, dst = sys.argv[1], sys.argv[2]
with open(src, encoding="utf-8") as f:
    body = f.read()
os.makedirs(os.path.dirname(dst), exist_ok=True)
head = ('<!doctype html><html lang="en"><head><meta charset="utf-8">'
        '<meta name="viewport" content="width=device-width,initial-scale=1">'
        '<script>window.SEMITONE_NATIVE=true;</script></head><body>')
with open(dst, "w", encoding="utf-8") as f:
    f.write(head + body + "</body></html>")
print(f"wrapped {len(body)//1024} KB -> {dst}")
