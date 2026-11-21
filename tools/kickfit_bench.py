#!/usr/bin/env python3
"""Kick Fit test bench: run a large set of real kicks through each genre mode against backing from real songs of
that genre, and count how many moved toward the target and how many got worse.

  python tools/kickfit_bench.py prepare        cut a 12 s backing (everything but the drums) from a few songs per genre
  python tools/kickfit_bench.py run [--limit N] put the kicks through Kick Fit and write docs/kickfit/bench-summary.json

Needs tools/kick_analyse.py to have measured kicks and songs first, and build-local/kickfit_run(.exe) built from
tools/kickfit_run.cpp. Sounds and names stay in the local folder; the summary holds counts only."""
import sys, os, json, math, random, subprocess, argparse, tempfile
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kick_analyse as ka

SR = 44100
BENCH = os.path.join(ka.LOCAL, 'bench')
RUN = os.path.join(ka.REPO, 'build-local', 'kickfit_run.exe' if os.name == 'nt' else 'kickfit_run')
PER_MODE = 6


def targets():
    return json.load(open(os.path.join(ka.REPO, 'data', 'kickfit-targets.json'), encoding='utf-8'))


def cmd_prepare(args):
    import torch
    from demucs.pretrained import get_model
    from demucs.apply import apply_model
    os.makedirs(BENCH, exist_ok=True)
    songs = [s for s in ka.read_local('songs.jsonl') if s.get('kicks', 0) >= 16 and s.get('mode')]
    measured = [m for m, d in targets()['modes'].items() if d.get('status') == 'measured']
    dev = 'cuda' if torch.cuda.is_available() else 'cpu'
    model = get_model('htdemucs'); model.to(dev).eval()
    meta = []
    for m in measured:
        pick = sorted([s for s in songs if s['mode'] == m], key=lambda s: -(s.get('key_conf') or 0))[:PER_MODE]
        for i, s in enumerate(pick):
            x, sr = ka.load_audio(s['path'], SR)
            a = int(len(x) * 0.4); x = x[a:a + SR * 16]
            w = torch.from_numpy(x.T.copy()); ref = w.mean(0); mu, sd = ref.mean(), ref.std() + 1e-8
            with torch.no_grad():
                est = apply_model(model, ((w - mu) / sd)[None], device=dev, split=True, overlap=0.25, progress=False)[0]
            est = (est * sd + mu).cpu().numpy()
            back = sum(est[k] for k, nm in enumerate(model.sources) if nm != 'drums').mean(axis=0)[SR * 2:SR * 14]
            fn = os.path.join(BENCH, f'{m}_{i}.f32'); back.astype('<f4').tofile(fn)
            meta.append({'mode': m, 'file': fn, 'key_pc': s['key_pc'], 'key_mode': s['key_mode'], 'key_conf': s.get('key_conf')})
    json.dump(meta, open(os.path.join(BENCH, 'meta.json'), 'w'))
    print(len(meta), 'backings in', BENCH)


def tune_to_key(hz, tonic, minor):
    scale = [0, 2, 3, 5, 7, 8, 10] if minor else [0, 2, 4, 5, 7, 9, 11]
    midi = 69 + 12 * math.log2(hz / 440.0)
    best = 99.0
    for o in (-1, 0, 1):
        for d in scale:
            move = 12 * (math.floor(midi / 12) + o) + (tonic + d) % 12 - midi
            if abs(move) < abs(best) - 1e-9 or (abs(abs(move) - abs(best)) < 1e-9 and move < best):
                best = move
    return best


def run(kick, side, out, *, tune=0, thump=0, punch=0, click=0, tail=0, level=0, sit=None):
    cmd = [RUN, kick, side or '-', out, str(SR)] + [str(v) for v in (tune, thump, punch, click, tail, level)] + ([str(v) for v in sit] if sit else [])
    return json.loads(subprocess.run(cmd, capture_output=True, text=True, check=True).stdout)


def dist(v, lo, hi):
    return 0.0 if lo <= v <= hi else (lo - v if v < lo else v - hi)


def cmd_run(args):
    T = targets(); meta = json.load(open(os.path.join(BENCH, 'meta.json')))
    kicks = [k for k in ka.read_local('kicks.jsonl') if not k.get('reject') and k.get('f0')]
    random.Random(5).shuffle(kicks)
    if args.limit: kicks = kicks[:args.limit]
    modes = sorted({b['mode'] for b in meta})
    S = {m: dict(kicks=0, could_not_measure=0, already_in_range=0, left_alone=0, moved_into_range=0, moved_closer=0, got_worse=0, changes_db=[],
                 tune_tried=0, tune_in_key=0, tune_off_cents=[], steady_tune_tried=0, steady_tune_in_key=0, no_single_note=0) for m in modes}
    phone = dict(tried=0, reached=0, before=[], after=[]); peak_over = 0
    tmp = tempfile.mkdtemp(prefix='kickfit')
    kf_, of_, o2 = (os.path.join(tmp, n) for n in ('k.f32', 'o.f32', 'o2.f32'))
    p10 = T['kickSamples']['all']['above180_db']['p10']
    for n, k in enumerate(kicks):
        try:
            x, _ = ka.load_audio(k['path'], SR)
        except Exception:
            continue
        hit = x.mean(axis=1)[:SR // 2]
        track = np.zeros(SR * 12, dtype='<f4')
        for t in range(1, 24):
            a = t * SR // 2; track[a:a + len(hit)] += hit[:len(track) - a]
        track.tofile(kf_)
        for m in modes:
            b = [q for q in meta if q['mode'] == m]; b = b[n % len(b)]
            d = T['modes'][m]['kick_low_vs_mix_db']; lo, mid, hi = d['p25'], d['median'], d['p75']
            r = run(kf_, b['file'], of_, sit=(lo, mid, hi)); s = S[m]; s['kicks'] += 1
            peak_over += r['peak'] > max(1.0, float(np.max(np.abs(track)))) + 1e-3      # louder than full scale and louder than it came in
            if not r['measured']: s['could_not_measure'] += 1; continue
            d0, d1 = dist(r['before'], lo, hi), dist(r['after'], lo, hi)
            if d0 == 0:
                s['already_in_range'] += 1; s['left_alone'] += r['change'] == 0
            elif d1 == 0: s['moved_into_range'] += 1
            elif d1 < d0 - 0.05: s['moved_closer'] += 1
            else: s['got_worse'] += d1 > d0 + 0.05
            if d0: s['changes_db'].append(r['change'])
            # tune to the song's key, then read the note of what comes out
            if r['note'] > 0 and (b.get('key_conf') or 0) >= 0.05:
                move = tune_to_key(r['note'], b['key_pc'], b['key_mode'] == 'min')
                if abs(move) >= 0.15:
                    run(kf_, None, of_, tune=move)
                    r2 = run(of_, None, o2)
                    if r2['note'] > 0:
                        off = abs(tune_to_key(r2['note'], b['key_pc'], b['key_mode'] == 'min')) * 100
                        s['tune_tried'] += 1; s['tune_in_key'] += off < 20; s['tune_off_cents'].append(off)
                        if r['spread'] <= 40: s['steady_tune_tried'] += 1; s['steady_tune_in_key'] += off < 20
                        else: s['no_single_note'] += 1
        # phone check: kicks with less above 180 Hz than nine in ten of the others, with Click all the way up
        if k['above180_db'] < p10:
            run(kf_, None, of_, click=1)
            y = np.fromfile(of_, dtype='<f4')[SR * 4:SR * 5][:, None]; r3, _ = ka.measure_kick(y[:SR // 2], SR)
            if r3 and 'above180_db' in r3:
                phone['tried'] += 1; phone['reached'] += r3['above180_db'] >= p10; phone['before'].append(k['above180_db']); phone['after'].append(r3['above180_db'])
        if (n + 1) % 100 == 0: print(f'  {n + 1}/{len(kicks)}', flush=True)
    out = {'about': 'Kick Fit test bench (tools/kickfit_bench.py): real kicks through each measured genre mode against backing cut from real songs of that genre. Counts only.',
           'made': __import__('time').strftime('%Y-%m-%d'), 'kicks': len(kicks), 'output_over_full_scale_when_input_was_not': int(peak_over), 'modes': {}}
    for m, s in S.items():
        out['modes'][m] = {**{k: v for k, v in s.items() if not isinstance(v, list)}, 'change_db': ka.stat(s['changes_db']), 'tune_off_cents': ka.stat(s['tune_off_cents'], 1)}
    out['phone'] = {'what': f'kicks with less than {p10} dB left above 180 Hz, Click at 100%: how many reach {p10} dB', 'tried': phone['tried'], 'reached': phone['reached'],
                    'before_db': ka.stat(phone['before']), 'after_db': ka.stat(phone['after'])}
    dst = os.path.join(ka.REPO, 'docs', 'kickfit', 'bench-summary.json'); os.makedirs(os.path.dirname(dst), exist_ok=True)
    json.dump(out, open(dst, 'w', encoding='utf-8'), indent=1); print(json.dumps(out, indent=1))


if __name__ == '__main__':
    ap = argparse.ArgumentParser(); ap.add_argument('cmd', choices=['prepare', 'run']); ap.add_argument('--limit', type=int, default=0)
    a = ap.parse_args(); {'prepare': cmd_prepare, 'run': cmd_run}[a.cmd](a)
