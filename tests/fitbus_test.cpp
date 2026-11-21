// Unit test for Fit Bus's processing (no JUCE needed): clang++ -std=c++17 -O2 -Ifitbus tests/fitbus_test.cpp -o t && ./t
// Every sound here is made by the test (sines, noise): none of it is anyone's sample.
#include "FitBus.h"
#include <cstdio>
#include <random>

using namespace fb;
static int failures = 0;
#define CHECK(c, ...) do { if (! (c)) { ++failures; printf ("FAIL %s:%d ", __FILE__, __LINE__); printf (__VA_ARGS__); printf ("\n"); } } while (0)
using Buf = std::vector<float>;

static Buf sine (double sr, double hz, double amp, double secs)
{
    Buf b ((size_t) (sr * secs)); for (size_t i = 0; i < b.size(); ++i) b[i] = (float) (amp * std::sin (2 * kf::pi * hz * (double) i / sr)); return b;
}
static void run (Engine& e, Buf& l, Buf& r, int block)
{
    for (size_t i = 0; i < l.size(); i += (size_t) block) e.process (l.data() + i, r.data() + i, (int) std::min<size_t> ((size_t) block, l.size() - i));
}

// under the ceiling nothing changes: the same samples come out, only later
static void testUntouched()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        std::mt19937 rng (3); std::uniform_real_distribution<float> u (-0.9f, 0.9f);
        Buf l ((size_t) sr), r ((size_t) sr); for (auto& v : l) v = u (rng); for (auto& v : r) v = u (rng);
        const Buf l0 = l, r0 = r; Engine e; e.prepare (sr); run (e, l, r, 256);
        const int d = e.latency(); bool same = true;
        for (size_t i = (size_t) d; i < l.size(); ++i) same = same && l[i] == l0[i - (size_t) d] && r[i] == r0[i - (size_t) d];
        CHECK (same, "quiet sound was changed at %.0f Hz", sr);
    }
}

// nothing ever passes the ceiling, whatever comes in, at any rate and block size
static void testCeiling()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 }) for (int block : { 32, 441, 2048 }) for (float ceilDb : { 0.0f, -3.0f })
    {
        std::mt19937 rng (9); std::uniform_real_distribution<float> u (-1, 1);
        Buf l ((size_t) (sr * 2)), r (l.size());
        for (size_t i = 0; i < l.size(); ++i)
        {
            const double t = (double) i / sr, burst = std::fmod (t, 0.5) < 0.05 ? 6.0 : 0.4;       // sudden hits 16 dB over
            l[i] = (float) (burst * std::sin (2 * kf::pi * 50 * t) + 0.5 * u (rng)); r[i] = (float) (burst * std::sin (2 * kf::pi * 73 * t));
        }
        if (block == 32) { l[1000] = 30; r[5000] = -30; }                                            // single wild samples
        Engine e; Params p; p.ceilingDb = ceilDb; p.gainDb = 6; e.set (p); e.prepare (sr); run (e, l, r, block);
        float top = 0; bool finite = true; for (size_t i = 0; i < l.size(); ++i) { top = std::max ({ top, std::abs (l[i]), std::abs (r[i]) }); finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]); }
        CHECK (finite, "not a number came out");
        CHECK (top <= (float) kf::dbToGain (ceilDb) * 1.00001f, "passed the ceiling: %.5f (ceiling %.1f dB, %.0f Hz, block %d)", top, ceilDb, sr, block);
    }
}

// the same sound comes out whatever size of pieces the host hands over
static void testBlocks()
{
    const double sr = 48000; Buf a = sine (sr, 80, 2.5, 1), b = a, a2 = a, b2 = a;
    Engine e1, e2; e1.prepare (sr); e2.prepare (sr); run (e1, a, a2, 64); run (e2, b, b2, 1000);
    bool same = true; for (size_t i = 0; i < a.size(); ++i) same = same && a[i] == b[i];
    CHECK (same, "block size changes the sound");
}

// the limiter turns down by about what is needed, smoothly, and lets go afterwards
static void testReduction()
{
    const double sr = 48000; Buf l = sine (sr, 100, 2.0, 1.0); Buf quiet = sine (sr, 100, 0.25, 1.5); l.insert (l.end(), quiet.begin(), quiet.end()); Buf r = l;
    Engine e; e.prepare (sr); run (e, l, r, 512);
    float loud = 0; for (size_t i = (size_t) (sr * 0.5); i < (size_t) (sr * 0.9); ++i) loud = std::max (loud, std::abs (l[i]));
    CHECK (loud > 0.9f && loud <= 1.00001f, "a sound 6 dB over should sit at the ceiling, got %.3f", loud);
    float jump = 0; for (size_t i = (size_t) (sr * 0.5); i < (size_t) (sr * 0.9); ++i) jump = std::max (jump, std::abs (l[i] - l[i - 1]));
    CHECK (jump < 0.02f, "the limited sound has a click in it (step %.4f)", jump);
    float later = 0; for (size_t i = (size_t) (sr * 2.0); i < l.size(); ++i) later = std::max (later, std::abs (l[i]));
    CHECK (std::abs (later - 0.25f) < 0.003f, "did not let go after the loud part: %.4f", later);
    CHECK (e.reductionDb() > -0.1f, "still reports holding down: %.2f dB", e.reductionDb());
}

// the picture: low sounds land in the red end, high sounds in the blue end, and the heights are the real peaks
static void testPicture()
{
    const double sr = 48000; const double tones[numBands] = { 35, 100, 250, 800, 3000, 9000 };
    for (int b = 0; b < numBands; ++b)
    {
        Buf l = sine (sr, tones[b], 0.5, 1), r = l; Engine e; e.prepare (sr); run (e, l, r, 512);
        Column c[10]; e.latest (c, 10); int best = 0; for (int k = 1; k < numBands; ++k) if (c[9].band[k] > c[9].band[best]) best = k;
        CHECK (best == b, "a %.0f Hz tone was filed under range %d, not %d", tones[b], best, b);
        CHECK (std::abs (c[9].out - 0.5f) < 0.01f && std::abs (c[9].in - 0.5f) < 0.01f && c[9].gain == 1.0f, "picture height wrong: in %.3f out %.3f", c[9].in, c[9].out);
    }
    Buf z ((size_t) sr, 0.0f), z2 = z; Engine e; e.prepare (sr); run (e, z, z2, 512); Column c[4]; e.latest (c, 4);
    CHECK (c[3].out == 0 && std::isfinite (c[3].band[0]), "silence draws something");
}

// three made-up parts through made-up faders: can the bus tell how much of each it holds?
static Buf fakeKick (double sr, double secs)
{
    Buf b ((size_t) (sr * secs), 0.0f);
    for (size_t i = 0; i < b.size(); ++i) { const double t = std::fmod ((double) i / sr, 0.5), f = 50 + 120 * std::exp (-t / 0.02); b[i] = (float) (0.9 * std::sin (2 * kf::pi * f * t) * std::exp (-t / 0.12)); }
    return b;
}
static Buf fakeBass (double sr, double secs)
{
    Buf b ((size_t) (sr * secs), 0.0f);
    for (size_t i = 0; i < b.size(); ++i) { const double t = (double) i / sr, f = std::fmod (t, 2.0) < 1.0 ? 82.4 : 61.7; double v = 0; for (int h = 1; h <= 6; ++h) v += std::sin (2 * kf::pi * f * h * t) / (h * h); b[i] = (float) (0.5 * v); }
    return b;
}
static Buf fakeVoice (double sr, double secs)
{
    Buf b ((size_t) (sr * secs), 0.0f); std::mt19937 rng (4); std::uniform_real_distribution<double> u (-1, 1); kf::Biquad bp, bp2; bp.bandpass (900, 0.6, sr); bp2.bandpass (2500, 0.8, sr);
    for (size_t i = 0; i < b.size(); ++i) { const double t = (double) i / sr, n = u (rng), syll = std::fmod (t, 0.37) < 0.25 && std::fmod (t, 3.0) < 2.2 ? 1.0 : 0.0; b[i] = (float) (syll * (0.8 * bp.run (n) + 0.5 * bp2.run (n))); }
    return b;
}
static void testMixer()
{
    const double sr = 48000, secs = 12; const Buf part[3] = { fakeKick (sr, secs), fakeBass (sr, secs), fakeVoice (sr, secs) };
    struct Case { double g[3]; bool have[3]; double extra; double tolDb; double minFit, maxFit; };
    const Case cases[] = { { { 0.5, 0.3, 1.2 }, { true, true, true }, 0, 1.0, 0.9, 1.0 },          // all three, faders all over the place
                           { { 1.0, 0.1, 0.4 }, { true, true, true }, 0, 6.0, 0.9, 1.0 },          // a bass 20 dB down under a kick in the same range: only roughly (the window says so and offers no fix)
                           { { 0.7, 0.0, 0.9 }, { true, false, true }, 0, 1.0, 0.9, 1.0 },         // no Kick Fit on a bass, and no bass
                           { { 0.5, 0.5, 0.5 }, { true, true, true }, 0.6, 99, 0.0, 0.9 } };       // a lot else in the bus (a loud hiss): the levels can't be trusted, and the fit must say so (under 0.9)
    for (size_t late : { (size_t) 0, (size_t) (0.03 * sr), (size_t) (0.007 * sr) }) for (auto& cs : cases)       // the bus may hear the parts a little late (the host lines plug-ins up)
    {
        fl::BandMeter pm[3], bm; for (auto& m : pm) m.prepare (sr);
        bm.prepare (sr); Mixer mix; std::mt19937 rng (11); std::uniform_real_distribution<double> u (-1, 1);
        for (size_t i = 0; i < part[0].size(); ++i)
        {
            double sum = cs.extra * u (rng); for (int k = 0; k < 3; ++k) sum += i >= late ? cs.g[k] * part[k][i - late] : 0.0;
            const bool step = bm.push (sum, std::abs (sum)); for (int k = 0; k < 3; ++k) pm[k].push (part[k][i], std::abs (part[k][i]));
            if (step) { double P[3][fl::numBands]; for (int k = 0; k < 3; ++k) for (int b = 0; b < fl::numBands; ++b) P[k][b] = pm[k].power[b]; mix.add (P, cs.have, bm.power, 0.02); }
        }
        double g2[3], fit = 0; CHECK (mix.solve (cs.have, g2, fit), "nothing solved");
        for (int k = 0; k < 3; ++k)
        {
            if (! cs.have[k]) { CHECK (g2[k] == 0, "a part with no Kick Fit got a level"); continue; }
            const double got = 10 * std::log10 (std::max (g2[k], 1e-12)), want = 20 * std::log10 (cs.g[k]);
            CHECK (std::abs (got - want) < cs.tolDb, "part %d: fader read as %.2f dB, was %.2f dB (extra %.1f, late %d)", k, got, want, cs.extra, (int) late);
        }
        CHECK (fit >= cs.minFit && fit <= cs.maxFit, "fit %.2f not in %.2f..%.2f (extra %.1f, late %d)", fit, cs.minFit, cs.maxFit, cs.extra, (int) late);
    }
}

int main()
{
    testUntouched(); testCeiling(); testBlocks(); testReduction(); testPicture(); testMixer();
    if (failures) { printf ("fitbus: %d failed\n", failures); return 1; }
    printf ("fitbus: all tests passed\n"); return 0;
}
