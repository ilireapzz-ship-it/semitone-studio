// Unit test for Kick Fit's processing (no JUCE needed): clang++ -std=c++17 -O2 -Ikickfit tests/kickfit_test.cpp -o t && ./t
// Everything here is made up by the test (sine kicks, a sine bass, noise clicks): none of it is anyone's sample.
#include "KickFit.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <random>

using namespace kf;
static int failures = 0;
#define CHECK(c, ...) do { if (! (c)) { ++failures; printf ("FAIL %s:%d ", __FILE__, __LINE__); printf (__VA_ARGS__); printf ("\n"); } } while (0)

using Buf = std::vector<float>;
struct Kick { double f = 55, fStart = 55, glide = 0.03, decay = 0.2, click = 0, amp = 0.7; };

// one hit added into `out` at sample `at`: a sine that drops from fStart to f, dies away, with an optional noise click
static void addKick (Buf& out, double sr, int at, const Kick& k)
{
    std::mt19937 rng (7); std::uniform_real_distribution<float> u (-1, 1);
    double ph = 0, lpn = 0;
    for (int i = 0; at + i < (int) out.size() && i < (int) (sr * 3); ++i)
    {
        const double t = i / sr, f = k.f + (k.fStart - k.f) * std::exp (-t / k.glide);
        ph += 2 * pi * f / sr;
        const double att = std::min (1.0, t / 0.0008);
        double v = std::sin (ph) * std::exp (-t / k.decay) * att;
        if (k.click > 0) { const double n = u (rng); lpn += 0.3 * (n - lpn); v += k.click * (n - lpn) * std::exp (-t / 0.003); }
        out[(size_t) (at + i)] += (float) (k.amp * v);
    }
}
static Buf kicks (double sr, double seconds, double every, const Kick& k, double first = 0.1)
{
    Buf b ((size_t) (sr * seconds), 0.0f);
    for (double t = first; t < seconds - 0.05; t += every) addKick (b, sr, (int) (t * sr), k);
    return b;
}
static Buf tone (double sr, double seconds, std::initializer_list<std::pair<double, double>> parts)
{
    Buf b ((size_t) (sr * seconds), 0.0f);
    for (auto& p : parts) for (size_t i = 0; i < b.size(); ++i) b[i] += (float) (p.second * std::sin (2 * pi * p.first * (double) i / sr));
    return b;
}
// level of one frequency in x[a..b) (Goertzel-style projection)
static double levelAt (const Buf& x, double sr, double f, int a, int b)
{
    double re = 0, im = 0; b = std::min (b, (int) x.size());
    for (int i = a; i < b; ++i) { re += x[(size_t) i] * std::cos (2 * pi * f * i / sr); im += x[(size_t) i] * std::sin (2 * pi * f * i / sr); }
    return 2 * std::sqrt (re * re + im * im) / std::max (1, b - a);
}
static double strongest (const Buf& x, double sr, double lo, double hi, int a, int b)       // Hz, 0.02 Hz steps near the top
{
    double best = lo, bl = -1;
    for (double f = lo; f <= hi; f += 0.25) { const double l = levelAt (x, sr, f, a, b); if (l > bl) { bl = l; best = f; } }
    const double c = best; bl = -1;
    for (double f = c - 0.3; f <= c + 0.3; f += 0.01) { const double l = levelAt (x, sr, f, a, b); if (l > bl) { bl = l; best = f; } }
    return best;
}
static double cents (double f, double ref) { return 1200 * std::log2 (f / ref); }
static double rms (const Buf& x, int a, int b) { double s = 0; b = std::min (b, (int) x.size()); for (int i = a; i < b; ++i) s += (double) x[(size_t) i] * x[(size_t) i]; return std::sqrt (s / std::max (1, b - a)); }
static double peak (const Buf& x, int a = 0, int b = 1 << 30) { double p = 0; b = std::min (b, (int) x.size()); for (int i = a; i < b; ++i) p = std::max (p, (double) std::abs (x[(size_t) i])); return p; }
static double dB (double v) { return 20 * std::log10 (std::max (v, 1e-12)); }
static Buf filtered (const Buf& x, double sr, double hpHz, double lpHz)
{
    Biquad a, b, c, d; Buf y (x.size());
    if (hpHz > 0) { a.highpass (hpHz, .54, sr); b.highpass (hpHz, 1.31, sr); }
    if (lpHz > 0) { c.lowpass (lpHz, .54, sr); d.lowpass (lpHz, 1.31, sr); }
    for (size_t i = 0; i < x.size(); ++i) y[i] = (float) d.run (c.run (b.run (a.run (x[i]))));
    return y;
}

struct Out { Buf l, r; };
static Out run (Engine& e, const Buf& in, const Buf* side, int block)
{
    Out o { in, in };
    for (int i = 0; i < (int) in.size(); i += block)
    {
        const int n = std::min (block, (int) in.size() - i);
        e.process (o.l.data() + i, o.r.data() + i, side ? side->data() + i : nullptr, side ? side->data() + i : nullptr, n);
    }
    return o;
}
static Out render (double sr, const Params& p, const Buf& in, const Buf* side = nullptr, int block = 256, Engine* keep = nullptr)
{
    Engine local; Engine& e = keep ? *keep : local;
    e.setParams (p); e.prepare (sr);
    return run (e, in, side, block);
}

static const double rates[] = { 44100, 48000, 96000 };
static const int blocks[] = { 32, 64, 256, 500, 2048 };

static void testDoNoHarm()
{
    for (double sr : rates)
    {
        const Buf in = kicks (sr, 2.0, 0.5, { 55, 180, 0.03, 0.2, 0.3, 0.9 }), bass = tone (sr, 2.0, { { 55, 0.2 }, { 110, 0.1 } });
        const Out o = render (sr, Params(), in, &bass, 256);
        CHECK (std::memcmp (o.l.data(), in.data(), in.size() * sizeof (float)) == 0, "all controls at rest must pass the kick through untouched (%g Hz)", sr);
        Params b; b.role = 1; b.room = 1;
        const Buf quiet (in.size(), 0.0f);
        const Out q = render (sr, b, bass, &quiet, 256), nul = render (sr, b, bass, nullptr, 256);
        CHECK (std::memcmp (q.l.data(), bass.data(), bass.size() * sizeof (float)) == 0, "a silent sidechain must leave the bass untouched (%g Hz)", sr);
        CHECK (std::memcmp (nul.l.data(), bass.data(), bass.size() * sizeof (float)) == 0, "no sidechain must leave the bass untouched (%g Hz)", sr);
    }
}

static void testSafe()
{
    Params hot; hot.thump = 1; hot.punch = 1; hot.click = 1; hot.tail = 1; hot.gainDb = 12; hot.tune = -3;
    Params up = hot; up.tune = 2; up.tail = -1;
    Params mild; mild.thump = 0.5f; mild.punch = 0.5f; mild.click = 0.5f; mild.tail = 0.3f; mild.tune = -3; mild.gainDb = -3;
    for (double sr : rates)
    {
        Buf in = kicks (sr, 3.0, 0.4, { 50, 220, 0.03, 0.25, 0.5, 1.0 });
        for (size_t i = (size_t) (sr * 1.6); i < in.size(); ++i) in[i] = 0;                 // then silence, to catch denormals
        const Buf bass = tone (sr, 3.0, { { 50, 0.3 } });
        Out ref;
        for (int bi = 0; bi < 5; ++bi)
            for (const Params* p : { &hot, &up, &mild })
            {
                const Out o = render (sr, *p, in, &bass, blocks[bi]);
                bool finite = true, denormal = false;
                for (float v : o.l) { finite = finite && std::isfinite (v); denormal = denormal || (v != 0 && std::abs (v) < 1e-30f); }
                CHECK (finite, "output must stay finite (%g Hz, block %d)", sr, blocks[bi]);
                CHECK (! denormal, "no denormal numbers in the output (%g Hz, block %d)", sr, blocks[bi]);
                CHECK (peak (o.l) <= 1.0, "output must never pass full scale: %.3f (%g Hz, block %d)", peak (o.l), sr, blocks[bi]);
                if (p == &mild)
                {
                    if (bi == 0) ref = o;
                    else
                    {       // the same sound whatever the block size (after the first hit, while Thump eases in): no clicks at block edges
                        double worst = 0; for (size_t i = (size_t) (sr * 0.9); i < o.l.size(); ++i) worst = std::max (worst, (double) std::abs (o.l[i] - ref.l[i]));
                        CHECK (worst < 0.02, "block size %d changes the sound by %.4f (%g Hz)", blocks[bi], worst, sr);
                    }
                }
            }
        // the sharpest step in the output is no sharper than a click could be: no jumps where a block ends
        Params soft; soft.thump = 1; soft.tail = 0.5f; soft.gainDb = 3;
        const Buf smooth = kicks (sr, 2.0, 0.5, { 50, 50, 0.03, 0.3, 0, 0.5 });
        const Out a = render (sr, soft, smooth, nullptr, 32);
        double step = 0; for (size_t i = (size_t) (sr * 0.7); i < a.l.size(); ++i) step = std::max (step, (double) std::abs (a.l[i] - a.l[i - 1]));
        CHECK (step < 0.02 * 48000 / sr * 2.5, "a jump of %.4f between two samples in a smooth kick (%g Hz)", step, sr);
    }
}

static void testPitch()
{
    for (double sr : rates)
    {
        for (double f : { 36.71, 41.2, 55.0, 65.41, 98.0, 130.81 })
        {
            Engine e; render (sr, Params(), kicks (sr, 1.2, 5, { f, f, 0.03, 1.5, 0.2, 0.7 }), nullptr, 256, &e);
            CHECK (e.note() > 0 && std::abs (cents (e.note(), f)) < 5, "steady %.2f Hz read as %.2f Hz (%.1f cents) at %g", f, e.note(), e.note() > 0 ? cents (e.note(), f) : 0.0, sr);
        }
        // a kick whose pitch drops at the start: the note is the pitch of its loud body, which sits a little above where the
        // pitch finally comes to rest (about 22 cents on these). Reading the body is what makes a real kick read the same on every hit.
        for (auto k : { Kick { 50, 220, 0.025, 0.35, 0.4, 0.8 }, Kick { 61.74, 300, 0.02, 0.25, 0.2, 0.8 }, Kick { 43.65, 120, 0.04, 0.6, 0, 0.8 } })
        {
            Engine e; render (sr, Params(), kicks (sr, 1.2, 5, k), nullptr, 256, &e);
            CHECK (e.note() > 0 && cents (e.note(), k.f) > -5 && cents (e.note(), k.f) < 30, "dropping kick to %.2f Hz read as %.2f Hz (%.1f cents) at %g", k.f, e.note(), e.note() > 0 ? cents (e.note(), k.f) : 0.0, sr);
            CHECK (e.glideSemis() > 0.3, "a pitch drop should show as a glide (%.2f semitones)", e.glideSemis());
        }
        // an 808 that slides from one note to another and holds it
        Buf s ((size_t) (sr * 1.6), 0.0f); double ph = 0;
        for (size_t i = (size_t) (0.1 * sr); i < s.size(); ++i)
        {
            const double t = i / sr - 0.1, f = t < 0.4 ? 55.0 : t < 0.6 ? 55.0 * std::pow (49.0 / 55.0, (t - 0.4) / 0.2) : 49.0;
            ph += 2 * pi * f / sr; s[i] = (float) (0.6 * std::sin (ph) * std::min (1.0, t / 0.001) * std::exp (-t / 2.0));
        }
        Engine e; e.setParams (Params()); e.prepare (sr);
        Buf first (s.begin(), s.begin() + (long) (0.45 * sr)), rest (s.begin() + (long) (0.45 * sr), s.end());
        run (e, first, nullptr, 256);
        CHECK (std::abs (cents (e.noteNow(), 55.0)) < 5, "808 first note read as %.2f Hz at %g", e.noteNow(), sr);
        run (e, rest, nullptr, 256);
        CHECK (std::abs (cents (e.noteNow(), 49.0)) < 5, "808 after the slide read as %.2f Hz at %g", e.noteNow(), sr);
    }
}

static void testTune()
{
    for (double sr : rates)
        for (double semis : { -3.0, -1.0, 1.0, 2.0 })
        {
            const Kick k { 60, 60, 0.03, 0.12, 0.3, 0.7 };
            const Buf in = kicks (sr, 2.4, 0.8, k);
            Params p; p.tune = (float) semis;
            Engine e; const Out o = render (sr, p, in, nullptr, 256, &e);
            const int lat = e.latency();
            CHECK (lat > 0 && lat <= (int) (0.07 * sr), "tuning %+g should report a small latency, got %d samples", semis, lat);
            const int at = (int) (0.9 * sr) + lat, a = at + (int) (0.04 * sr), b = at + (int) (0.2 * sr);          // the second hit
            const double want = 60 * Engine::ratioOf (semis), got = strongest (o.l, sr, 40, 90, a, b);
            CHECK (std::abs (cents (got, want)) < 12, "tuned %+g semitones: wanted %.2f Hz, got %.2f (%g Hz)", semis, want, got, sr);
            CHECK (std::abs (dB (peak (o.l, at - 100, at + (int) (0.05 * sr))) - dB (peak (in, at - lat - 100, at - lat + (int) (0.05 * sr)))) < 1.5, "tuning must keep the hit's peak (%+g, %g Hz)", semis, sr);
            // the hit starts where the host expects it: input start + latency, within 1.5 ms
            auto start = [&] (const Buf& x, int from) { for (int i = from; i < (int) x.size(); ++i) if (std::abs (x[(size_t) i]) > 0.05) return i; return -1; };
            const int si = start (in, (int) (0.85 * sr)), so = start (o.l, (int) (0.85 * sr) + lat - (int) (0.002 * sr));
            CHECK (std::abs ((so - si) - lat) < (int) (0.0015 * sr), "tuned hit starts %d samples after the input, latency says %d (%+g, %g Hz)", so - si, lat, semis, sr);
        }
    Engine e; e.setParams (Params()); e.prepare (48000);
    CHECK (e.latency() == 0, "no tuning, no latency");
}

static void testControls()
{
    const double sr = 48000;
    const Kick k { 52, 160, 0.03, 0.18, 0.15, 0.5 };
    const Buf in = kicks (sr, 4.0, 0.5, k);
    const int a = (int) (3.1 * sr), b = (int) (3.6 * sr);                   // the seventh hit, once things have settled
    auto with = [&] (float Params::* f, float v) { Params p; p.*f = v; return render (sr, p, in).l; };
    auto lows = [&] (const Buf& x) { return dB (rms (filtered (x, sr, 30, 150), a, b)); };
    auto tailLevel = [&] (const Buf& x) { return dB (rms (x, a + (int) (0.25 * sr), a + (int) (0.4 * sr))); };
    auto highs = [&] (const Buf& x) { return dB (rms (filtered (x, sr, 2500, 0), a, a + (int) (0.02 * sr))); };
    CHECK (lows (with (&Params::thump, 1)) - lows (in) > 3, "Thump up should lift the boom: %.1f dB", lows (with (&Params::thump, 1)) - lows (in));
    CHECK (lows (with (&Params::thump, -1)) - lows (in) < -3, "Thump down should lower the boom");
    CHECK (dB (peak (with (&Params::punch, 1), a, b)) - dB (peak (in, a, b)) > 1.5, "Punch up should lift the hit's peak");
    CHECK (std::abs (tailLevel (with (&Params::punch, 1)) - tailLevel (in)) < 1, "Punch must leave the tail alone");
    CHECK (highs (with (&Params::click, 1)) - highs (in) > 4, "Click up should lift the click: %.1f dB", highs (with (&Params::click, 1)) - highs (in));
    CHECK (highs (with (&Params::click, -1)) - highs (in) < -3, "Click down should soften the click");
    CHECK (tailLevel (with (&Params::tail, -1)) - tailLevel (in) < -10, "Tail down should shorten the tail");
    CHECK (tailLevel (with (&Params::tail, 1)) - tailLevel (in) > 5, "Tail up should lengthen the tail");
    CHECK (std::abs (dB (peak (with (&Params::tail, -1), a, b)) - dB (peak (in, a, b))) < 0.5, "Tail must leave the hit itself alone");
}

// A phone can't play much under 180 Hz. A kick that is only boom vanishes there; Click must bring it back.
static void testSmallSpeaker()
{
    for (double sr : rates)
    {
        const Buf in = kicks (sr, 4.0, 0.5, { 45, 45, 0.03, 0.3, 0, 0.6 });      // a pure sub kick: nothing a phone can play
        const int a = (int) (3.1 * sr), b = (int) (3.5 * sr);
        auto phone = [&] (const Buf& x) { return dB (rms (filtered (x, sr, 180, 0), a, b)) - dB (rms (in, a, b)); };
        Params p; p.click = 1;
        const Buf out = render (sr, p, in).l;
        CHECK (phone (in) < -30, "the test kick should be inaudible on a phone to begin with (%.1f dB)", phone (in));
        CHECK (phone (out) > -26, "with Click up a sub kick must be heard on a phone: %.1f dB under the kick (%g Hz)", phone (out), sr);
        CHECK (std::abs (dB (rms (filtered (out, sr, 0, 120), a, b)) - dB (rms (filtered (in, sr, 0, 120), a, b))) < 1, "Click must not change the boom");
    }
}

static void testDuck()
{
    for (double sr : rates)
    {
        const Buf bass = tone (sr, 3.0, { { 50, 0.3 }, { 1000, 0.1 } }), kick = kicks (sr, 3.0, 1.0, { 50, 150, 0.03, 0.15, 0.2, 0.8 }, 0.5);
        Params p; p.role = 1; p.room = 1;
        const Buf out = render (sr, p, bass, &kick).l;
        const int hit = (int) (1.5 * sr), a = hit + (int) (0.005 * sr), b = hit + (int) (0.045 * sr);          // 40 ms = whole cycles of 50 and 1000 Hz
        const double lowCut = dB (levelAt (out, sr, 50, a, b)) - dB (levelAt (bass, sr, 50, a, b)), highCut = dB (levelAt (out, sr, 1000, a, b)) - dB (levelAt (bass, sr, 1000, a, b));
        CHECK (lowCut < -6, "the bass's lows should dip under the kick: %.1f dB (%g Hz)", lowCut, sr);
        CHECK (std::abs (highCut) < 0.3, "the dip must leave 1 kHz alone: %.2f dB (%g Hz)", highCut, sr);
        const int q = hit + (int) (0.8 * sr);                                                                      // long after the hit: back to normal
        CHECK (std::abs (dB (levelAt (out, sr, 50, q, q + (int) (0.04 * sr))) - dB (levelAt (bass, sr, 50, q, q + (int) (0.04 * sr)))) < 0.5, "the bass should be back before the next kick (%g Hz)", sr);
        p.room = 0;
        const Buf off = render (sr, p, bass, &kick).l;
        CHECK (std::memcmp (off.data(), bass.data(), bass.size() * sizeof (float)) == 0, "Room for bass at zero must change nothing");
    }
}

static void testSit()
{
    const double sr = 48000, lo = -9, target = -6, hi = -3;       // a made-up target range, only for the test
    const Buf bass = tone (sr, 6.0, { { 82, 0.12 }, { 164, 0.1 }, { 820, 0.08 } });
    auto level = [&] (double amp, double gainDb, double& out)
    {
        Params p; p.gainDb = (float) gainDb; Engine e;
        render (sr, p, kicks (sr, 6.0, 0.5, { 52, 160, 0.03, 0.18, 0.15, amp }), &bass, 256, &e);
        double change = 0; const bool ok = e.sitLevel (out) && e.sitGain (lo, target, hi, change);
        CHECK (ok, "Sit it should be able to measure a kick with a sidechain");
        return change;
    };
    double now = 0, after = 0;
    // find an amplitude that already sits inside the range, and check Sit it leaves it alone
    double inside = 0;
    for (double amp = 0.05; amp < 1 && inside == 0; amp *= 1.15) { level (amp, 0, now); if (now > lo + 1 && now < hi - 1) inside = amp; }
    CHECK (inside > 0, "the test needs a kick that sits in range");
    CHECK (level (inside, 0, now) == 0, "a kick already in range must not be moved (it sits at %.1f dB)", now);
    for (double scale : { 0.3, 3.0 })
    {
        const double change = level (inside * scale, 0, now);
        level (inside * scale, change, after);
        CHECK (now < lo || now > hi, "the test kick should start out of range (%.1f dB)", now);
        CHECK (std::abs (after - target) < 1.0, "Sit it moved the kick from %.1f to %.1f dB, wanted %.1f", now, after, target);
    }
    Engine e; render (sr, Params(), kicks (sr, 4.0, 0.5, Kick()), nullptr, 256, &e);
    double c; CHECK (! e.sitGain (lo, target, hi, c), "without a sidechain Sit it must say it can't tell, not guess");
}

// On the vocal: only Level changes the sound, and Sit it moves the singing to the target against the rest of the song.
static void testVocal()
{
    const double sr = 48000, lo = -2, target = 0, hi = 2;          // a made-up target range, only for the test
    Buf voice ((size_t) (sr * 12), 0.0f), back = tone (sr, 12.0, { { 110, 0.1 }, { 440, 0.08 }, { 2500, 0.03 } });
    for (size_t i = 0; i < voice.size(); ++i)                        // sung phrases with gaps: 1.2 s on, 0.4 s off
    {
        const double t = (double) i / sr, ph = std::fmod (t, 1.6);
        if (ph < 1.2) voice[i] = (float) (0.1 * std::sin (2 * pi * 330 * t) + 0.05 * std::sin (2 * pi * 1320 * t));
    }
    auto run1 = [&] (double scale, double level, double& now, double& change, const Buf* side)
    {
        Buf v = voice; for (auto& x : v) x *= (float) scale;
        Params p; p.role = 2; p.gainDb = (float) level; Engine e; const Out o = render (sr, p, v, side, 256, &e);
        if (level == 0) CHECK (std::memcmp (o.l.data(), v.data(), v.size() * sizeof (float)) == 0, "on the vocal, Level at 0 must change nothing");
        return e.vocalLevel (now) && e.vocalGain (lo, target, hi, change);
    };
    double now = 0, change = 0, at1 = 0;
    CHECK (run1 (1, 0, at1, change, &back), "the vocal setting should be able to measure singing against a backing");
    const double inRange = std::pow (10.0, (target - at1) / 20);     // the scale that puts this voice on the target
    CHECK (run1 (inRange, 0, now, change, &back) && change == 0 && std::abs (now - target) < 0.5, "a vocal already in range must not be moved (it sits at %.1f dB)", now);
    for (double off : { -9.0, 7.0 })
    {
        const double scale = inRange * std::pow (10.0, off / 20);
        CHECK (run1 (scale, 0, now, change, &back) && std::abs (now - (target + off)) < 0.5, "a vocal %.0f dB off should read as that, read %.1f", off, now);
        double after = 0, c2 = 0; run1 (scale, change, after, c2, &back);
        CHECK (std::abs (after - target) < 1.0, "Sit it moved the vocal from %.1f to %.1f dB, wanted %.1f", now, after, target);
    }
    CHECK (! run1 (1, 0, now, change, nullptr), "without the rest of the song, the vocal setting must say it can't tell");
}

int main()
{
    testDoNoHarm(); testSafe(); testPitch(); testTune(); testControls(); testSmallSpeaker(); testDuck(); testSit(); testVocal();
    printf (failures ? "%d FAILED\n" : "kickfit: all tests passed\n", failures);
    return failures ? 1 : 0;
}
