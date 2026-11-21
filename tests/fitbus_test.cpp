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
static Buf fakeRest (double sr, double secs)           // a pad: a chord of saw-ish notes that swells, with some air
{
    Buf b ((size_t) (sr * secs), 0.0f); std::mt19937 rng (8); std::uniform_real_distribution<double> u (-1, 1); kf::Biquad hp; hp.highpass (6000, 0.7, sr);
    for (size_t i = 0; i < b.size(); ++i)
    {
        const double t = (double) i / sr, swell = 0.5 + 0.5 * std::sin (2 * kf::pi * 0.13 * t); double v = 0;
        for (double f : { 220.0, 277.2, 329.6, 440.0 }) for (int h = 1; h <= 4; ++h) v += std::sin (2 * kf::pi * f * h * t) / (h * 4.0);
        b[i] = (float) (swell * (0.3 * v + 0.15 * hp.run (u (rng))));
    }
    return b;
}
static void testMixer()
{
    const double sr = 48000, secs = 12; const Buf part[4] = { fakeKick (sr, secs), fakeBass (sr, secs), fakeVoice (sr, secs), fakeRest (sr, secs) };
    struct Case { double g[4]; bool have[4]; double extra; double tolDb; double minFit, maxFit; };
    const Case cases[] = { { { 0.5, 0.3, 1.2 }, { true, true, true }, 0, 1.0, 0.97, 1.0 },          // all three, faders all over the place
                           { { 0.5, 0.4, 0.8, 0.7 }, { true, true, true, true }, 0, 1.5, 0.97, 1.0 },   // with the rest of the song, which has its own Kick Fit
                           { { 0.5, 0.4, 0.8, 0.7 }, { true, true, true, false }, 0, 99, 0.0, 0.97 },   // the rest of the song is there but has no Kick Fit: it must say it can't tell
                           { { 1.0, 0.1, 0.4 }, { true, true, true }, 0, 6.0, 0.97, 1.0 },          // a bass 20 dB down under a kick in the same range: only roughly (the window says so and offers no fix)
                           { { 0.7, 0.0, 0.9 }, { true, false, true }, 0, 1.0, 0.97, 1.0 },         // no Kick Fit on a bass, and no bass
                           { { 0.5, 0.5, 0.5 }, { true, true, true }, 0.6, 99, 0.0, 0.97 } };       // a lot else in the bus (a loud hiss): the levels can't be trusted, and the fit must say so (under 0.97)
    for (size_t late : { (size_t) 0, (size_t) (0.03 * sr), (size_t) (0.007 * sr) }) for (auto& cs : cases)       // the bus may hear the parts a little late (the host lines plug-ins up)
    {
        fl::BandMeter pm[4], bm; for (auto& m : pm) m.prepare (sr);
        bm.prepare (sr); Mixer mix; std::mt19937 rng (11); std::uniform_real_distribution<double> u (-1, 1);
        for (size_t i = 0; i < part[0].size(); ++i)
        {
            double sum = cs.extra * u (rng); for (int k = 0; k < 4; ++k) sum += i >= late ? cs.g[k] * part[k][i - late] : 0.0;
            const bool step = bm.push (sum, std::abs (sum)); for (int k = 0; k < 4; ++k) pm[k].push (part[k][i], std::abs (part[k][i]));
            if (step) { double P[4][fl::numBands]; for (int k = 0; k < 4; ++k) for (int b = 0; b < fl::numBands; ++b) P[k][b] = pm[k].power[b]; mix.add (P, cs.have, bm.power, 0.02); }
        }
        double g2[4], fit = 0; CHECK (mix.solve (cs.have, g2, fit), "nothing solved");
        for (int k = 0; k < 4; ++k)
        {
            if (! cs.have[k]) { CHECK (g2[k] == 0, "a part with no Kick Fit got a level"); continue; }
            const double got = 10 * std::log10 (std::max (g2[k], 1e-12)), want = 20 * std::log10 (cs.g[k]);
            CHECK (std::abs (got - want) < cs.tolDb, "part %d: fader read as %.2f dB, was %.2f dB (extra %.1f, late %d)", k, got, want, cs.extra, (int) late);
        }
        CHECK (fit >= cs.minFit && fit <= cs.maxFit, "fit %.2f not in %.2f..%.2f (extra %.1f, late %d)", fit, cs.minFit, cs.maxFit, cs.extra, (int) late);
    }
}

// level of a stretch, dB
static double levelDb (const Buf& b, double sr, double from, double to)
{
    double e = 0; size_t n = 0; for (size_t i = (size_t) (from * sr); i < (size_t) (to * sr) && i < b.size(); ++i) { e += (double) b[i] * b[i]; ++n; }
    return 10 * std::log10 (e / std::max<size_t> (n, 1) + 1e-20);
}
static double peakDb (const Buf& b, double sr, double from, double to)
{
    float m = 0; for (size_t i = (size_t) (from * sr); i < (size_t) (to * sr) && i < b.size(); ++i) m = std::max (m, std::abs (b[i]));
    return 20 * std::log10 (m + 1e-12);
}
// a hit every half second: a sharp front, then a body that dies away over a few tenths of a second
static Buf hits (double sr, double secs)
{
    Buf b ((size_t) (sr * secs), 0.0f);
    for (size_t i = 0; i < b.size(); ++i) { const double t = std::fmod ((double) i / sr, 0.5); b[i] = (float) (0.25 * std::sin (2 * kf::pi * 180 * t) * (std::exp (-t / 0.15) + 1.2 * std::exp (-t / 0.004))); }
    return b;
}
static void testTransient()
{
    const double sr = 48000;
    auto shaped = [&] (const Buf& in, float attack, float sustain) { Buf l = in, r = in; Engine e; Params p; p.attack = attack; p.sustain = sustain; e.set (p); e.prepare (sr); run (e, l, r, 512); return l; };
    const Buf in = hits (sr, 4); const double d = (double) (Engine().latency()) / sr; Engine probe; probe.prepare (sr); const double lat = probe.latency() / sr; (void) d;
    // measured on the fourth hit (3.0 s in): its front is the first 6 ms, its body 60 to 250 ms after
    auto front = [&] (const Buf& b) { return peakDb (b, sr, 3.0 + lat, 3.006 + lat); };
    auto body = [&] (const Buf& b) { return levelDb (b, sr, 3.06 + lat, 3.25 + lat); };
    const Buf flat = shaped (in, 0, 0), up = shaped (in, 1, 0), down = shaped (in, -1, 0), longer = shaped (in, 0, 1), shorter = shaped (in, 0, -1);
    CHECK (front (up) - front (flat) > 2.0, "Attack up should make the front of a hit taller: %.2f dB", front (up) - front (flat));
    CHECK (front (down) - front (flat) < -2.0, "Attack down should soften the front: %.2f dB", front (down) - front (flat));
    CHECK (std::abs (body (up) - body (flat)) < 1.0 && std::abs (body (down) - body (flat)) < 1.0, "Attack should leave the body alone: %.2f / %.2f dB", body (up) - body (flat), body (down) - body (flat));
    CHECK (body (longer) - body (flat) > 1.5, "Sustain up should lift the body: %.2f dB", body (longer) - body (flat));
    CHECK (body (shorter) - body (flat) < -1.5, "Sustain down should thin the body: %.2f dB", body (shorter) - body (flat));
    CHECK (std::abs (front (longer) - front (flat)) < 1.0 && std::abs (front (shorter) - front (flat)) < 1.0, "Sustain should leave the front alone: %.2f / %.2f dB", front (longer) - front (flat), front (shorter) - front (flat));
    // a steady sound has no hits: nothing should change, whatever the knobs say
    const Buf tone = sine (sr, 300, 0.3, 3);
    for (float a : { -1.0f, 1.0f }) for (float su : { -1.0f, 1.0f })
    { const Buf o = shaped (tone, a, su); CHECK (std::abs (levelDb (o, sr, 2, 3) - levelDb (tone, sr, 2, 3)) < 0.3, "a steady sound moved %.2f dB (attack %.0f sustain %.0f)", levelDb (o, sr, 2, 3) - levelDb (tone, sr, 2, 3), a, su); }
    Buf z ((size_t) sr, 0.0f); const Buf zo = shaped (z, 1, 1); bool ok = true; for (float v : zo) ok = ok && v == 0.0f; CHECK (ok, "silence came out as something");
}

// two tones at once, one in the range being watched and one outside it
static void testTame()
{
    const double sr = 48000; const size_t n = (size_t) (sr * 3);
    auto mixOf = [&] (double ampIn, double hzIn, double hzOut) { Buf b (n); for (size_t i = 0; i < n; ++i) { const double t = (double) i / sr; b[i] = (float) (ampIn * std::sin (2 * kf::pi * hzIn * t) + 0.2 * std::sin (2 * kf::pi * hzOut * t)); } return b; };
    auto toneDb = [&] (const Buf& b, double hz) { double re = 0, im = 0; const size_t a = (size_t) (sr * 2); for (size_t i = a; i < n; ++i) { const double t = (double) i / sr; re += b[i] * std::cos (2 * kf::pi * hz * t); im += b[i] * std::sin (2 * kf::pi * hz * t); } return 20 * std::log10 (2 * std::sqrt (re * re + im * im) / (double) (n - a) + 1e-12); };
    struct Case { int range; double hzIn, hzOut; }; const Case cases[] = { { 1, 40, 1000 }, { 2, 100, 2000 }, { 4, 800, 80 }, { 5, 3000, 200 }, { 6, 9000, 300 } };
    for (auto& c : cases)
    {
        const Buf in = mixOf (0.5, c.hzIn, c.hzOut); Buf l = in, r = in; Engine e; Params p; p.tameRange = c.range; p.tameThrDb = -30; p.tameAmount = 1; e.set (p); e.prepare (sr); run (e, l, r, 512);
        const double inside = toneDb (l, c.hzIn) - toneDb (in, c.hzIn), outside = toneDb (l, c.hzOut) - toneDb (in, c.hzOut);
        CHECK (inside < -6.0, "range %d: the loud tone inside it (%.0f Hz) should come down, moved %.2f dB", c.range, c.hzIn, inside);
        CHECK (std::abs (outside) < 1.5, "range %d: the tone outside it (%.0f Hz) moved %.2f dB", c.range, c.hzOut, outside);
        CHECK (e.tameDb() < -3.0f, "range %d: it should report holding down, said %.2f dB", c.range, e.tameDb());
        // the tone bars read what leaves: the tamed range's share must be lower than without Tame
        Buf nl = in, nr = in; Engine en; en.prepare (sr); run (en, nl, nr, 512); const int b = c.range - 1;
        CHECK (e.outPower[b].load() < 0.5f * en.outPower[b].load(), "range %d: the tone reading did not follow Tame (%.5f against %.5f)", c.range, e.outPower[b].load(), en.outPower[b].load());
        // quiet enough to stay under the threshold: the same samples come out
        const Buf q = mixOf (0.01, c.hzIn, c.hzOut); Buf ql = q, qr = q; Engine e2; e2.set (p); e2.prepare (sr); run (e2, ql, qr, 512);
        bool same = true; const size_t d = (size_t) e2.latency(); for (size_t i = d; i < n; ++i) same = same && ql[i] == q[i - d];
        CHECK (same, "range %d: a sound under the threshold was changed", c.range);
        // Amount at 0 does nothing even when loud
        Buf al = in, ar = in; Engine e3; Params p0 = p; p0.tameAmount = 0; e3.set (p0); e3.prepare (sr); run (e3, al, ar, 512);
        same = true; for (size_t i = d; i < n; ++i) same = same && al[i] == in[i - d];
        CHECK (same, "range %d: Amount at 0 changed the sound", c.range);
    }
    // it lets go: loud in the range for a second, then quiet
    Buf b (n); for (size_t i = 0; i < n; ++i) { const double t = (double) i / sr; b[i] = (float) ((t < 1 ? 0.5 : 0.005) * std::sin (2 * kf::pi * 3000 * t)); }
    Buf l = b, r = b; Engine e; Params p; p.tameRange = 5; p.tameThrDb = -30; p.tameAmount = 1; e.set (p); e.prepare (sr); run (e, l, r, 512);
    CHECK (std::abs (levelDb (l, sr, 2.5, 3) - levelDb (b, sr, 2.5, 3)) < 0.2, "did not let go after the loud part: %.2f dB", levelDb (l, sr, 2.5, 3) - levelDb (b, sr, 2.5, 3));
    float jump = 0; for (size_t i = (size_t) (sr * 0.2); i < (size_t) (sr * 0.9); ++i) jump = std::max (jump, std::abs (l[i] - l[i - 1])); CHECK (jump < 0.25f, "the tamed sound has a click in it (step %.3f)", jump);
}

// the tone change: each range's gain moves a tone inside it and leaves a far-away tone nearly alone; A/B keeps the loudness
static void testToneEq()
{
    const double sr = 48000; const size_t n = (size_t) (sr * 3); const double inside[numBands] = { 35, 100, 250, 800, 3000, 10000 }, away[numBands] = { 3000, 3000, 6000, 60, 60, 100 };
    auto two = [&] (double a, double b) { Buf v (n); for (size_t i = 0; i < n; ++i) { const double t = (double) i / sr; v[i] = (float) (0.2 * std::sin (2 * kf::pi * a * t) + 0.2 * std::sin (2 * kf::pi * b * t)); } return v; };
    auto toneDb = [&] (const Buf& b, double hz) { double re = 0, im = 0; const size_t a = (size_t) (sr * 2); for (size_t i = a; i < n; ++i) { const double t = (double) i / sr; re += b[i] * std::cos (2 * kf::pi * hz * t); im += b[i] * std::sin (2 * kf::pi * hz * t); } return 20 * std::log10 (2 * std::sqrt (re * re + im * im) / (double) (n - a) + 1e-12); };
    for (int b = 0; b < numBands; ++b) for (float dB : { -6.0f, 4.0f })
    {
        const Buf in = two (inside[b], away[b]); Buf l = in, r = in; Engine e; Params p; p.tone[b] = dB; e.set (p); e.prepare (sr); run (e, l, r, 512);
        const double moved = toneDb (l, inside[b]) - toneDb (in, inside[b]), other = toneDb (l, away[b]) - toneDb (in, away[b]);
        CHECK (moved * dB > 0 && std::abs (moved) > 0.55 * std::abs (dB) && std::abs (moved) < 1.15 * std::abs (dB), "range %d at %.0f dB: its tone moved %.2f dB", b, dB, moved);
        CHECK (std::abs (other) < 1.0, "range %d at %.0f dB: a far tone (%.0f Hz) moved %.2f dB", b, dB, away[b], other);
        // no click as it comes in: no step bigger than the loudest the finished sound steps anyway (a 10 kHz tone steps a lot by itself)
        float jump = 0, usual = 0; for (size_t i = 1; i < (size_t) (sr * 0.5); ++i) jump = std::max (jump, std::abs (l[i] - l[i - 1]));
        for (size_t i = (size_t) (sr * 2); i < n; ++i) usual = std::max ({ usual, std::abs (l[i] - l[i - 1]), std::abs (in[i] - in[i - 1]) });      // before or after the change, whichever steps more
        CHECK (jump < 1.3f * usual + 0.01f, "range %d: a click as the tone change came in (step %.3f, usual %.3f)", b, jump, usual);
    }
    // A/B: "without" is flat (both tones move by the same amount) and about as loud as "with"
    const Buf in = two (100, 3000); Params p; p.tone[1] = -6; p.tone[4] = 3;
    Buf wl = in, wr = in; Engine ew; ew.set (p); ew.prepare (sr); run (ew, wl, wr, 512);
    p.toneOn = false; Buf ol = in, orr = in; Engine eo; eo.set (p); eo.prepare (sr);
    for (size_t i = 0; i < n; i += 512) { eo.set (p); eo.process (ol.data() + i, orr.data() + i, (int) std::min<size_t> (512, n - i)); }      // as a host does: settings before every block
    const double a = toneDb (ol, 100) - toneDb (in, 100), b = toneDb (ol, 3000) - toneDb (in, 3000);
    CHECK (std::abs (a - b) < 0.2, "A/B off should be flat: the two tones moved %.2f and %.2f dB", a, b);
    CHECK (std::abs (levelDb (ol, sr, 2, 3) - levelDb (wl, sr, 2, 3)) < 1.0, "A/B: without is %.2f dB louder than with", levelDb (ol, sr, 2, 3) - levelDb (wl, sr, 2, 3));
    CHECK (std::abs (a) > 0.5, "A/B off did not match the loudness at all (moved %.2f dB)", a);
}

// loudness: a 1 kHz tone on both sides at -20 dB reads -20 (the standard's own check), and quiet gaps don't drag it down
static void testLoudness()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        Buf l = sine (sr, 997, 0.1, 6), r = l; Engine e; e.prepare (sr); run (e, l, r, 512);
        double v = 0; CHECK (e.loud.read (v), "no loudness after 6 s of tone");
        CHECK (std::abs (v + 20.0) < 0.2, "a -20 dB tone on both sides read %.2f at %.0f Hz", v, sr);
        Buf q ((size_t) (sr * 6), 0.0f); Buf l2 = sine (sr, 997, 0.1, 6); l2.insert (l2.end(), q.begin(), q.end()); Buf r2 = l2;
        Engine e2; e2.prepare (sr); run (e2, l2, r2, 512);
        CHECK (e2.loud.read (v) && std::abs (v + 20.0) < 0.3, "silence after the tone dragged it to %.2f", v);
        Buf a = sine (sr, 997, 0.1, 6), b2 = sine (sr, 997, 0.2, 6); a.insert (a.end(), b2.begin(), b2.end()); Buf ar = a;
        Engine e3; e3.prepare (sr); run (e3, a, ar, 512);
        CHECK (e3.loud.read (v, 4) && std::abs (v + 14.0) < 0.3, "the last 4 s (a -14 dB tone) read %.2f", v);
    }
    Buf z (48000 * 3, 0.0f), z2 = z; Engine e; e.prepare (48000); run (e, z, z2, 512); double v = 0; CHECK (! e.loud.read (v), "silence gave a loudness");
}

int main()
{
    testUntouched(); testCeiling(); testBlocks(); testReduction(); testPicture(); testMixer(); testTransient(); testTame(); testToneEq(); testLoudness();
    if (failures) { printf ("fitbus: %d failed\n", failures); return 1; }
    printf ("fitbus: all tests passed\n"); return 0;
}
