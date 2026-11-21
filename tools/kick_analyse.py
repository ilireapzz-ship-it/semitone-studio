#!/usr/bin/env python3
"""Kick Fit: measure real kicks and real songs, so the plug-in's targets come from numbers.

  python tools/kick_analyse.py selftest            check the measuring itself on made-up kicks
  python tools/kick_analyse.py kicks  [folders…]   measure kick samples
  python tools/kick_analyse.py songs  [folders…]   split songs into stems (Demucs) and measure them
  python tools/kick_analyse.py targets             write data/kickfit-targets.json (totals only)

Privacy: every per-file result (with its path) is written to the local folder
%LOCALAPPDATA%/SemitoneKickFit (or ~/.semitone-kickfit) and never into the repository.
Only `targets` writes into the repo, and it writes medians, ranges and counts: no names, no paths.

Needs: numpy, scipy, soundfile, av; for songs also torch, demucs, mutagen.
"""
import sys, os, re, json, math, hashlib, argparse, time
import numpy as np
from scipy import signal

HOME = os.path.expanduser('~')
LOCAL = os.path.join(os.environ.get('LOCALAPPDATA') or HOME, 'SemitoneKickFit' if os.environ.get('LOCALAPPDATA') else '.semitone-kickfit')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AUDIO_EXT = ('.wav', '.flac', '.aif', '.aiff', '.ogg', '.mp3', '.m4a')
KICK_NAME = re.compile(r'(^|[^a-z])(kicks?|kck|kik|bd|bassdrum|bass drum|808s?)([^a-z]|$)', re.I)
NOTES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']

# Where Jamie's kicks live on the Windows PC (label: folder). Missing folders are skipped.
KICK_ROOTS = {
    'splice': r'~\Documents\Splice',
    'flcloud': r'~\Documents\Image-Line\Downloads\FL CLOUD Sounds',
    'flcloud2': r'~\Documents\Image-Line\FL Studio\flcloud.user\FL CLOUD Sounds',
    'flpacks': r'~\Documents\Music Production\Data\Patches\Packs',
    'battery': r'~\Documents\Battery 4 Factory Library\Samples',
}
SONG_ROOTS = [r'~\Music\iTunes\iTunes Media\Music']

# iTunes genre tag -> Kick Fit mode. Tags that match nothing are counted as "unmapped".
MODES = {
    'trap-hiphop': ['hip-hop', 'hip hop', 'rap', 'trap', 'drill', 'grime'],
    'pop': ['pop', 'k-pop', 'singer/songwriter', 'singer-songwriter'],
    'rnb': ['r&b', 'rnb', 'soul', 'funk'],
    'house-edm': ['dance', 'electronic', 'house', 'edm', 'techno', 'trance', 'garage'],
    'dnb': ['drum & bass', 'drum and bass', 'dnb', 'jungle'],
    'rock': ['rock', 'alternative', 'indie', 'metal', 'punk', 'grunge'],
    'afro': ['afrobeats', 'afro', 'amapiano', 'afropop', 'african'],
    'lofi': ['lo-fi', 'lofi', 'chillhop'],
}
MODE_ORDER = ['dnb', 'afro', 'lofi', 'trap-hiphop', 'rnb', 'house-edm', 'rock', 'pop']   # most specific first
MIN_SONGS = 8        # fewer than this in a mode: "not enough data"


def mode_of(genre):
    g = (genre or '').lower()
    for m in MODE_ORDER:
        if any(w in g for w in MODES[m]):
            return m
    return None


# ---------- audio in ----------
def load_audio(path, sr=None):
    """Returns (float32 array [n, channels], rate). sr = resample to this rate."""
    x = None
    if path.lower().endswith(('.wav', '.flac', '.aif', '.aiff', '.ogg')):
        try:
            import soundfile as sf
            x, r = sf.read(path, dtype='float32', always_2d=True)
        except Exception:
            x = None
    if x is None and path.lower().endswith('.wav'):          # FL Studio's packs: an Ogg Vorbis stream inside a WAV (format 0x674F)
        try:
            import io, soundfile as sf
            b = open(path, 'rb').read(); i = b.find(b'data', 12); j = b.find(b'OggS', i)
            if i > 0 and 0 < j < i + 64:
                x, r = sf.read(io.BytesIO(b[j:]), dtype='float32', always_2d=True)
        except Exception:
            x = None
    if x is None:
        import av
        with av.open(path) as c:
            st = c.streams.audio[0]
            r = sr or st.codec_context.sample_rate
            rs = av.AudioResampler(format='flt', layout='stereo', rate=r)
            parts = []
            for fr in c.decode(st):
                for o in rs.resample(fr):
                    parts.append(o.to_ndarray().reshape(-1, 2))
            for o in rs.resample(None):
                parts.append(o.to_ndarray().reshape(-1, 2))
        if not parts:
            raise ValueError('no audio decoded')
        x = np.concatenate(parts).astype(np.float32)
    if sr and r != sr:
        g = math.gcd(int(r), int(sr))
        x = signal.resample_poly(x, sr // g, r // g, axis=0).astype(np.float32)
        r = sr
    return x, int(r)


# ---------- small DSP helpers ----------
def db(v):
    return 10.0 * math.log10(max(float(v), 1e-20))


def band(x, sr, lo=None, hi=None, order=4):
    ny = sr / 2
    if lo and hi:
        sos = signal.butter(order, [lo / ny, min(hi / ny, 0.99)], 'band', output='sos')
    elif lo:
        sos = signal.butter(order, lo / ny, 'high', output='sos')
    else:
        sos = signal.butter(order, min(hi / ny, 0.99), 'low', output='sos')
    return signal.sosfiltfilt(sos, x, axis=0)


def rms_env(x, sr, ms):
    n = max(1, int(sr * ms / 1000))
    return np.sqrt(np.convolve(x * x, np.ones(n) / n, mode='same'))


def kweight(x, sr):
    """ITU-R BS.1770 K-weighting (shelf + high-pass) designed for any sample rate."""
    def biq(b, a):
        return np.array([[b[0] / a[0], b[1] / a[0], b[2] / a[0], 1.0, a[1] / a[0], a[2] / a[0]]])
    f0, G, Q = 1681.974450955533, 3.999843853973347, 0.7071752369554196
    K = math.tan(math.pi * f0 / sr); Vh = 10 ** (G / 20); Vb = Vh ** 0.4996667741545416
    a0 = 1 + K / Q + K * K
    s1 = biq([(Vh + Vb * K / Q + K * K), 2 * (K * K - Vh), (Vh - Vb * K / Q + K * K)], [a0, 2 * (K * K - 1), (1 - K / Q + K * K)])
    f0, Q = 38.13547087602444, 0.5003270373238773
    K = math.tan(math.pi * f0 / sr)
    s2 = biq([1, -2, 1], [1 + K / Q + K * K, 2 * (K * K - 1), 1 - K / Q + K * K])
    return signal.sosfilt(np.vstack([s1, s2]), x, axis=0)


def spec_peak(seg, sr, lo, hi):
    """Strongest spectral peak between lo and hi Hz (Hann window, zero-padded, parabolic fit)."""
    n = len(seg)
    if n < 16:
        return None
    nfft = 1 << int(math.ceil(math.log2(max(n * 8, 16384))))
    sp = np.abs(np.fft.rfft(seg * np.hanning(n), nfft))
    f = np.fft.rfftfreq(nfft, 1 / sr)
    a, b = np.searchsorted(f, lo), np.searchsorted(f, hi)
    if b - a < 3:
        return None
    k = a + int(np.argmax(sp[a:b]))
    if k <= 0 or k >= len(sp) - 1:
        return None
    y0, y1, y2 = np.log(sp[k - 1:k + 2] + 1e-20)
    d = 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2) if (y0 - 2 * y1 + y2) != 0 else 0.0
    return float((k + d) * sr / nfft)


def zc_track(lp, sr, env, floor):
    """Cycle-by-cycle frequency from rising zero crossings: [(time s, Hz)]."""
    s = np.signbit(lp)
    idx = np.nonzero(s[:-1] & ~s[1:])[0]
    if len(idx) < 2:
        return []
    t = (idx + lp[idx] / (lp[idx] - lp[idx + 1])) / sr
    out = []
    for a, b in zip(t[:-1], t[1:]):
        f = 1.0 / (b - a)
        m = int((a + b) / 2 * sr)
        if 20 <= f <= 400 and env[min(m, len(env) - 1)] > floor:
            out.append(((a + b) / 2, f))
    return out


def note_of(f):
    m = 69 + 12 * math.log2(f / 440.0)
    r = int(round(m))
    return NOTES[r % 12], r % 12, 100 * (m - r)


# ---------- one kick ----------
def measure_kick(x, sr):
    """x: [n, ch]. Returns (measurements dict, reason it is not a kick or None)."""
    mono = x.mean(axis=1).astype(np.float64)
    peak = float(np.max(np.abs(x))) if len(x) else 0.0
    if peak < 1e-4:
        return None, 'silent'
    a = np.abs(mono)
    on = int(np.argmax(a > 0.03 * a.max()))                   # start: first sample 30 dB under the peak
    mono = mono[on:]; xs = x[on:]
    dur = len(mono) / sr
    if dur < 0.03:
        return None, 'too short'
    if dur > 12:
        return None, 'too long (a loop or a song)'
    dc = float(np.mean(mono))
    m0 = mono - dc
    env = rms_env(m0, sr, 20)
    pk_i = int(np.argmax(env)); pk = env[pk_i]

    def fall(dbdown):
        th = pk * 10 ** (-dbdown / 20)
        below = np.nonzero(env[pk_i:] < th)[0]
        return (pk_i + below[0]) / sr if len(below) else None
    t20, t40 = fall(20), fall(40)
    end = t40 or dur
    low = band(m0, sr, 25, 200)
    lenv = rms_env(low, sr, 20)
    w = m0[:int(min(0.3, dur) * sr)]; lw = low[:len(w)]
    low_frac = float(np.sum(lw * lw) / max(np.sum(w * w), 1e-20))
    low_pk_t = float(np.argmax(lenv) / sr)
    pks, _ = signal.find_peaks(lenv, distance=int(0.15 * sr), prominence=0.4 * lenv.max())
    r = {
        'sr': sr, 'channels': int(x.shape[1]), 'dur': round(dur, 4),
        'peak_db': round(20 * math.log10(peak), 2), 'dc': round(dc / peak, 5),
        'low_frac': round(low_frac, 3), 'hits': int(len(pks)),
        'attack_ms': round(1000 * float(np.argmax(np.abs(m0)) / sr), 2),
        't20': t20 and round(t20, 4), 't40': t40 and round(t40, 4),
    }
    if len(pks) > 1:
        return r, 'more than one hit (a loop)'
    if low_frac < 0.3:
        return r, 'not enough low end at the start'
    if low_pk_t > 0.15:
        return r, 'the low end arrives late (not a hit)'

    # loudness of the hit: K-weighted and plain RMS over the hit (start to 40 dB down)
    hit = m0[:max(int(end * sr), int(0.05 * sr))]
    kw = kweight(np.concatenate([hit, np.zeros(int(0.05 * sr))]), sr)
    r['rms_db'] = round(db(np.mean(hit * hit)), 2)
    r['kw_db'] = round(db(np.sum(kw * kw) / len(hit)) - 0.691, 2)
    up = signal.resample_poly(hit[:int(0.2 * sr)], 4, 1)
    r['true_peak_db'] = round(20 * math.log10(max(np.max(np.abs(up)), 1e-9)), 2)
    r['first_swing'] = '+' if m0[int(np.argmax(np.abs(m0) > 0.25 * np.abs(m0).max()))] > 0 else '-'
    ap, an = m0.max(), -m0.min()
    r['asym_db'] = round(20 * math.log10(max(ap, 1e-9) / max(an, 1e-9)), 2)

    # balance: energy per band over the hit, dB relative to the whole hit
    tot = np.sum(hit * hit) + 1e-20
    for nm, lo, hi in (('sub', 20, 60), ('lowb', 60, 120), ('lowmid', 120, 300), ('mid', 300, 2000), ('high', 2000, None)):
        if hi and hi >= sr / 2: hi = None
        b = band(hit, sr, lo, hi) if (lo or hi) else hit
        r['b_' + nm] = round(db(np.sum(b * b) / tot), 2)
    # small speakers: what is left above 180 Hz
    hp = band(hit, sr, 180, None)
    r['above180_db'] = round(db(np.sum(hp * hp) / tot), 2)

    # click: energy above 2 kHz in the first 20 ms against the whole hit, and where it sits
    c = band(m0[:int(0.05 * sr)], sr, 2000, None)[:int(0.02 * sr)]
    r['click_db'] = round(db(np.sum(c * c) / tot), 2)
    if np.sum(c * c) > 1e-12 and len(c) > 32:
        sp = np.abs(np.fft.rfft(c * np.hanning(len(c)))) ** 2
        f = np.fft.rfftfreq(len(c), 1 / sr)
        r['click_hz'] = round(float(np.sum(f * sp) / np.sum(sp)))

    # boom note: cycle-by-cycle track for the glide, spectrum peak for the settled note
    lp = band(m0, sr, None, 300)
    tr = zc_track(lp, sr, env, pk * 10 ** (-30 / 20))
    body_end = t20 or dur
    if len(tr) >= 3:
        late = [f for t, f in tr if t >= 0.5 * body_end and t <= max(body_end, 0.06)] or [f for t, f in tr[-3:]]
        f_set = float(np.median(late))
        early = [f for t, f in tr if t < 0.025] or [tr[0][1]]
        f_start = float(np.median(early))
        g_end = 0.0
        for t, f in tr:
            if abs(12 * math.log2(f / f_set)) > 1 and t < body_end:
                g_end = t
        a_i = int(max(g_end, 0.03) * sr); b_i = int(min(dur, max(t40 or dur, 0.12)) * sr)
        fp = spec_peak(m0[a_i:b_i], sr, f_set * 0.7, f_set * 1.4) if b_i - a_i > 3 * sr / f_set else None
        f0 = fp or f_set
        nm, pc, cents = note_of(f0)
        r.update(f0=round(f0, 2), f0_by='spectrum' if fp else 'cycles', note=nm, pc=pc, cents=round(cents, 1),
                 f_start=round(f_start, 1), glide_st=round(12 * math.log2(f_start / f0), 2), glide_ms=round(1000 * g_end, 1))
        steady = [f for t, f in tr if t > g_end]
        if len(steady) >= 4:
            r['wobble_cents'] = round(1200 * float(np.std(np.log2(np.array(steady) / f0))), 1)
    # stereo: do the lows agree in both speakers
    if x.shape[1] >= 2:
        L, R = band(xs[:, 0].astype(np.float64), sr, None, 150), band(xs[:, 1].astype(np.float64), sr, None, 150)
        den = math.sqrt(np.sum(L * L) * np.sum(R * R))
        r['low_corr'] = round(float(np.sum(L * R) / den), 3) if den > 1e-12 else None
    return r, None


# ---------- kicks ----------
def find_kicks(roots):
    seen = set()
    for label, root in roots.items():
        root = os.path.expanduser(root)
        if not os.path.isdir(root):
            continue
        for d, _, files in os.walk(root):
            for fn in files:
                if not fn.lower().endswith(AUDIO_EXT):
                    continue
                p = os.path.join(d, fn)
                if not KICK_NAME.search(os.path.relpath(p, root)):
                    continue
                try:
                    with open(p, 'rb') as fh:
                        key = (os.path.getsize(p), hashlib.md5(fh.read(65536)).hexdigest())
                except OSError:
                    continue
                if key in seen:
                    continue
                seen.add(key)
                yield label, p


def cmd_kicks(args):
    roots = {('arg%d' % i): r for i, r in enumerate(args.folders)} if args.folders else KICK_ROOTS
    os.makedirs(LOCAL, exist_ok=True)
    out = os.path.join(LOCAL, 'kicks.jsonl')
    n = ok = 0
    why = {}
    with open(out, 'w', encoding='utf-8') as fh:
        for label, p in find_kicks(roots):
            n += 1
            try:
                x, sr = load_audio(p)
                r, reason = measure_kick(x, sr)
            except Exception as e:
                r, reason = None, 'could not read: ' + type(e).__name__
            rec = dict(r or {}, path=p, source=label, name808=bool(re.search(r'808', p, re.I)), reject=reason)
            fh.write(json.dumps(rec) + '\n')
            if reason: why[reason] = why.get(reason, 0) + 1
            else: ok += 1
            if args.limit and n >= args.limit: break
            if n % 200 == 0: print(f'  {n} looked at, {ok} confirmed as kicks', flush=True)
    print(f'{n} kick-named files, {ok} confirmed by sound. Not kicks: {why}')
    print('Details (stay on this computer):', out)


# ---------- songs ----------
KK_MAJ = np.array([6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88])
KK_MIN = np.array([6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17])


def detect_key(x, sr):
    """Key from the pitched stems (Krumhansl-Kessler profiles on a chroma sum). Returns (tonic pc, 'maj'/'min', confidence)."""
    y = signal.resample_poly(x, 1, 4); r = sr / 4
    f, _, Z = signal.stft(y, r, nperseg=4096, noverlap=2048)
    P = np.abs(Z) ** 2
    ch = np.zeros(12)
    for i, fr in enumerate(f):
        if 55 <= fr <= 2000:
            ch[int(round(69 + 12 * math.log2(fr / 440))) % 12] += P[i].sum()
    ch = np.sqrt(ch)
    best = []
    for t in range(12):
        for nm, prof in (('maj', KK_MAJ), ('min', KK_MIN)):
            best.append((float(np.corrcoef(np.roll(prof, t), ch)[0, 1]), t, nm))
    best.sort(reverse=True)
    return best[0][1], best[0][2], round(best[0][0] - best[1][0], 3)


def measure_song(mix, stems, sr):
    """mix [n,2]; stems dict of [n,2]. Everything measured at kick hits found in the drum stem's low band."""
    dr = stems['drums'].mean(axis=1).astype(np.float64)
    bs = stems['bass'].mean(axis=1).astype(np.float64)
    mx = mix.mean(axis=1).astype(np.float64)
    klow = band(dr, sr, 30, 150)
    env = rms_env(klow, sr, 10)
    ref = np.percentile(env, 99)
    if ref < 1e-4:
        return {'kicks': 0}
    pk, _ = signal.find_peaks(env, height=0.35 * ref, distance=int(0.12 * sr), prominence=0.2 * ref)
    ons = []
    for p in pk:
        a = max(0, p - int(0.06 * sr))
        seg = env[a:p]
        under = np.nonzero(seg < 0.2 * env[p])[0]
        ons.append(a + under[-1] if len(under) else a)
    ons = np.array(sorted(set(ons)))
    r = {'kicks': int(len(ons)), 'secs': round(len(mx) / sr, 1)}
    if len(ons) < 16:
        return r
    r['kicks_per_min'] = round(len(ons) / (len(mx) / sr) * 60, 1)
    W = int(0.12 * sr)
    kfull_k, mix_k = kweight(dr, sr), kweight(mx, sr)
    blow = band(bs, sr, 30, 150)
    lows_mix, lows_kb = band(mx, sr, None, 300), band(dr + bs, sr, None, 300)      # everything under 300 Hz: the whole mix, and kick + bass only
    d_lm, d_kb = [], []
    benv = rms_env(blow, sr, 15)
    bact = np.percentile(benv, 75)

    def lvl(v, a, b):
        a = max(a, 0); b = min(b, len(v))
        return np.mean(v[a:b] ** 2) if b > a else 0.0
    d_low, d_full, ducks, both = [], [], [], []
    NF = 1 << 17; f0s = [np.zeros(NF // 2 + 1), np.zeros(NF // 2 + 1)]; nf0 = [0, 0]
    for i, o in enumerate(ons):
        if o + W >= len(mx): break
        m = lvl(mx, o, o + W)
        if m < 1e-10: continue
        d_low.append(db(lvl(klow, o, o + W)) - db(m))
        d_lm.append(db(lvl(klow, o, o + W)) - db(lvl(lows_mix, o, o + W))); d_kb.append(db(lvl(klow, o, o + W)) - db(lvl(lows_kb, o, o + W)))
        d_full.append(db(lvl(kfull_k, o, o + W)) - db(lvl(mix_k, o, o + W)))
        nxt = ons[i + 1] if i + 1 < len(ons) else len(mx)
        a, b = o + int(0.03 * sr), min(o + int(0.22 * sr), nxt - int(0.01 * sr))
        if b - a >= int(0.1 * sr):                     # add this hit's spectrum to one of two piles (every other hit)
            seg = dr[a:a + int(0.1 * sr)]
            f0s[len(d_low) % 2] += np.abs(np.fft.rfft(seg * np.hanning(len(seg)), NF)) ** 2; nf0[len(d_low) % 2] += 1
        # is the bass sounding when the kick hits, and does it dip
        before = math.sqrt(lvl(blow, o - int(0.08 * sr), o - int(0.02 * sr)))
        during = math.sqrt(lvl(blow, o + int(0.01 * sr), o + int(0.07 * sr)))
        after = math.sqrt(lvl(blow, o + int(0.15 * sr), o + int(0.25 * sr)))
        both.append(1.0 if max(during, after) > 0.25 * bact else 0.0)
        if before > 0.5 * bact and after > 0.5 * bact:
            ducks.append(20 * math.log10(max(during, 1e-9) / (0.5 * (before + after))))
    r['kick_low_vs_mix_db'] = round(float(np.median(d_low)), 2)
    r['kick_low_vs_lows_db'] = round(float(np.median(d_lm)), 2); r['kick_low_vs_kickbass_lows_db'] = round(float(np.median(d_kb)), 2)
    r['drums_at_kick_vs_mix_kw_db'] = round(float(np.median(d_full)), 2)
    r['bass_under_kick_share'] = round(float(np.mean(both)), 3)
    if len(ducks) >= 12:
        r['bass_dip_db'] = round(float(np.median(ducks)), 2); r['bass_dip_hits'] = len(ducks)
    # boom note against the key
    if min(nf0) >= 8:
        fr = np.fft.rfftfreq(NF, 1 / sr); a_i, b_i = np.searchsorted(fr, 30), np.searchsorted(fr, 150)
        pk = lambda sp: float(fr[a_i + int(np.argmax(sp[a_i:b_i]))])
        f0 = pk(f0s[0] + f0s[1])
        spread = abs(1200 * math.log2(pk(f0s[0]) / pk(f0s[1])))        # do the two piles agree
        nm, pc, cents = note_of(f0)
        r.update(kick_f0=round(f0, 2), kick_note=nm, kick_pc=pc, kick_cents=round(cents, 1), kick_f0_spread_cents=round(spread, 1))
    pitched = (stems['bass'] + stems['other'] + stems['vocals']).mean(axis=1)
    t, md, conf = detect_key(pitched, sr)
    r.update(key=NOTES[t] + ('m' if md == 'min' else ''), key_pc=t, key_mode=md, key_conf=conf)
    if 'kick_pc' in r:
        r['kick_vs_tonic'] = (r['kick_pc'] - t) % 12
        scale = {0, 2, 4, 5, 7, 9, 11} if md == 'maj' else {0, 2, 3, 5, 7, 8, 10}
        r['kick_in_scale'] = r['kick_vs_tonic'] in scale
    # overlap in pitch: third-octave band energies of kick hits and of the bass, 20-320 Hz
    hits = np.concatenate([dr[o:o + W] for o in ons if o + W < len(dr)])
    edges = [20 * 2 ** (i / 3) for i in range(13)]

    def bands_of(v):
        f, P = signal.welch(v, sr, nperseg=16384)
        e = np.array([P[(f >= lo) & (f < hi)].sum() for lo, hi in zip(edges[:-1], edges[1:])])
        return e / max(e.sum(), 1e-20), float(np.sum(f[(f >= 20) & (f < 320)] * P[(f >= 20) & (f < 320)]) / max(P[(f >= 20) & (f < 320)].sum(), 1e-20))
    ke, kc = bands_of(hits)
    be, bc = bands_of(bs[benv > 0.25 * bact]) if np.any(benv > 0.25 * bact) else (np.zeros(12), 0.0)
    r['low_overlap'] = round(float(np.minimum(ke, be).sum()), 3)          # 0 = separate ranges, 1 = same range
    r['kick_low_centre_hz'] = round(kc, 1); r['bass_low_centre_hz'] = round(bc, 1)
    r['kick_top_band_hz'] = round(edges[int(np.argmax(ke))] * 2 ** (1 / 6), 1)
    r['bass_top_band_hz'] = round(edges[int(np.argmax(be))] * 2 ** (1 / 6), 1) if be.sum() else None
    # low end mono-ness of the finished mix below 120 Hz
    L, R = band(mix[:, 0].astype(np.float64), sr, None, 120), band(mix[:, 1].astype(np.float64), sr, None, 120)
    mid, side = (L + R) / 2, (L - R) / 2
    r['low_side_vs_mid_db'] = round(db(np.mean(side ** 2)) - db(np.mean(mid ** 2)), 2)
    den = math.sqrt(np.sum(L * L) * np.sum(R * R))
    r['low_corr'] = round(float(np.sum(L * R) / den), 3) if den > 1e-12 else None
    # how loud the low end of the whole mix is against the whole mix
    ml = band(mx, sr, None, 120)
    r['mix_low_share_db'] = round(db(np.mean(ml ** 2)) - db(np.mean(mx ** 2)), 2)
    return r


def cmd_songs(args):
    import torch, mutagen
    from demucs.pretrained import get_model
    from demucs.apply import apply_model
    roots = [os.path.expanduser(r) for r in (args.folders or SONG_ROOTS)]
    os.makedirs(LOCAL, exist_ok=True)
    out = os.path.join(LOCAL, 'songs.jsonl')
    done = set()
    if args.fresh and os.path.exists(out): os.remove(out)
    if os.path.exists(out):
        for ln in open(out, encoding='utf-8'):
            try: done.add(json.loads(ln)['path'])
            except Exception: pass
    files = [os.path.join(d, f) for r in roots for d, _, fs in os.walk(r) for f in fs if f.lower().endswith(AUDIO_EXT)]
    files = [f for f in sorted(files) if f not in done]
    if args.limit: files = files[:args.limit]
    dev = 'cuda' if torch.cuda.is_available() else 'cpu'
    print(f'{len(files)} songs to measure ({len(done)} already done), separating on {dev}', flush=True)
    model = get_model('htdemucs'); model.to(dev).eval()
    t0 = time.time()
    with open(out, 'a', encoding='utf-8') as fh:
        for i, p in enumerate(files):
            rec = {'path': p}
            try:
                tg = mutagen.File(p, easy=True)
                rec['genre'] = (tg.get('genre') or [None])[0] if tg else None
                rec['year'] = ((tg.get('date') or [''])[0] or '')[:4] if tg else None
                x, sr = load_audio(p, model.samplerate)
                if len(x) < sr * 30: raise ValueError('shorter than 30 seconds')
                x = x[:sr * 420]
                w = torch.from_numpy(x.T.copy())
                ref = w.mean(0); mu, sd = ref.mean(), ref.std() + 1e-8
                with torch.no_grad():
                    est = apply_model(model, ((w - mu) / sd)[None], device=dev, split=True, overlap=0.25, progress=False)[0]
                est = (est * sd + mu).cpu().numpy()
                stems = {nm: est[k].T for k, nm in enumerate(model.sources)}
                rec.update(measure_song(x, stems, sr))
            except Exception as e:
                rec['error'] = type(e).__name__ + ': ' + str(e)[:120]
            rec['mode'] = mode_of(rec.get('genre'))
            fh.write(json.dumps(rec) + '\n'); fh.flush()
            if (i + 1) % 10 == 0:
                print(f'  {i + 1}/{len(files)}  ({(time.time() - t0) / (i + 1):.1f} s a song)', flush=True)
    print('Details (stay on this computer):', out)


# ---------- totals ----------
def stat(vals, nd=2):
    v = np.array([x for x in vals if x is not None and not (isinstance(x, float) and math.isnan(x))], dtype=float)
    if len(v) == 0:
        return None
    return {'n': int(len(v)), 'median': round(float(np.median(v)), nd), 'p10': round(float(np.percentile(v, 10)), nd),
            'p25': round(float(np.percentile(v, 25)), nd), 'p75': round(float(np.percentile(v, 75)), nd), 'p90': round(float(np.percentile(v, 90)), nd)}


def read_local(name):
    p = os.path.join(LOCAL, name)
    return [json.loads(l) for l in open(p, encoding='utf-8')] if os.path.exists(p) else []


KICK_FIELDS = ['f0', 'f_start', 'glide_st', 'glide_ms', 'attack_ms', 't20', 't40', 'click_db', 'click_hz', 'b_sub', 'b_lowb', 'b_lowmid',
               'b_mid', 'b_high', 'above180_db', 'peak_db', 'true_peak_db', 'rms_db', 'kw_db', 'dc', 'asym_db', 'low_corr', 'wobble_cents']
SONG_FIELDS = ['kick_low_vs_mix_db', 'kick_low_vs_lows_db', 'kick_low_vs_kickbass_lows_db', 'drums_at_kick_vs_mix_kw_db', 'bass_under_kick_share', 'bass_dip_db', 'kick_f0', 'kick_cents',
               'low_overlap', 'kick_low_centre_hz', 'bass_low_centre_hz', 'low_side_vs_mid_db', 'low_corr', 'mix_low_share_db', 'kicks_per_min']


def kick_kind(k):
    if (k.get('t20') or 0) >= 0.35 and abs(k.get('wobble_cents') or 99) < 40: return 'long-pitched (808 style)'
    return 'short' if (k.get('t20') or 9) < 0.15 else 'medium'


def cmd_targets(args):
    kicks = [k for k in read_local('kicks.jsonl') if not k.get('reject')]
    allk = read_local('kicks.jsonl')
    songs = read_local('songs.jsonl')
    good = [s for s in songs if s.get('kicks', 0) >= 16 and 'error' not in s]
    out = {
        'about': 'Kick Fit targets, measured by tools/kick_analyse.py on the kicks and songs on Jamie\'s PC. Totals only: no names, no paths, no audio.',
        'made': time.strftime('%Y-%m-%d'),
        'how': {
            'kicks': 'Files with a kick word in the name (kick, kck, bd, 808...), kept only when they sound like one hit with strong lows at the start.',
            'f0': 'Boom note in Hz: strongest spectrum peak after the glide has settled, near the cycle-by-cycle estimate.',
            'glide_st': 'Semitones from the pitch in the first 25 ms down to the settled boom note. glide_ms: when it comes within a semitone.',
            't20/t40': 'Seconds for the level (20 ms RMS) to fall 20 / 40 dB under its peak.',
            'click_db': 'Energy above 2 kHz in the first 20 ms, against the whole hit. click_hz: its centre.',
            'b_*': 'Energy share per band over the hit, dB: sub 20-60 Hz, lowb 60-120, lowmid 120-300, mid 300-2000, high above 2000.',
            'above180_db': 'What is left of the hit above 180 Hz (what a phone speaker can play), dB against the whole hit.',
            'kw_db/rms_db': 'K-weighted (BS.1770 filters) and plain RMS level of the hit from its start to 40 dB down. Not LUFS: a hit is too short for that.',
            'songs': 'Each song is split into drums, bass, other and vocals with Demucs (htdemucs). Kick hits are found as peaks in the drum stem between 30 and 150 Hz. Separation is not perfect, so some bass leaks into the drum stem and the other way round.',
            'kick_low_vs_mix_db': 'Level of the drum stem between 30 and 150 Hz against the whole mix, in the 120 ms after each kick hit; median over the hits.',
            'kick_low_vs_lows_db': 'The same kick lows (30-150 Hz) against everything under 300 Hz in the whole mix, in the same windows. This is what Sit it uses: it does not depend on how much is playing above the bass range.',
            'kick_low_vs_kickbass_lows_db': 'The same, against only the drum and bass stems under 300 Hz: what Kick Fit hears when just the bass is fed to it.',
            'drums_at_kick_vs_mix_kw_db': 'K-weighted level of the whole drum stem against the mix in the same windows. Includes hats and snares that land with the kick.',
            'bass_under_kick_share': 'Share of kick hits with bass sounding under them.',
            'bass_dip_db': 'Bass stem (30-150 Hz) level in the 10-70 ms after a kick against its level just before and 150-250 ms after. Negative = the bass drops. Leaked kick makes this look smaller than it is.',
            'kick_f0': 'Boom note of the kick in a song: the strongest peak between 30 and 150 Hz in the summed spectrum of every hit (30 to 130 ms after its start) in the drum stem. Counted only when two halves of the hits agree within 50 cents.',
            'kick_vs_tonic': 'That boom note as semitones above the key note; key guessed from the non-drum stems (Krumhansl-Kessler profiles; such guesses are often a fifth or a relative key out). Only songs with a steady kick note and a key guess that clearly beats the next one.',
            'low_overlap': 'How much the kick and the bass share the same third-octave bands between 20 and 320 Hz: 0 none, 1 identical.',
            'low_side_vs_mid_db': 'Below 120 Hz, the side (stereo difference) against the middle. Very negative = mono lows.',
            'click in songs': 'Not measured: hats and snares hit with the kick, so the kick\'s own click cannot be told apart in a finished song.',
        },
        'kickSamples': {'looked_at': len(allk), 'confirmed': len(kicks), 'by_source': {}, 'by_kind': {}, 'all': {}},
        'modes': {}, 'songs': {'measured': len(good), 'could_not_use': len(songs) - len(good), 'no_matching_mode': sum(1 for s in good if not s.get('mode'))},
        'min_songs_per_mode': MIN_SONGS,
    }
    ks = out['kickSamples']
    ks['all'] = {f: stat([k.get(f) for k in kicks]) for f in KICK_FIELDS}
    ks['notes'] = {NOTES[i]: sum(1 for k in kicks if k.get('pc') == i) for i in range(12)}
    ks['first_swing_up_share'] = round(sum(1 for k in kicks if k.get('first_swing') == '+') / max(len(kicks), 1), 3)
    ks['stereo_share'] = round(sum(1 for k in kicks if k.get('channels', 1) > 1) / max(len(kicks), 1), 3)
    ks['sample_rates'] = {str(r): sum(1 for k in kicks if k.get('sr') == r) for r in sorted({k.get('sr') for k in kicks})}
    for src in sorted({k['source'] for k in kicks}):
        ks['by_source'][src] = sum(1 for k in kicks if k['source'] == src)
    for kind in ('short', 'medium', 'long-pitched (808 style)'):
        sub = [k for k in kicks if kick_kind(k) == kind]
        ks['by_kind'][kind] = {'n': len(sub), **{f: stat([k.get(f) for k in sub]) for f in ('f0', 'glide_st', 't20', 'click_db', 'above180_db', 'b_sub', 'b_lowb')}}
    for m in MODES:
        sub = [s for s in good if s.get('mode') == m]
        if len(sub) < MIN_SONGS:
            out['modes'][m] = {'songs': len(sub), 'status': 'not enough data'}
            continue
        steady = [s for s in sub if s.get('kick_f0_spread_cents') is not None and s['kick_f0_spread_cents'] < 50]
        d = {'songs': len(sub), 'status': 'measured', **{f: stat([s.get(f) for s in sub]) for f in SONG_FIELDS}}
        d['kick_f0'] = stat([s.get('kick_f0') for s in steady]); d['kick_cents'] = stat([s.get('kick_cents') for s in steady])
        keyed = [s for s in steady if (s.get('key_conf') or 0) >= 0.05]
        d['kick_note_steady_songs'] = len(steady)
        d['kick_note_and_key_songs'] = len(keyed)
        d['kick_vs_tonic_semitones'] = {str(i): sum(1 for s in keyed if s.get('kick_vs_tonic') == i) for i in range(12)}
        d['kick_in_scale_share'] = round(sum(1 for s in keyed if s.get('kick_in_scale')) / len(keyed), 3) if keyed else None
        out['modes'][m] = d
    dst = os.path.join(REPO, 'data', 'kickfit-targets.json')
    json.dump(out, open(dst, 'w', encoding='utf-8'), indent=1)
    print('wrote', dst)
    tags = {}
    for s in good: tags[s.get('genre')] = tags.get(s.get('genre'), 0) + 1
    print('genre tags seen (not saved):', sorted(tags.items(), key=lambda kv: -kv[1]))


# ---------- self test: known answers ----------
def synth_kick(sr, f_end, f_start=None, glide=0.03, decay=0.25, click=0.0, dur=1.0):
    t = np.arange(int(sr * dur)) / sr
    f = f_end + ((f_start or f_end) - f_end) * np.exp(-t / glide)
    y = np.sin(2 * np.pi * np.cumsum(f) / sr) * np.exp(-t / decay)
    if click:
        rng = np.random.default_rng(1)
        y += click * band(rng.standard_normal(len(t)), sr, 3000, 9000) * np.exp(-t / 0.003)
    return (0.8 * y / np.max(np.abs(y)))[:, None].astype(np.float32)


def cmd_selftest(args):
    bad = 0
    for sr in (44100, 48000, 96000):
        for f_end, f_start, decay in ((55.0, 180.0, 0.18), (43.65, 43.65, 0.9), (61.74, 240.0, 0.12), (49.0, 98.0, 0.5), (82.41, 300.0, 0.08)):
            r, why = measure_kick(synth_kick(sr, f_end, f_start, decay=decay, click=0.2, dur=max(1.0, decay * 4)), sr)
            cents = 1200 * math.log2(r['f0'] / f_end) if r and 'f0' in r else 999
            want = decay * math.log(10)       # time to fall 20 dB
            ok = why is None and abs(cents) < 15 and r.get('t20') is not None and abs(r['t20'] - want) < 0.03 + 0.1 * want
            bad += not ok
            print(f"{'ok  ' if ok else 'FAIL'} {sr} Hz: boom {f_end} Hz measured {r.get('f0')} ({cents:+.1f} cents), glide {r.get('glide_st')} st, "
                  f"20 dB decay {r.get('t20')} s (true {want:.3f}) {why or ''}")
    for nm, x in (('hat', band(np.random.default_rng(2).standard_normal(22050), 44100, 6000, None)[:, None] * np.exp(-np.arange(22050) / 2000)[:, None]),
                  ('loop', np.concatenate([synth_kick(44100, 50, 150, dur=0.5)] * 4))):
        r, why = measure_kick(x.astype(np.float32), 44100)
        print(f"{'ok  ' if why else 'FAIL'} a {nm} is turned away: {why}"); bad += not why
    print('self test', 'passed' if not bad else f'FAILED ({bad})')
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    for nm in ('selftest', 'kicks', 'songs', 'targets'):
        p = sub.add_parser(nm)
        p.add_argument('folders', nargs='*')
        p.add_argument('--limit', type=int, default=0)
        p.add_argument('--fresh', action='store_true')
    a = ap.parse_args()
    sys.exit({'selftest': cmd_selftest, 'kicks': cmd_kicks, 'songs': cmd_songs, 'targets': cmd_targets}[a.cmd](a) or 0)


if __name__ == '__main__':
    main()
