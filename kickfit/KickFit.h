#pragma once
// Kick Fit: the sound processing, with no JUCE in it, so it can be tested alone:
//   clang++ -std=c++17 -O2 -Ikickfit tests/kickfit_test.cpp -o t && ./t
//
// On the kick (role 0): find each hit, read its boom note, retune it, shape it (Thump, Punch, Click, Tail),
// measure it against the sidechain (the bass or the whole beat) and set its level.
// On the bass (role 1): the kick comes in on the sidechain and only the bass's low range dips under it.
//
// Latency: none, unless Tune is moving the note. Retuning plays the hit back slower or faster, the way a
// sampler does, so it needs 3 ms to catch the very start of the hit, plus up to 60 ms to tune upwards
// (playing faster needs sound that has not arrived yet). Engine::latency() says how much; tell the host.
#include <algorithm>
#include <cmath>
#include <vector>

namespace kf
{
constexpr double pi = 3.14159265358979323846;
inline double zap (double v) { return std::abs (v) < 1e-18 ? 0.0 : v; }       // no denormals in feedback paths
inline double dbToGain (double dB) { return std::pow (10.0, dB / 20.0); }
inline double gainToDb (double g) { return 20.0 * std::log10 (std::max (g, 1e-12)); }

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    void set (double B0, double B1, double B2, double A0, double A1, double A2) { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }
    double run (double x) { const double y = b0 * x + z1; z1 = zap (b1 * x - a1 * y + z2); z2 = zap (b2 * x - a2 * y); return y; }
    void lowpass (double f, double q, double sr)  { const double w = 2 * pi * std::min (f, sr * .45) / sr, c = std::cos (w), al = std::sin (w) / (2 * q); set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); }
    void highpass (double f, double q, double sr) { const double w = 2 * pi * std::min (f, sr * .45) / sr, c = std::cos (w), al = std::sin (w) / (2 * q); set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); }
    void bandpass (double f, double q, double sr) { const double w = 2 * pi * std::min (f, sr * .45) / sr, c = std::cos (w), al = std::sin (w) / (2 * q); set (al, 0, -al, 1 + al, -2 * c, 1 - al); }
    void lowShelf (double f, double gain, double sr)          // gain 1 = exactly nothing (b == a)
    {
        const double A = std::sqrt (std::max (gain, 1e-3)), w = 2 * pi * std::min (f, sr * .45) / sr, c = std::cos (w), al = std::sin (w) / 2 * std::sqrt (2.0), s = 2 * std::sqrt (A) * al;
        const double a0 = (A + 1) + (A - 1) * c + s, A1 = -2 * ((A - 1) + (A + 1) * c), A2 = (A + 1) + (A - 1) * c - s;
        if (gain == 1.0) { a1 = A1 / a0; a2 = A2 / a0; b0 = 1; b1 = a1; b2 = a2; return; }
        set (A * ((A + 1) - (A - 1) * c + s), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - s), a0, A1, A2);
    }
    void peak (double f, double q, double dB, double sr)
    {
        const double A = std::pow (10.0, dB / 40), w = 2 * pi * std::min (f, sr * .45) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
};

struct OnePole
{
    double a = 0, y = 0;
    void time (double seconds, double sr) { a = std::exp (-1.0 / (std::max (1e-5, seconds) * sr)); }
    void cutoff (double hz, double sr) { a = std::exp (-2 * pi * hz / sr); }
    double run (double x) { y = zap (x + a * (y - x)); return y; }
};

// ---------- where a hit starts ----------
class Onset
{
public:
    void prepare (double sr)
    {
        rel = std::exp (-1.0 / (0.004 * sr)); slow.time (0.05, sr); hold = (int) (0.06 * sr); since = hold; fast = 0; slow.y = 0;
    }
    bool run (double x)
    {
        const double a = std::abs (x), before = slow.y;
        fast = std::max (a, fast * rel);
        slow.run (a);
        if (since < hold) { ++since; return false; }
        if (a > floor && a > 2.5 * before && a >= fast * 0.999) { since = 0; return true; }
        return false;
    }
    double floor = 2e-3;      // -54 dB: quieter than this is not a hit
private:
    double rel = 0, fast = 0; OnePole slow; int hold = 0, since = 0;
};

// ---------- the boom note ----------
// The lows (under 350 Hz) at about 4 kHz, searched for the repeat length that fits best (the YIN method),
// every 15 ms. A hit's note is the middle value of the later, steady frames, so a pitch drop at the start doesn't count.
class PitchTracker
{
public:
    void prepare (double sr)
    {
        M = std::max (1, (int) (sr / 4000.0 + 0.5)); fs = sr / M;
        lp1.lowpass (350, .54, sr); lp2.lowpass (350, 1.31, sr); lp1.reset(); lp2.reset();
        W = (int) (fs * 0.045); tauMin = (int) (fs / 260.0); tauMax = (int) (fs / 26.0) + 2;
        N = W + tauMax + 2; ring.assign ((size_t) N, 0.0); lin.assign ((size_t) N, 0.0); d.assign ((size_t) tauMax + 2, 0.0);
        hop = std::max (1, (int) (fs * 0.015)); w = 0; ph = 0; sinceFrame = 0; sinceOnset = 1 << 28; frames.clear(); peakRms = 0; last = 0; held = 0; nRecent = 0;
    }
    void onset() { if (frames.size() >= 3) { held = settled(); heldSpread = spreadCents(); recent[nRecent % 5] = held; ++nRecent; } sinceOnset = 0; frames.clear(); peakRms = 0; }
    // for the display: the middle of the last five hits when they agree (one kick), else the newest hit (an 808 playing notes)
    double steadyNote() const
    {
        const int n = std::min (nRecent, 5); if (n < 3) return settled();
        double v[5]; for (int i = 0; i < n; ++i) v[i] = recent[i];
        std::sort (v, v + n);
        return 1200 * std::log2 (v[n - 1] / v[0]) < 80 ? v[n / 2] : (frames.size() >= 4 ? settled() : held);
    }
    double heldNote() const { return held > 0 ? held : (frames.size() >= 8 ? settled() : 0.0); }     // steady between hits: for filters
    void push (double x)
    {
        x = lp2.run (lp1.run (x));
        if (++ph < M) return;
        ph = 0;
        ring[(size_t) w] = x; w = (w + 1) % N;
        if (sinceOnset < (1 << 28)) ++sinceOnset;
        if (++sinceFrame >= hop) { sinceFrame = 0; analyse(); }
    }
    // the note of the hit playing now or just played (Hz), 0 when not known
    double settled() const
    {
        if (frames.empty()) return last;
        std::vector<double> v; const size_t from = frames.size() >= 4 ? frames.size() / 2 : 0;
        for (size_t i = from; i < frames.size(); ++i) v.push_back (frames[i]);
        std::nth_element (v.begin(), v.begin() + (long) (v.size() / 2), v.end());
        return v[v.size() / 2];
    }
    // how far the later frames spread around that note, in cents (middle half). Big = the pitch keeps moving: no single note.
    double spreadCents() const
    {
        if (frames.size() < 4) return 0;
        std::vector<double> v (frames.begin() + (long) (frames.size() / 2), frames.end());
        std::sort (v.begin(), v.end());
        return 1200 * std::log2 (v[v.size() * 3 / 4] / v[v.size() / 4]);
    }
    double current() const { return frames.empty() ? 0.0 : frames.back(); }                 // the newest frame (follows a slide)
    double glideSemis() const { const double s = settled(); return frames.size() >= 3 && s > 0 ? 12 * std::log2 (frames.front() / s) : 0.0; }
    int frameCount() const { return (int) frames.size(); }
private:
    void analyse()
    {
        if (sinceOnset < N || sinceOnset > (int) (fs * 4)) return;        // wait until the frame holds only this hit
        double e = 0;
        for (int i = 0; i < N; ++i) { lin[(size_t) i] = ring[(size_t) ((w + i) % N)]; }
        for (int i = 0; i < W; ++i) e += lin[(size_t) i] * lin[(size_t) i];
        const double rms = std::sqrt (e / W);
        peakRms = std::max (peakRms, rms);
        if (rms < 1e-4 || rms < 0.03 * peakRms) return;                    // 30 dB under the hit: too quiet to trust
        double run = 0; d[0] = 1;
        for (int t = 1; t <= tauMax; ++t)
        {
            double s = 0;
            for (int j = 0; j < W; ++j) { const double v = lin[(size_t) j] - lin[(size_t) (j + t)]; s += v * v; }
            run += s; d[(size_t) t] = run > 0 ? s * t / run : 1;
        }
        int best = -1;
        for (int t = std::max (2, tauMin); t < tauMax; ++t)
            if (d[(size_t) t] < 0.15 && d[(size_t) t] <= d[(size_t) (t - 1)] && d[(size_t) t] <= d[(size_t) (t + 1)]) { best = t; break; }
        if (best < 0)
        {
            best = std::max (2, tauMin);
            for (int t = best; t < tauMax; ++t) if (d[(size_t) t] < d[(size_t) best]) best = t;
            if (d[(size_t) best] > 0.5 || best <= tauMin || best >= tauMax - 1) return;        // no clear note
        }
        const double y0 = d[(size_t) (best - 1)], y1 = d[(size_t) best], y2 = d[(size_t) (best + 1)], den = y0 - 2 * y1 + y2;
        const double tau = best + (den != 0 ? 0.5 * (y0 - y2) / den : 0.0);
        if (frames.size() < 256) frames.push_back (fs / tau);
        last = settled();
    }
    Biquad lp1, lp2; std::vector<double> ring, lin, d, frames;
    double fs = 4000, peakRms = 0, last = 0, held = 0, heldSpread = 0, recent[5] {}; int nRecent = 0;
public:
    double lastSpread() const { return frames.size() >= 8 ? spreadCents() : heldSpread; }
private: int M = 1, W = 0, N = 0, tauMin = 0, tauMax = 0, hop = 1, w = 0, ph = 0, sinceFrame = 0, sinceOnset = 0;
};

// ---------- retune ----------
// Plays each hit back from a short memory at another speed, exactly what retuning the sample in a sampler does.
// Slower (lower) falls behind and needs nothing ahead of time; faster (higher) starts `latency` behind and catches up.
class Retune
{
public:
    static int latencyFor (double ratio, double sr)
    {
        if (std::abs (ratio - 1.0) < 1e-6) return 0;
        return (int) (sr * (0.003 + (ratio > 1 ? std::min (0.06, (ratio - 1) * 0.6) : 0.0)));
    }
    void prepare (double sr_, int channels)
    {
        sr = sr_; N = 1; while (N < (int) (sr * 2.6)) N <<= 1;
        buf.assign ((size_t) channels, std::vector<float> ((size_t) N, 0.0f)); w = 0;
        lat = latencyFor (ratio, sr); guard = std::max (3.0, 0.003 * sr); cur = { std::max ((double) lat, guard), ratio, 0 }; old = cur; xf = 1; xfStep = 0;
    }
    void setRatio (double r)                     // takes effect on the next hit, so a hit never changes pitch half way
    {
        ratio = r; lat = latencyFor (r, sr);
    }
    int latency() const { return lat; }
    void write (int ch, float x) { buf[(size_t) ch][(size_t) w] = x; }
    void hit() { splice (0.002); cur.hold = (int) cur.d; }
    float read (int ch) const
    {
        const float a = tap (ch, cur.d);
        return xf >= 1 ? a : (float) (tap (ch, old.d) * (1 - xf) + a * xf);
    }
    void advance()
    {
        w = (w + 1) & (N - 1);
        step (cur); if (xf < 1) { step (old); xf = std::min (1.0, xf + xfStep); }
        if (cur.d > N - 16 - sr * 0.5) splice (0.02);            // fell too far behind (a note that never stops): step back gently
    }
private:
    struct Voice { double d, rate; int hold = 0; };     // hold: samples at normal speed until the hit itself arrives
    void splice (double seconds) { old = cur; cur = { std::max ((double) lat, guard), ratio, 0 }; xf = 0; xfStep = 1.0 / std::max (1.0, seconds * sr); }
    void step (Voice& v) const
    {
        if (v.hold > 0) { --v.hold; return; }
        double r = v.rate;
        if (r > 1) r = 1 + (r - 1) * std::clamp ((v.d - guard) / (0.008 * sr), 0.0, 1.0);       // caught up: ease back to normal speed
        v.d = std::max (guard, v.d + 1.0 - r);
    }
    float tap (int ch, double delay) const       // the sample `delay` ago, 4-point Hermite
    {
        const auto& b = buf[(size_t) ch];
        const double pos = (double) w - delay + (double) N;
        const int i = (int) pos; const float t = (float) (pos - i);
        const float xm = b[(size_t) ((i - 1) & (N - 1))], x0 = b[(size_t) (i & (N - 1))], x1 = b[(size_t) ((i + 1) & (N - 1))], x2 = b[(size_t) ((i + 2) & (N - 1))];
        const float c1 = .5f * (x1 - xm), c2 = xm - 2.5f * x0 + 2 * x1 - .5f * x2, c3 = .5f * (x2 - xm) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }
    std::vector<std::vector<float>> buf; double sr = 48000, ratio = 1, xf = 1, xfStep = 0, guard = 3; int N = 1, w = 0, lat = 0; Voice cur { 3, 1, 0 }, old { 3, 1, 0 };
};

// ---------- the song's key, from the sidechain ----------
// Adds up how strong each of the 12 notes is (130 Hz to 2 kHz) and matches that against the usual major and minor
// shapes (Krumhansl-Kessler). It is a guess that gets better with a few bars; the key setting overrides it.
class KeyFinder
{
public:
    void reset() { for (auto& c : chroma) c = 0; }
    void add (const float* mags, int bins, double binHz, double keep = 0.985)
    {
        for (auto& c : chroma) c *= keep;
        for (int i = std::max (1, (int) (130.0 / binHz)); i < bins && i * binHz < 2000.0; ++i)
        {
            const int pc = ((int) std::lround (69 + 12 * std::log2 (i * binHz / 440.0)) % 12 + 12) % 12;
            chroma[pc] += (double) mags[i] * mags[i];
        }
    }
    bool key (int& tonic, bool& minor, double& sure) const
    {
        static const double maj[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static const double mnr[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
        double c[12], total = 0; for (int i = 0; i < 12; ++i) { c[i] = std::sqrt (chroma[i]); total += c[i]; }
        if (total < 1e-9) return false;
        double best = -2, second = -2;
        for (int t = 0; t < 12; ++t)
            for (int m = 0; m < 2; ++m)
            {
                const double* pr = m ? mnr : maj; double mx = 0, my = 0, sxy = 0, sxx = 0, syy = 0;
                for (int i = 0; i < 12; ++i) { mx += pr[i]; my += c[i]; }
                mx /= 12; my /= 12;
                for (int i = 0; i < 12; ++i) { const double a = pr[i] - mx, b = c[(i + t) % 12] - my; sxy += a * b; sxx += a * a; syy += b * b; }
                const double r = sxx > 0 && syy > 0 ? sxy / std::sqrt (sxx * syy) : 0;
                if (r > best) { second = best; best = r; tonic = t; minor = m == 1; } else if (r > second) second = r;
            }
        sure = best - second; return true;
    }
private:
    double chroma[12] {};
};

// the nearest note of the key to `hz`: how many semitones to move (with cents). Ties go down, which needs no latency.
inline double tuneToKey (double hz, int tonic, bool minor, int* notePc = nullptr)
{
    static const int majS[7] = { 0, 2, 4, 5, 7, 9, 11 }, minS[7] = { 0, 2, 3, 5, 7, 8, 10 };
    const double midi = 69 + 12 * std::log2 (hz / 440.0);
    double best = 99; int bestPc = 0;
    for (int o = -1; o <= 1; ++o)
        for (int i = 0; i < 7; ++i)
        {
            const int pc = (tonic + (minor ? minS : majS)[i]) % 12;
            const double target = 12 * (std::floor (midi / 12) + o) + pc, move = target - midi;
            if (std::abs (move) < std::abs (best) - 1e-9 || (std::abs (std::abs (move) - std::abs (best)) < 1e-9 && move < best)) { best = move; bestPc = pc; }
        }
    if (notePc) *notePc = bestPc;
    return best;
}

struct Params
{
    int role = 0;                 // 0 on the kick, 1 on the bass
    float thump = 0, punch = 0, click = 0, tail = 0;    // -1..1, 0 changes nothing
    float room = 0;               // 0..1: how far the bass's lows dip under the kick (role 1), up to 12 dB
    float tune = 0;               // semitones, 0 = off
    float gainDb = 0;             // set by Sit it, or by hand
    float duckHz = 120, duckRelease = 0.12f;
};

// what one hit looked like, for the picture: level every 3 ms for 384 ms, kick lows and sidechain lows
struct HitView { static constexpr int n = 128; float kick[n] {}, side[n] {}; int count = 0; };

class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        onsetIn.prepare (sr); onsetOut.prepare (sr); onsetSide.prepare (sr); pitch.prepare (sr); retune.prepare (sr, 2);
        envFast.time (0.008, sr); envSlow.time (0.06, sr); trSmooth.time (0.002, sr); tailSmooth.time (0.002, sr); gainSmooth.time (0.02, sr);
        peakDecay = std::exp (-1.0 / (1.5 * sr)); peakHold = 0; since = 1 << 28; fast = 0; slowY = 0; slowUp.time (0.015, sr);
        for (int c = 0; c < 2; ++c)
        {
            punchBp[c].bandpass (350, 0.45, sr); clickHp[c].highpass (2500, .707, sr); harmLp[c].lowpass (200, .707, sr); harmHp[c].highpass (180, .707, sr);
            duckShelf[c].lowShelf (p.duckHz, 1.0, sr); duckShelf[c].reset();
            for (auto* b : { &punchBp[c], &clickHp[c], &harmLp[c], &harmHp[c], &thumpEq[c] }) b->reset();
        }
        meterLow1.highpass (30, .707, sr); meterLow2.lowpass (150, .707, sr); sideLow1.highpass (30, .707, sr); sideLow2.lowpass (150, .707, sr); duckDet1.highpass (30, .707, sr); duckDet2.lowpass (150, .707, sr);
        for (auto* b : { &kLow3a, &sLow3a }) { b->lowpass (300, .54, sr); b->reset(); } for (auto* b : { &kLow3b, &sLow3b }) { b->lowpass (300, 1.31, sr); b->reset(); }
        duckEnv.y = 0; duckRef = 0; duckSet = 1; duckTick = 0; duckRefDecay = std::exp (-1.0 / (3.0 * sr)); duckAtk = std::exp (-1.0 / (0.001 * sr));
        win = (int) (0.12 * sr); viewStep = std::max (1, (int) (0.003 * sr)); viewDecay = std::exp (-1.0 / (0.02 * sr)); nHits = 0; hitW = 0; accOn = false; viewPos = HitView::n; thumpHz = 0; thumpDb = 0;
        gainSmooth.y = dbToGain (p.gainDb); tailSmooth.y = 1; applied = p; retune.setRatio (ratioOf (p.tune)); retune.prepare (sr, 2); setThump (true);
    }
    void setParams (const Params& n)
    {
        if (n.tune != p.tune) retune.setRatio (ratioOf (n.tune));
        if (n.duckHz != p.duckHz) duckSet = -1;
        p = n;
    }
    const Params& params() const { return p; }
    int latency() const { return p.role == 0 ? retune.latency() : 0; }
    static double ratioOf (double semis) { return std::pow (2.0, semis / 12.0); }

    // l, r: this channel (changed in place). sl, sr_: the sidechain, or null when nothing is connected.
    void process (float* l, float* r, const float* sl, const float* sr_, int n)
    {
        if (p.role == 1) { duck (l, r, sl, sr_, n); return; }
        setThump (false);
        const bool tuning = std::abs (p.tune) > 1e-4f || retune.latency() > 0;
        const bool shaping = p.thump != 0 || p.punch != 0 || p.click != 0 || p.tail != 0 || std::abs (p.gainDb) > 1e-4f || std::abs (gainSmooth.y - 1.0) > 1e-5 || std::abs (tailSmooth.y - 1.0) > 1e-5;
        const double gP = dbToGain (p.punch * 8.0) - 1, gC = dbToGain (p.click * 9.0) - 1, harm = std::max (0.0f, p.click) * 0.7, gTarget = dbToGain (p.gainDb);
        for (int i = 0; i < n; ++i)
        {
            double x[2] = { l[i], r[i] };
            const double in = 0.5 * (x[0] + x[1]);
            const bool hitIn = onsetIn.run (in);
            if (hitIn) pitch.onset();
            pitch.push (in);
            if (tuning)
            {
                retune.write (0, (float) x[0]); retune.write (1, (float) x[1]);
                if (hitIn) retune.hit();
                x[0] = retune.read (0); x[1] = retune.read (1);
                retune.advance();
            }
            const double m = 0.5 * (x[0] + x[1]), a = std::abs (m);
            const bool hit = tuning ? onsetOut.run (m) : hitIn;
            if (hit) { since = 0; ++hitCount; if (tailSmooth.y < 1) tailSmooth.y = 1; }      // a shortened tail never eats the next hit
            else if (since < (1 << 28)) ++since;
            if (shaping)
            {
                fast = zap (std::max (a, fast * envFast.a));                                  // peak, falls in 8 ms
                slowY = zap (a + (a > slowY ? slowUp.a : envSlow.a) * (slowY - a));            // body: up in 15 ms, down in 60
                const double slow = slowY;
                peakHold = std::max (a, peakHold * peakDecay);
                const double trRaw = std::clamp ((fast - slow) / (fast + 1e-9), 0.0, 1.0);
                const double tr = trRaw > trSmooth.y ? (trSmooth.y = trRaw) : trSmooth.run (trRaw);
                const double t = since / sr - 0.05;
                double tailDb = 0;
                if (p.tail < 0) tailDb = std::max (-48.0, p.tail * 120.0 * std::max (0.0, t));
                else if (p.tail > 0 && slow > 1e-3 * peakHold && since < (1 << 28)) tailDb = std::min (12.0, p.tail * 50.0 * std::max (0.0, t));
                const double g = tailSmooth.run (dbToGain (tailDb)) * gainSmooth.run (gTarget);
                for (int c = 0; c < 2; ++c)
                {
                    const double dry = x[c];
                    double y = p.thump != 0 || thumpDb != 0 ? thumpEq[c].run (dry) : dry;
                    if (gP != 0) y += gP * tr * punchBp[c].run (dry);
                    if (gC != 0) y += gC * tr * clickHp[c].run (dry);
                    if (harm > 0)
                    {
                        const double pk = peakHold + 1e-9;
                        const double u = harmLp[c].run (dry) / pk;
                        y += harm * pk * harmHp[c].run (std::tanh (3.0 * u) + 0.8 * std::abs (u));
                    }
                    y *= g;
                    const double ay = std::abs (y);                       // ceiling: nothing leaves above full scale
                    if (ay > knee) y = (y < 0 ? -1 : 1) * (knee + (1 - knee) * std::tanh ((ay - knee) / (1 - knee)));
                    x[c] = y;
                }
            }
            l[i] = (float) x[0]; r[i] = (float) x[1];
            meter (hit, 0.5 * (x[0] + x[1]), sl ? 0.5 * ((double) sl[i] + (sr_ ? sr_[i] : sl[i])) : 0.0);
        }
    }

    // ---------- what the picture and Sit it read ----------
    double note() const { return pitch.settled(); }                 // Hz of the kick as it comes in; x ratioOf(tune) after tuning
    double noteNow() const { return pitch.current(); }
    double noteShown() const { return pitch.steadyNote(); }             // steadier, for the window
    double glideSemis() const { return pitch.glideSemis(); }
    double noteSpreadCents() const { return pitch.lastSpread(); }       // above about 40: the kick has no single note
    int hitsSeen() const { return hitCount; }
    bool hasSidechain() const { return sideAge < (int) (sr * 3); }       // something came in on the other input in the last 3 s
    // kick lows (30-150 Hz) against everything under 300 Hz in kick + sidechain, dB, middle of the last hits.
    // Same measure as kick_low_vs_lows_db in tools/kick_analyse.py; lows only, so it works with just a bass or a whole song.
    bool sitLevel (double& dB) const
    {
        if (nHits < 4) return false;
        const Hit h = middle(); if (h.mix <= 1e-12 || h.low <= 1e-14) return false;
        dB = 10 * std::log10 (h.low / h.mix); return true;
    }
    // the gain change (dB) that puts the kick at `target`; nothing if it already sits between lo and hi. false = can't tell.
    bool sitGain (double lo, double target, double hi, double& changeDb) const
    {
        double now; if (! sitLevel (now) || ! hasSidechain()) return false;
        changeDb = 0; if (now >= lo && now <= hi) return true;
        const Hit h = middle(); const double T = std::pow (10.0, target / 10.0), den = h.low - T * h.full;
        if (h.side <= 1e-12) return false;
        changeDb = den > 1e-14 ? std::clamp (10 * std::log10 (T * h.side / den), -24.0, 24.0) : 24.0;   // a kick with few lows can't get there: stop at +24
        return true;
    }
    const HitView& view() const { return hv; }
    double duckNowDb() const { return gainToDb (duckGainNow); }

private:
    struct Hit { double low = 0, full = 0, side = 0, mix = 0; };      // kick 30-150 Hz; kick, sidechain and both together under 300 Hz
    Hit middle() const
    {
        const int cnt = std::clamp (nHits, 1, maxHits);
        auto med = [this, cnt] (double Hit::* f) { double v[maxHits]; for (int i = 0; i < cnt; ++i) v[i] = hits[i].*f; std::nth_element (v, v + cnt / 2, v + cnt); return v[cnt / 2]; };
        return { med (&Hit::low), med (&Hit::full), med (&Hit::side), med (&Hit::mix) };
    }
    void meter (bool hit, double k, double s)
    {
        const double kl = meterLow2.run (meterLow1.run (k)), sLow = sideLow2.run (sideLow1.run (s));
        if (std::abs (s) > 1e-5) sideAge = 0; else if (sideAge < (1 << 30)) ++sideAge;
        if (hit) { if (accOn && accN >= win) store(); acc = {}; accN = 0; accOn = true; viewPos = 0; viewN = 0; vk = vs = 0; ++hv.count; }
        if (accOn && accN < win)
        {
            const double k3 = kLow3b.run (kLow3a.run (k)), s3 = sLow3b.run (sLow3a.run (s));        // everything under 300 Hz, each side
            acc.low += kl * kl; acc.full += k3 * k3; acc.side += s3 * s3; acc.mix += (k3 + s3) * (k3 + s3);
            if (++accN == win) store();
        }
        if (viewPos < HitView::n)
        {
            ek = std::max (std::abs (kl), ek * viewDecay); es = std::max (std::abs (sLow), es * viewDecay);      // outline, not the wave itself
            vk = std::max (vk, ek); vs = std::max (vs, es);
            if (++viewN >= viewStep) { hv.kick[viewPos] = (float) vk; hv.side[viewPos] = (float) vs; ++viewPos; viewN = 0; vk = vs = 0; }
        }
    }
    void store()
    {
        Hit h { acc.low / win, acc.full / win, acc.side / win, acc.mix / win };
        hits[hitW] = h; hitW = (hitW + 1) % maxHits; nHits = std::min (nHits + 1, maxHits); accOn = false;
    }
    void setThump (bool force)
    {
        const double f = std::clamp (pitch.heldNote() > 0 ? pitch.heldNote() * ratioOf (p.tune) : 55.0, 30.0, 160.0), dB = p.thump * 6.0;
        if (! force && std::abs (dB - thumpDb) < 1e-6 && std::abs (f - thumpHz) < 1.0) return;
        thumpDb += std::clamp (dB - thumpDb, -0.5, 0.5);                 // at most half a dB a block: no zipper noise
        thumpHz = f;
        for (int c = 0; c < 2; ++c) thumpEq[c].peak (thumpHz, 1.0, thumpDb, sr);
    }
    // role 1: this channel is the bass, the sidechain is the kick. A low shelf at duckHz that dips with the kick: above it nothing changes,
    // and at rest the shelf is exactly a wire.
    void duck (float* l, float* r, const float* sl, const float* sr_, int n)
    {
        const double depth = 1 - dbToGain (-12.0 * std::clamp (p.room, 0.0f, 1.0f)), rel = std::exp (-1.0 / (std::max (0.01f, p.duckRelease) * sr));
        for (int i = 0; i < n; ++i)
        {
            const double s = sl ? 0.5 * ((double) sl[i] + (sr_ ? sr_[i] : sl[i])) : 0.0;
            const double e = std::abs (duckDet2.run (duckDet1.run (s)));
            const bool hit = onsetSide.run (s); if (hit) ++hitCount;
            duckEnv.y = zap (e > duckEnv.y ? e + duckAtk * (duckEnv.y - e) : e + rel * (duckEnv.y - e));
            duckRef = std::max (duckEnv.y, zap (duckRef * duckRefDecay));
            const double amt = duckRef > 1e-5 ? std::min (1.0, duckEnv.y / duckRef) : 0.0;
            const double g = 1 - depth * amt; duckGainNow = g;
            if ((duckTick++ & 7) == 0 && g != duckSet) { duckSet = g; duckShelf[0].lowShelf (p.duckHz, g, sr); duckShelf[1].lowShelf (p.duckHz, g, sr); }
            // at rest and rung out: a wire, to the last bit (a compiler may round the idle filter differently)
            const bool idle = duckSet == 1.0 && std::abs (duckShelf[0].z1) + std::abs (duckShelf[0].z2) + std::abs (duckShelf[1].z1) + std::abs (duckShelf[1].z2) < 1e-9;
            if (! idle) { l[i] = (float) duckShelf[0].run (l[i]); r[i] = (float) duckShelf[1].run (r[i]); }
            meter (hit, s, 0.5 * ((double) l[i] + r[i]));          // the kick (sidechain) against the bass (this channel), as on the kick
        }
    }

    double sr = 48000; Params p, applied;
    Onset onsetIn, onsetOut, onsetSide; PitchTracker pitch; Retune retune;
    OnePole envFast, envSlow, slowUp, trSmooth, tailSmooth, gainSmooth, duckEnv;
    Biquad thumpEq[2], punchBp[2], clickHp[2], harmLp[2], harmHp[2], meterLow1, meterLow2, sideLow1, sideLow2, duckDet1, duckDet2, duckShelf[2], kLow3a, kLow3b, sLow3a, sLow3b;
    double peakHold = 0, peakDecay = 0, fast = 0, thumpHz = 0, thumpDb = 0, duckRef = 0, duckRefDecay = 0, duckAtk = 0, duckGainNow = 1, duckSet = 1, vk = 0, vs = 0, ek = 0, es = 0, viewDecay = 0; unsigned duckTick = 0;
    const double knee = 0.89;
    int since = 0, hitCount = 0, win = 1, accN = 0, viewStep = 1, viewPos = 0, viewN = 0; bool accOn = false; int sideAge = 1 << 30;
    static constexpr int maxHits = 12;
    Hit acc, hits[maxHits]; int nHits = 0, hitW = 0; double slowY = 0; HitView hv;
};
}
