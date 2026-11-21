#!/usr/bin/env python3
"""Turn data/kickfit-targets.json (measured by tools/kick_analyse.py) into kickfit/KickFitTargets.h for the plug-in.
Run it after the data changes; `--check` fails when the header is out of date (CI runs that).
A genre without enough measured songs gets measured=false and no numbers: the plug-in then says so instead of guessing."""
import json, os, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NAMES = [('trap-hiphop', 'Trap / hip-hop'), ('pop', 'Pop'), ('rnb', 'R&B'), ('house-edm', 'House / EDM'),
         ('dnb', 'Drum & bass'), ('rock', 'Rock'), ('afro', 'Afrobeats / amapiano'), ('lofi', 'Lo-fi')]


def build():
    d = json.load(open(os.path.join(REPO, 'data', 'kickfit-targets.json'), encoding='utf-8'))
    rows = []
    for mid, name in NAMES:
        m = d['modes'].get(mid, {})
        ok = m.get('status') == 'measured' and m.get('kick_low_vs_mix_db')
        sit = m['kick_low_vs_mix_db'] if ok else {'p25': 0, 'median': 0, 'p75': 0}
        note = (m.get('kick_f0') or {}) if ok else {}
        if note.get('n', 0) < d['min_songs_per_mode']:
            note = {}                                  # too few songs with a steady kick note: no zone drawn
        dip = (m.get('bass_dip_db') or {}).get('median', 0) if ok else 0
        rows.append('    { "%s", "%s", %d, %s, %.2ff, %.2ff, %.2ff, %.1ff, %.1ff, %.1ff, %.2ff },' % (
            mid, name, m.get('songs', 0), 'true' if ok else 'false', sit['p25'], sit['median'], sit['p75'],
            note.get('p25', 0), note.get('median', 0), note.get('p75', 0), dip))
    ks = d['kickSamples']['all']
    return '''#pragma once
// Made by tools/kickfit_targets.py from data/kickfit-targets.json (measured %s). Do not edit by hand.
namespace kf
{
struct ModeTarget
{
    const char* id; const char* name; int songs; bool measured;
    float sitLo, sitMid, sitHi;        // kick lows (30-150 Hz) against the whole mix at kick hits, dB: quarter, middle, three quarters of the songs
    float noteLo, noteMid, noteHi;     // the kick's boom note in those songs, Hz (0 = too few songs with a steady note)
    float dipDb;                       // how far the bass's lows drop under the kick in those songs, dB (middle)
};
inline constexpr ModeTarget modeTargets[] = {
%s
};
inline constexpr int numModes = %d;
// from %d kick samples: what is left of a kick above 180 Hz (what a phone plays), dB against the whole hit
inline constexpr float kickAbove180P10 = %.2ff, kickAbove180Median = %.2ff;
}
''' % (d['made'], '\n'.join(rows), len(NAMES), d['kickSamples']['confirmed'], ks['above180_db']['p10'], ks['above180_db']['median'])


if __name__ == '__main__':
    dst = os.path.join(REPO, 'kickfit', 'KickFitTargets.h')
    txt = build()
    if '--check' in sys.argv:
        cur = open(dst, encoding='utf-8').read().replace('\r\n', '\n') if os.path.exists(dst) else ''
        if cur != txt:
            print('kickfit/KickFitTargets.h is out of date: run tools/kickfit_targets.py'); sys.exit(1)
        print('kickfit targets header is up to date')
    else:
        open(dst, 'w', encoding='utf-8', newline='\n').write(txt); print('wrote', dst)
