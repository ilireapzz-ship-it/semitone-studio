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

struct Params { float gainDb = 0, ceilingDb = 0, releaseMs = 120; };

// How much of each part (kick, bass, vocal) is in the bus. Each Kick Fit says how strong its own output is in six ranges of
// pitch; the bus measures itself the same way. The bus should be those parts added together, each times its own gain
// (the mixer fader and anything else between Kick Fit and here). This finds the three gains that fit best over the last
// several seconds: least squares on the strengths, never below zero. `fit` says how much of the bus the parts explain
// (near 1 = the bus is just these parts; low = there is a lot else in it, or the parts cannot be told apart).
struct Mixer
{
    static constexpr int parts = 3;
    void reset() { for (auto& row : A) for (auto& v : row) v = 0; for (auto& v : r) v = 0; c = 0; for (auto& row : sP) for (auto& v : row) v = 0; for (auto& v : sB) v = 0; }
    // P: each part's strengths as its Kick Fit reports them; Bnow: the bus's; dt: seconds since the last call.
    // Both are first averaged over about 1.5 s: the bus hears the parts a few ms late (the host lines plug-ins up, and the
    // numbers are read at slightly different moments), which matters at the instant a kick lands and not over a second.
    void add (const double Pnow[parts][fl::numBands], const bool have[parts], const double Bnow[fl::numBands], double dt)
    {
        const double a = 1 - std::exp (-dt / slow), keep = std::exp (-dt / memory);
        for (int b = 0; b < fl::numBands; ++b) { sB[b] += a * (Bnow[b] - sB[b]); for (int i = 0; i < parts; ++i) sP[i][b] += a * ((have[i] ? Pnow[i][b] : 0.0) - sP[i][b]); }
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
    double slow = 1.5, memory = 4.0;          // seconds: the averaging, and how fast old moments fade
private:
    double A[parts][parts] {}, r[parts] {}, c = 0, sP[parts][fl::numBands] {}, sB[fl::numBands] {};
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
        inMeter.prepare (sr);
        set (p);
    }
    void set (const Params& np) { p = np; gIn = kf::dbToGain (p.gainDb); ceil = kf::dbToGain (p.ceilingDb); relCoef = 1 - std::exp (-1.0 / (std::max (5.0f, p.releaseMs) * 0.001 * sr)); }
    int latency() const { return L - 1; }
    float reductionDb() const { return redNow.load(); }                       // how far the limiter is holding the level down right now (0 or below)

    void process (float* l, float* r, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            if (inMeter.push (0.5 * ((double) l[i] + r[i]), std::max (std::abs (l[i]), std::abs (r[i]))))      // what arrives, measured the way each Kick Fit measures itself
            { for (int k = 0; k < fl::numBands; ++k) inPower[k].store ((float) inMeter.power[k]); inPeak.store ((float) inMeter.peak); }
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
            cur.in = std::max (cur.in, (float) pk); cur.out = std::max (cur.out, std::max (std::abs (yl), std::abs (yr))); cur.gain = std::min (cur.gain, (float) g);
            if (++colN >= colStep)
            {
                for (int b = numBands - 1; b >= 0; --b) { cur.band[b] = (float) std::sqrt (std::max (0.0, bandE[b] - (b ? bandE[b - 1] : 0.0)) / colN); }
                for (auto& v : bandE) v = 0;
                const int w = colW.load (std::memory_order_relaxed); cols[(size_t) w] = cur; colW.store ((w + 1) % numColumns, std::memory_order_release);
                redNow.store ((float) (20 * std::log10 (std::max (1e-6f, cur.gain)))); cur = {}; cur.gain = 1; colN = 0;
            }
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
    fl::BandMeter inMeter;
public:
    std::atomic<float> inPower[fl::numBands] {}, inPeak { 0.0f };       // the bus as it arrives: strength per range, recent peak
private:
    Column cur, cols[numColumns]; std::atomic<int> colW { 0 }; std::atomic<float> redNow { 0.0f };
};
}
