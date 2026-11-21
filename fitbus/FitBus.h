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
#include <atomic>
#include <vector>

namespace fb
{
constexpr int numBands = 6;
// upper edge of each range, Hz (the last has none): sub, bass, low mids, mids, presence, air
constexpr double bandEdges[numBands - 1] = { 60, 150, 400, 1500, 5000 };

struct Column { float in = 0, out = 0, gain = 1, band[numBands] {}; };      // one slice of the picture (20 ms): peaks before and after, lowest gain, level per range

struct Params { float gainDb = 0, ceilingDb = 0, releaseMs = 120; };

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
        set (p);
    }
    void set (const Params& np) { p = np; gIn = kf::dbToGain (p.gainDb); ceil = kf::dbToGain (p.ceilingDb); relCoef = 1 - std::exp (-1.0 / (std::max (5.0f, p.releaseMs) * 0.001 * sr)); }
    int latency() const { return L - 1; }
    float reductionDb() const { return redNow.load(); }                       // how far the limiter is holding the level down right now (0 or below)

    void process (float* l, float* r, int count)
    {
        for (int i = 0; i < count; ++i)
        {
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
    Column cur, cols[numColumns]; std::atomic<int> colW { 0 }; std::atomic<float> redNow { 0.0f };
};
}
