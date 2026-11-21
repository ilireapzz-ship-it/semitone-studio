#!/usr/bin/env python3
"""Turn data/kickfit-targets.json (measured by tools/kick_analyse.py) into kickfit/KickFitTargets.h for the plug-in.
Run it after the data changes; `--check` fails when the header is out of date (CI runs that).
A genre without enough measured songs gets measured=false and no numbers: the plug-in then says so instead of guessing."""
import json, os, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NAMES = [('trap-hiphop', 'Trap / hip-hop'), ('pop', 'Pop'), ('rnb', 'R&B'), ('house-edm', 'House / EDM'),
         ('dnb', 'Drum & bass'), ('rock', 'Rock'), ('afro', 'Afrobeats / amapiano'), ('lofi', 'Lo-fi')]


def sit_range(m):
    """Where the kick's lows sit against all the lows: from the whole-mix measure and the kick+bass-only measure together
    (low edge = the lower quarter mark, high edge = the higher three-quarter mark, middle = between the two middles)."""
    a, b = m['kick_low_vs_lows_db'], m['kick_low_vs_kickbass_lows_db']
    return {'p25': min(a['p25'], b['p25']), 'median': (a['median'] + b['median']) / 2, 'p75': max(a['p75'], b['p75'])}


def build():
    d = json.load(open(os.path.join(REPO, 'data', 'kickfit-targets.json'), encoding='utf-8'))
    rows = []
    for mid, name in NAMES:
        m = d['modes'].get(mid, {})
        ok = m.get('status') == 'measured' and m.get('kick_low_vs_lows_db') and m.get('kick_low_vs_kickbass_lows_db')
        sit = sit_range(m) if ok else {'p25': 0, 'median': 0, 'p75': 0}
        note = (m.get('kick_f0') or {}) if ok else {}
        if note.get('n', 0) < d['min_songs_per_mode']:
            note = {}                                  # too few songs with a steady kick note: no zone drawn
        dip = (m.get('bass_dip_db') or {}).get('median', 0) if ok else 0
        voc = (m.get('vocal_vs_backing_db') or {}) if ok else {}
        pk = [(m.get(k) or {}) if ok else {} for k in ('bass_peak_vs_kick_db', 'vocal_peak_vs_kick_db')]
        pk_ok = all(q.get('n', 0) >= d['min_songs_per_mode'] for q in pk)
        rows.append('    { "%s", "%s", %d, %s, %.2ff, %.2ff, %.2ff, %.1ff, %.1ff, %.1ff, %.2ff, %s, %.2ff, %.2ff, %.2ff, %s, %s },' % (
            mid, name, m.get('songs', 0), 'true' if ok else 'false', sit['p25'], sit['median'], sit['p75'],
            note.get('p25', 0), note.get('median', 0), note.get('p75', 0), dip,
            'true' if voc.get('n', 0) >= d['min_songs_per_mode'] else 'false', voc.get('p25', 0), voc.get('median', 0), voc.get('p75', 0),
            'true' if pk_ok else 'false', ', '.join('{ %.2ff, %.2ff, %.2ff }' % (q.get('p25', 0), q.get('median', 0), q.get('p75', 0)) if pk_ok else '{ 0, 0, 0 }' for q in pk)))
    ks = d['kickSamples']['all']
    return '''#pragma once
// Made by tools/kickfit_targets.py from data/kickfit-targets.json (measured %s). Do not edit by hand.
namespace kf
{
struct ModeTarget
{
    const char* id; const char* name; int songs; bool measured;
    float sitLo, sitMid, sitHi;        // kick lows (30-150 Hz) against everything under 300 Hz at kick hits, dB: low edge, middle, high edge (see sit_range)
    float noteLo, noteMid, noteHi;     // the kick's boom note in those songs, Hz (0 = too few songs with a steady note)
    float dipDb;                       // how far the bass's lows drop under the kick in those songs, dB (middle)
    bool vocal; float vocLo, vocMid, vocHi;   // the singing against everything else (K-weighted), dB: quarter, middle, three quarters of the songs
    bool peaks; float bassVsKick[3], vocVsKick[3];   // the bass's / the vocal's usual peak against the kick's, dB, song by song (quarter, middle, three quarters); parts split by Demucs
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
