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

int main()
{
    testUntouched(); testCeiling(); testBlocks(); testReduction(); testPicture();
    if (failures) { printf ("fitbus: %d failed\n", failures); return 1; }
    printf ("fitbus: all tests passed\n"); return 0;
}
