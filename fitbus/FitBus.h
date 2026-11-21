#pragma once
// Fit Bus: the processing, with no JUCE in it (tested by tests/fitbus_test.cpp).
// A limiter for the bus the kick, bass and vocal meet on, plus what the window needs to draw them:
// the level over time, split into six ranges of pitch so each column can be coloured from red (lowest) to deep blue (highest).
//
// The limiter looks 5 ms ahead. For every sample it works out how far the level must come down to stay under the ceiling,
// takes the lowest of those over the next 5 ms, lets that recover slowly, then averages it over 5 ms so the gain moves smoothly.
// Each averaged value is made only from gains that are low enough for the sample it is applied to, so nothing passes the ceiling.
// Below the ceiling the gain is exactly 1: the sound comes out untouched, only 5 ms late (the host is told, and lines it up).
#include "../kickfit/KickFit.h"
#include "../kickfit/FitLink.h"
#include <atomic>
#include <vector>

namespace fb
{
constexpr int numBands = 6;
// upper edge of each range, Hz (the last has none): sub, bass, low mids, mids, presence, air
constexpr double bandEdges[numBands - 1] = { 60, 150, 400, 1500, 5000 };

struct Column { float in = 0, out = 0, gain = 1, band[numBands] {}; };      // one slice of the picture (20 ms): peaks before and after, lowest gain, level per range

struct Params
{
    float gainDb = 0, ceilingDb = 0, releaseMs = 120;
    float attack = 0, sustain = 0;                       // transients, -1..1: the front of each hit, and the body after it
    float tone[numBands] {}; bool toneOn = true;         // the tone change: dB per range, and the A/B switch (off = hear it without, at the same loudness)
    int tameRange = 0; float tameThrDb = -24, tameAmount = 0; bool tameHear = false;      // one range held down when it jumps out: 0 off, 1..6 = the six ranges
};

// follows a level: quick or slow to rise, quick or slow to fall
struct Follower
{
    double y = 0, up = 0, down = 0;
    void set (double riseMs, double fallMs, double sr) { up = std::exp (-1.0 / (riseMs * 0.001 * sr)); down = std::exp (-1.0 / (fallMs * 0.001 * sr)); }
    double run (double x) { y = kf::zap (x + (x > y ? up : down) * (y - x)); return y; }
};

// Transients. First the outline of the sound (it rises at once and falls over 40 ms, so the wave's own ups and downs
// don't count as hits). Then two pairs of followers on that outline: one pair differs only in how fast it rises, so the gap
// between them opens at the front of a hit; the other differs only in how fast it falls, so its gap opens in the body after
// a hit. Whatever gap a steady sound leaves open is learned and taken away, and the body's gap is ignored while a front is
// landing. Attack turns the level up or down by the first gap, Sustain by the second. With both at 0 nothing is touched.
class Transient
{
public:
    void prepare (double sr)
    {
        outline.set (0.01, 40, sr); quickUp.set (1, 40, sr); slowUp.set (20, 40, sr); slowDown.set (5, 400, sr); quickDown.set (5, 60, sr);
        ease = 1 - std::exp (-1.0 / (0.001 * sr)); learn = 1 - std::exp (-1.0 / (0.7 * sr)); gDb = gFront = gBody = 0; restFront = restBody = 0;
        for (auto* f : { &outline, &quickUp, &slowUp, &slowDown, &quickDown }) f->y = 0;
    }
    void set (float attack, float sustain) { atk = std::clamp (attack, -1.0f, 1.0f); sus = std::clamp (sustain, -1.0f, 1.0f); }
    void run (float& l, float& r)
    {
        if (atk == 0 && sus == 0 && std::abs (gDb) < 1e-4) { gDb = gFront = gBody = 0; return; }
        const double was = quickDown.y, x = outline.run (std::max (std::abs ((double) l), std::abs ((double) r))), fl_ = 1e-4;       // the floor keeps silence from looking like a hit
        double front = 20 * std::log10 ((quickUp.run (x) + fl_) / (slowUp.run (x) + fl_)), body = 20 * std::log10 ((slowDown.run (x) + fl_) / (quickDown.run (x) + fl_));
        restFront += learn * (std::min (front, 1.5) - restFront); restBody += learn * (std::min (body, 1.5) - restBody);          // what a steady sound leaves open (a hit can't drag these far)
        front = std::clamp (front - restFront, 0.0, 12.0); body = std::clamp (body - restBody, 0.0, 12.0) * std::clamp (1 - front / 2, 0.0, 1.0);
        if (x > 1.4 * was + fl_) { body = 0; gBody = 0; }          // a new hit is landing this very sample: the body's share lets go at once, not a millisecond late
        gFront += ease * (0.75 * atk * front - gFront); gBody += ease * (0.75 * sus * body - gBody); gDb = std::clamp (gFront + gBody, -12.0, 12.0);
        const double g = std::pow (10.0, gDb / 20); l = (float) (l * g); r = (float) (r * g);
    }
private:
    Follower outline, quickUp, slowUp, slowDown, quickDown; double ease = 0.02, learn = 0, gDb = 0, gFront = 0, gBody = 0, restFront = 0, restBody = 0; float atk = 0, sus = 0;
};

// The tone change: one gentle equaliser band per colour range (a shelf at the bottom and top, bells in between), each with
// its own gain in dB. Gains move a little at a time so nothing clicks. With every gain at 0 the sound is not touched at all.
// A/B: switched off, the bands go flat and the whole sound is turned up or down by what the tone change did to the overall
// level (worked out from how strong each range is), so "with" and "without" are equally loud and only the tone differs.
class ToneEq
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate; for (int b = 0; b < numBands; ++b) { cur[b] = 0; want[b] = 0; shape (b); for (auto& f : eq[b]) f.reset(); }
        match = wantMatch = 0; matchGain = 1; tick = 0;
    }
    // dB per range; power = how strong each range is on the way in (for the matched loudness of "without")
    void set (const float* dB, bool on, const double* power)
    {
        double with = 0, all = 0;
        for (int b = 0; b < numBands; ++b) { const double g = std::clamp ((double) dB[b], -12.0, 12.0); want[b] = on ? g : 0.0; with += std::pow (10.0, g / 10) * power[b]; all += power[b]; }
        wantMatch = on || all <= 1e-12 ? 0.0 : std::clamp (10 * std::log10 (with / all), -8.0, 8.0);
    }
    void run (float& l, float& r)
    {
        if ((tick++ & 7) == 0)
        {
            for (int b = 0; b < numBands; ++b) if (cur[b] != want[b]) { cur[b] += std::clamp (want[b] - cur[b], -0.05, 0.05); shape (b); }
            if (match != wantMatch) { match += std::clamp (wantMatch - match, -0.05, 0.05); matchGain = kf::dbToGain (match); }
        }
        bool flat = true; double ring = 0;
        for (int b = 0; b < numBands; ++b) { flat = flat && cur[b] == 0; ring += std::abs (eq[b][0].z1) + std::abs (eq[b][0].z2) + std::abs (eq[b][1].z1) + std::abs (eq[b][1].z2); }
        if (! (flat && ring < 1e-9))
        {
            double x = l, y = r; for (int b = 0; b < numBands; ++b) { x = eq[b][0].run (x); y = eq[b][1].run (y); }
            l = (float) (x * post); r = (float) (y * post);
        }
        if (match != 0) { l = (float) (l * matchGain); r = (float) (r * matchGain); }
    }
private:
    void shape (int b)
    {
        const double lo = b == 0 ? 0.0 : bandEdges[b - 1], hi = b == numBands - 1 ? 0.0 : bandEdges[b], g = kf::dbToGain (cur[b]);
        for (auto& f : eq[b])
        {
            if (lo <= 0) f.lowShelf (hi, g, sr);
            else if (hi <= 0) { f.lowShelf (lo, 1 / g, sr); post = g; }             // top range: lows by 1/g, everything by g
            else f.peak (std::sqrt (lo * hi), std::sqrt (lo * hi) / (hi - lo), cur[b], sr);
        }
    }
    double sr = 44100, cur[numBands] {}, want[numBands] {}, post = 1, match = 0, wantMatch = 0, matchGain = 1; int tick = 0; kf::Biquad eq[numBands][2];
};

// One range held down when it jumps out (what a de-esser does for "s" sounds; the same idea tames a boomy or harsh range).
// It listens to that range only. When the range goes over the threshold, an equaliser dip (a shelf at the bottom or top
// range, a bell in between) turns just that range down, by more the further over it goes; everything else passes. Under
// the threshold the equaliser is flat and switched out, so the sound is not touched at all.
class Tame
{
public:
    void prepare (double sampleRate) { sr = sampleRate; lev.set (2, 100, sr); lev.y = 0; red = setRed = 0; tick = 0; build(); for (auto& f : eq) f.reset(); for (auto& f : det) f.reset(); }
    void set (int range_, float thrDb, float amount, bool hear_)
    {
        const int rg = std::clamp (range_, 0, numBands); if (rg != range) { range = rg; build(); setRed = -1; }
        thr = thrDb; ratio = 1 + 9 * std::clamp (amount, 0.0f, 1.0f); hear = hear_;
    }
    float reductionDb() const { return -(float) red; }
    void run (float& l, float& r)
    {
        if (range == 0) { red = 0; return; }
        const double d = det[1].run (det[0].run (0.5 * ((double) l + r)));
        const double over = 20 * std::log10 (lev.run (std::abs (d)) + 1e-9) - thr;
        red = over > 0 ? std::min (18.0, over * (1 - 1 / ratio)) : 0.0;
        if ((tick++ & 7) == 0 && std::abs (red - setRed) > (red == 0 ? 0.0 : 0.05)) { setRed = red; shape (red); }
        const bool idle = setRed == 0 && std::abs (eq[0].z1) + std::abs (eq[0].z2) + std::abs (eq[1].z1) + std::abs (eq[1].z2) < 1e-9;
        if (! idle) { l = (float) (post * eq[0].run (l)); r = (float) (post * eq[1].run (r)); }
        if (hear) l = r = (float) d;                                    // listen to just the range it is watching
    }
private:
    double lo() const { return range <= 1 ? 0.0 : bandEdges[range - 2]; }
    double hi() const { return range >= numBands ? 0.0 : bandEdges[range - 1]; }
    void build()
    {
        det[0] = {}; det[1] = {};
        if (range == 0) return;
        if (lo() > 0) det[0].highpass (lo(), 0.707, sr);
        if (hi() > 0) det[1].lowpass (hi(), 0.707, sr);
    }
    void shape (double dB)                                              // the dip: dB down in the range
    {
        const double g = kf::dbToGain (-dB); post = 1;
        for (auto& f : eq)
        {
            if (lo() <= 0) f.lowShelf (hi(), g, sr);                                    // bottom range: a low shelf
            else if (hi() <= 0) { f.lowShelf (lo(), 1 / g, sr); post = g; }             // top range: lows up by 1/g, everything times g = highs down
            else f.peak (std::sqrt (lo() * hi()), std::sqrt (lo() * hi()) / (hi() - lo()), -dB, sr);
        }
    }
    double sr = 44100, thr = -24, ratio = 1, red = 0, setRed = 0, post = 1; int range = 0, tick = 0; bool hear = false;
    Follower lev; kf::Biquad eq[2], det[2];
};

// How much of each part (kick, bass, vocal) is in the bus. Each Kick Fit says how strong its own output is in six ranges of
// pitch; the bus measures itself the same way. The bus should be those parts added together, each times its own gain
// (the mixer fader and anything else between Kick Fit and here). This finds the three gains that fit best over the last
// several seconds: least squares on the strengths, never below zero. `fit` says how much of the bus the parts explain
// (near 1 = the bus is just these parts; low = there is a lot else in it, or the parts cannot be told apart).
struct Mixer
{
    static constexpr int parts = 4;          // kick, bass, vocal, everything else
    void reset() { for (auto& row : A) for (auto& v : row) v = 0; for (auto& v : r) v = 0; c = 0; for (auto& row : sP) for (auto& v : row) v = 0; for (auto& v : sB) v = 0; age = 0; }
    // P: each part's strengths as its Kick Fit reports them; Bnow: the bus's; dt: seconds since the last call.
    // Both are first averaged over about 1.5 s: the bus hears the parts a few ms late (the host lines plug-ins up, and the
    // numbers are read at slightly different moments), which matters at the instant a kick lands and not over a second.
    void add (const double Pnow[parts][fl::numBands], const bool have[parts], const double Bnow[fl::numBands], double dt)
    {
        const double a = 1 - std::exp (-dt / slow), keep = std::exp (-dt / memory);
        for (int b = 0; b < fl::numBands; ++b) { sB[b] += a * (Bnow[b] - sB[b]); for (int i = 0; i < parts; ++i) sP[i][b] += a * ((have[i] ? Pnow[i][b] : 0.0) - sP[i][b]); }
        age += dt; if (age < warm) return;          // the first moments after play starts are not averaged yet: measured in FL, counting them kept the fit low for 10 s
        const auto& P = sP; const auto& B = sB;
        double tot = 0; for (int b = 0; b < fl::numBands; ++b) tot += B[b];
        if (tot <= 1e-12) return;
        for (auto& row : A) for (auto& v : row) v *= keep;
        for (auto& v : r) v *= keep;
        c *= keep;
        for (int b = 0; b < fl::numBands; ++b)
        {
            const double d = B[b] + 0.02 * tot, w = 1 / (d * d);          // every range counts about the same, quiet ones a little less
            for (int i = 0; i < parts; ++i) if (have[i]) { for (int j = 0; j < parts; ++j) if (have[j]) A[i][j] += w * P[i][b] * P[j][b]; r[i] += w * P[i][b] * B[b]; }
            c += w * B[b] * B[b];
        }
    }
    // gain2[i] = that part's gain, squared. false = nothing to go on yet.
    bool solve (const bool have[parts], double gain2[parts], double& fit) const
    {
        for (int i = 0; i < parts; ++i) gain2[i] = 0;
        fit = 0; if (c <= 0) return false;
        double best = c;
        for (int mask = 1; mask < (1 << parts); ++mask)
        {
            int idx[parts], n = 0; bool ok = true;
            for (int i = 0; i < parts; ++i) if (mask & (1 << i)) { if (! have[i]) ok = false; idx[n++] = i; }
            if (! ok) continue;
            double M[parts][parts + 1];
            for (int a = 0; a < n; ++a) { for (int b = 0; b < n; ++b) M[a][b] = A[idx[a]][idx[b]]; M[a][n] = r[idx[a]]; }
            for (int a = 0; a < n && ok; ++a)                              // Gaussian elimination with pivoting
            {
                int pv = a; for (int k = a + 1; k < n; ++k) if (std::abs (M[k][a]) > std::abs (M[pv][a])) pv = k;
                if (std::abs (M[pv][a]) < 1e-12) { ok = false; break; }
                for (int k = 0; k <= n; ++k) std::swap (M[a][k], M[pv][k]);
                for (int k = 0; k < n; ++k) if (k != a) { const double f = M[k][a] / M[a][a]; for (int q = a; q <= n; ++q) M[k][q] -= f * M[a][q]; }
            }
            if (! ok) continue;
            double x[parts], res = c; for (int a = 0; a < n; ++a) { x[a] = M[a][n] / M[a][a]; if (x[a] < 0) ok = false; res -= x[a] * r[idx[a]]; }
            if (! ok || res >= best) continue;
            best = res; for (int i = 0; i < parts; ++i) gain2[i] = 0;
            for (int a = 0; a < n; ++a) gain2[idx[a]] = x[a];
        }
        fit = std::clamp (1 - best / c, 0.0, 1.0); return true;
    }
    double slow = 1.5, memory = 4.0, warm = 2.0;          // seconds: the averaging, how fast old moments fade, and how long to listen before counting
private:
    double A[parts][parts] {}, r[parts] {}, c = 0, sP[parts][fl::numBands] {}, sB[fl::numBands] {}, age = 0;
};

// Loudness the way finished songs are measured (ITU-R BS.1770, the "LUFS" figure): both sides filtered to weigh pitches as
// the ear does, the energy of every 0.4 s (a new one each 0.1 s), stretches under -70 left out, then those more than 10
// under the rest left out, and the average of what remains. Loudness::lufs works that out from the 0.4 s energies.
struct Loudness
{
    static constexpr int size = 1200;                       // two minutes of 0.1 s steps
    void prepare (double sr)
    {
        for (auto& f : shelf) weigh (f, hp[&f - shelf], sr);
        step = std::max (1, (int) (0.1 * sr)); n = 0; e = 0; for (auto& v : last) v = 0; w.store (0); count.store (0);
    }
    void clear() { count.store (0); }
    void push (float l, float r)
    {
        const double a = hp[0].run (shelf[0].run (l)), b = hp[1].run (shelf[1].run (r)); e += a * a + b * b;
        if (++n < step) return;
        last[3] = last[2]; last[2] = last[1]; last[1] = last[0]; last[0] = e / n; e = 0; n = 0;
        const int i = w.load (std::memory_order_relaxed); ring[i] = (float) (0.25 * (last[0] + last[1] + last[2] + last[3]));
        w.store ((i + 1) % size, std::memory_order_release); if (count.load() < size) count.fetch_add (1);
    }
    // the newest `seconds` of it (0 = all that is kept). false = not enough sound yet.
    bool read (double& lufsOut, double seconds = 0) const
    {
        float v[size]; int c = count.load(); if (seconds > 0) c = std::min (c, (int) (seconds * 10)); const int end = w.load (std::memory_order_acquire);
        for (int i = 0; i < c; ++i) v[i] = ring[((end - c + i) % size + size) % size];
        return lufs (v, c, lufsOut);
    }
    static bool lufs (const float* z, int c, double& out)
    {
        auto lk = [] (double ms) { return -0.691 + 10 * std::log10 (ms + 1e-12); };
        double sum = 0; int k = 0; for (int i = 0; i < c; ++i) if (lk (z[i]) > -70) { sum += z[i]; ++k; }
        if (k < 10) return false;                             // under a second of sound
        const double gate = lk (sum / k) - 10; sum = 0; k = 0;
        for (int i = 0; i < c; ++i) if (lk (z[i]) > -70 && lk (z[i]) > gate) { sum += z[i]; ++k; }
        if (k == 0) return false;
        out = lk (sum / k); return true;
    }
private:
    static void weigh (kf::Biquad& sh, kf::Biquad& h, double sr)
    {
        double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        double K = std::tan (kf::pi * f0 / sr); const double Vh = std::pow (10.0, G / 20), Vb = std::pow (Vh, 0.4996667741545416);
        sh.set (Vh + Vb * K / Q + K * K, 2 * (K * K - Vh), Vh - Vb * K / Q + K * K, 1 + K / Q + K * K, 2 * (K * K - 1), 1 - K / Q + K * K);
        f0 = 38.13547087602444; Q = 0.5003270373238773; K = std::tan (kf::pi * f0 / sr);
        h.set (1, -2, 1, 1 + K / Q + K * K, 2 * (K * K - 1), 1 - K / Q + K * K); sh.reset(); h.reset();
    }
    kf::Biquad shelf[2], hp[2]; double e = 0, last[4] {}; int step = 4410, n = 0; float ring[size] {}; std::atomic<int> w { 0 }, count { 0 };
};

class Engine
{
public:
    static constexpr int numColumns = 512;

    void prepare (double sampleRate)
    {
        sr = sampleRate; L = std::max (8, (int) std::lround (0.005 * sr));
        dl.assign ((size_t) L, 0.0f); dr.assign ((size_t) L, 0.0f); box.assign ((size_t) L, 1.0); dqV.assign ((size_t) L + 1, 1.0); dqI.assign ((size_t) L + 1, 0);
        pos = 0; n = 0; dqHead = dqTail = 0; rel = 1; boxSum = L; lowered = 0; colStep = std::max (1, (int) (0.02 * sr)); colN = 0; cur = {}; cur.gain = 1;
        for (int i = 0; i < numBands - 1; ++i) { lpA[i].lowpass (bandEdges[i], 0.5412, sr); lpB[i].lowpass (bandEdges[i], 1.3066, sr); lpA[i].reset(); lpB[i].reset(); }
        for (auto& e : bandE) e = 0;
        inMeter.prepare (sr); outMeter.prepare (sr); toneEq.prepare (sr); transient.prepare (sr); tame.prepare (sr); loud.prepare (sr);
        set (p);
    }
    void set (const Params& np) { p = np; toneEq.set (p.tone, p.toneOn, inMeter.power); transient.set (p.attack, p.sustain); tame.set (p.tameRange, p.tameThrDb, p.tameAmount, p.tameHear); gIn = kf::dbToGain (p.gainDb); ceil = kf::dbToGain (p.ceilingDb); relCoef = 1 - std::exp (-1.0 / (std::max (5.0f, p.releaseMs) * 0.001 * sr)); }
    int latency() const { return L - 1; }
    float reductionDb() const { return redNow.load(); }
    float tameDb() const { return tameNow.load(); }                           // how far the tamed range is being held down right now (0 or below)                       // how far the limiter is holding the level down right now (0 or below)

    void process (float* l, float* r, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            if (inMeter.push (0.5 * ((double) l[i] + r[i]), std::max (std::abs (l[i]), std::abs (r[i]))))      // what arrives, measured the way each Kick Fit measures itself
            { for (int k = 0; k < fl::numBands; ++k) inPower[k].store ((float) inMeter.power[k]); inPeak.store ((float) inMeter.peak); inNow.store ((float) inMeter.stepPeak); }
            toneEq.run (l[i], r[i]); tame.run (l[i], r[i]); transient.run (l[i], r[i]);          // then Loudness, then the limiter
            const float xl = (float) (l[i] * gIn), xr = (float) (r[i] * gIn);
            const double pk = std::max (std::abs ((double) xl), std::abs ((double) xr));
            const double need = pk > ceil ? ceil / pk : 1.0;
            // lowest `need` over the last L samples
            while (dqHead != dqTail && dqV[(size_t) prev (dqTail)] >= need) dqTail = prev (dqTail);
            dqV[(size_t) dqTail] = need; dqI[(size_t) dqTail] = n; dqTail = next (dqTail);
            if (dqI[(size_t) dqHead] <= n - L) dqHead = next (dqHead);
            const double m = dqV[(size_t) dqHead];
            rel = m <= rel ? m : (rel + (m - rel) * relCoef > 0.999999 && m == 1.0 ? 1.0 : rel + (m - rel) * relCoef);
            // average over L
            const double old = box[(size_t) pos]; box[(size_t) pos] = rel;
            if (old != 1.0) --lowered; if (rel != 1.0) ++lowered;
            boxSum += rel - old; if (lowered == 0) boxSum = L;
            const double g = lowered == 0 ? 1.0 : std::min (1.0, boxSum / L);
            // the sound from L-1 samples ago
            dl[(size_t) pos] = xl; dr[(size_t) pos] = xr; pos = pos + 1 == L ? 0 : pos + 1;
            float yl = dl[(size_t) pos], yr = dr[(size_t) pos];
            if (g != 1.0) { yl = (float) (yl * g); yr = (float) (yr * g); }
            const float c = (float) ceil; yl = std::clamp (yl, -c, c); yr = std::clamp (yr, -c, c);       // rounding only: the gain already keeps it under
            // the picture
            const double mono = 0.5 * ((double) yl + yr);            // bandE[b] = energy of everything up to that range's upper edge
            for (int b = 0; b < numBands; ++b) { const double upTo = b < numBands - 1 ? lpB[b].run (lpA[b].run (mono)) : mono; bandE[b] += upTo * upTo; }
            tameHeld = std::min (tameHeld, tame.reductionDb());
            cur.in = std::max (cur.in, (float) pk); cur.out = std::max (cur.out, std::max (std::abs (yl), std::abs (yr))); cur.gain = std::min (cur.gain, (float) g);
            if (++colN >= colStep)
            {
                for (int b = numBands - 1; b >= 0; --b) { cur.band[b] = (float) std::sqrt (std::max (0.0, bandE[b] - (b ? bandE[b - 1] : 0.0)) / colN); }
                for (auto& v : bandE) v = 0;
                const int w = colW.load (std::memory_order_relaxed); cols[(size_t) w] = cur; colW.store ((w + 1) % numColumns, std::memory_order_release);
                redNow.store ((float) (20 * std::log10 (std::max (1e-6f, cur.gain)))); tameNow.store (std::min (tameHeld, tame.reductionDb())); tameHeld = 0; cur = {}; cur.gain = 1; colN = 0;
            }
            loud.push (yl, yr);                                          // what leaves, after the limiter
            if (outMeter.push (0.5 * ((double) yl + yr), std::max (std::abs (yl), std::abs (yr)))) for (int k = 0; k < fl::numBands; ++k) outPower[k].store ((float) outMeter.power[k]);
            l[i] = yl; r[i] = yr; ++n;
        }
    }
    // for the window: the last `count` slices, oldest first
    void latest (Column* out, int count) const
    {
        const int w = colW.load (std::memory_order_acquire);
        for (int i = 0; i < count; ++i) out[i] = cols[(size_t) (((w - count + i) % numColumns + numColumns) % numColumns)];
    }

private:
    int next (int i) const { return i == L ? 0 : i + 1; }
    int prev (int i) const { return i == 0 ? L : i - 1; }
    Params p; double sr = 44100, gIn = 1, ceil = 1, relCoef = 0.001, rel = 1, boxSum = 0, bandE[numBands] {};
    int L = 221, pos = 0, dqHead = 0, dqTail = 0, lowered = 0, colStep = 882, colN = 0; long long n = 0;
    std::vector<float> dl, dr; std::vector<double> box, dqV; std::vector<long long> dqI;
    kf::Biquad lpA[numBands - 1], lpB[numBands - 1];
    fl::BandMeter inMeter, outMeter; ToneEq toneEq; Transient transient; Tame tame; float tameHeld = 0; std::atomic<float> tameNow { 0.0f };
public:
    Loudness loud;
    std::atomic<float> outPower[fl::numBands] {};                          // what leaves, strength per range: the tone bars (so Tame and the rest show in them)
    std::atomic<float> inPower[fl::numBands] {}, inPeak { 0.0f }, inNow { 0.0f };       // the bus as it arrives: strength per range, recent peak, the peak of the last 20 ms
private:
    Column cur, cols[numColumns]; std::atomic<int> colW { 0 }; std::atomic<float> redNow { 0.0f };
};
}
