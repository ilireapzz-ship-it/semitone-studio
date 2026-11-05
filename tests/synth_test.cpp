// Unit test for st::Synth (no JUCE needed): clang++ -std=c++17 -O2 -ISource tests/synth_test.cpp -o t && ./t
#include "Synth.h"
#include "ClipPlayer.h"
#include <cassert>
#include <cstdio>
#include <deque>
#include <string>

using namespace st;
static const double SR = 48000;
static std::deque<NoteBuffer> store;
static int failures = 0;
#define CHECK(c, ...) do { if (! (c)) { ++failures; printf ("FAIL %s:%d ", __FILE__, __LINE__); printf (__VA_ARGS__); printf ("\n"); } } while (0)

static PartPatch part (int ch, Kind k)
{
    PartPatch p; p.ch = ch; p.kind = k; p.vol = .24f; p.level = 1; return p;
}
static PartPatch junoPatch (int ch)
{
    auto p = part (ch, Kind::juno); p.vol = .32f; p.saw = .7f; p.pulse = .35f; p.pw = .45f; p.pwm = .15f; p.sub = .3f; p.cut = 900; p.res = .2f; p.env = 2.6f; p.kt = .6f;
    p.fe = { .002f, .45f, .25f, .35f }; p.ae = { .003f, .9f, .75f, .35f }; p.lfo = { 4, 0, 0, 0 }; p.hpHz = 90; return p;
}
static PartPatch prophetPatch (int ch)
{
    auto p = part (ch, Kind::prophet); p.vol = .32f; p.aWave = Wave::saw; p.aGain = .6f; p.bWave = Wave::pulse; p.bGain = .45f; p.bPw = .38f; p.det = 5; p.cut = 800; p.res = .25f; p.env = 2.8f; p.kt = .6f;
    p.fe = { .002f, .7f, .3f, .45f }; p.ae = { .003f, 1.2f, .6f, .45f }; p.drift = 0; return p;
}

// a patch with one part on channel 0, no room, and only the safety compressor on the master
static std::shared_ptr<SynthPatch> single (PartPatch p, const NoteBuffer* buf = nullptr, int key = 69)
{
    auto s = std::make_shared<SynthPatch>();
    NoteBuffers nb {}; nb.fill (nullptr); if (buf) nb[(size_t) key] = buf;
    s->parts.push_back (p); s->buffers.push_back (nb); s->drumCh = 9;
    return s;
}

struct Render { std::vector<float> l, r; };
static Render run (Synth& sy, std::vector<std::pair<int, Event>> evs, int total, int block = 256)
{
    Render o; o.l.assign ((size_t) total, 0); o.r.assign ((size_t) total, 0);
    std::vector<Event> now; size_t e = 0;
    std::stable_sort (evs.begin(), evs.end(), [] (auto& a, auto& b) { return a.first < b.first; });
    for (int i = 0; i < total; i += block)
    {
        const int n = std::min (block, total - i);
        now.clear();
        while (e < evs.size() && evs[e].first < i + n) { auto ev = evs[e].second; ev.offset = evs[e].first - i; now.push_back (ev); ++e; }
        sy.process (now, n, o.l.data() + i, o.r.data() + i);
    }
    return o;
}
static Event on (int ch, int key, int vel) { return { 0, (unsigned char) (0x90 | ch), (unsigned char) key, (unsigned char) vel }; }
static Event off (int ch, int key) { return { 0, (unsigned char) (0x80 | ch), (unsigned char) key, 0 }; }

static float peakOf (const std::vector<float>& x, int a, int b) { float p = 0; for (int i = a; i < b && i < (int) x.size(); ++i) p = std::max (p, std::abs (x[(size_t) i])); return p; }
static float rmsOf (const std::vector<float>& x, int a, int b) { double s = 0; int n = 0; for (int i = a; i < b && i < (int) x.size(); ++i, ++n) s += x[(size_t) i] * x[(size_t) i]; return n ? (float) std::sqrt (s / n) : 0; }

// the repeat rate, by autocorrelation: the shortest lag that matches nearly as well as the best lag
static double pitchOf (const std::vector<float>& x, int a, int len)
{
    const int lo = (int) (SR / 2000), hi = (int) (SR / 40);
    std::vector<double> c ((size_t) hi + 2, 0.0);
    double top = 0;
    for (int lag = lo - 1; lag <= hi + 1; ++lag)
    {
        double s = 0;
        for (int i = 0; i < len; ++i) s += x[(size_t) (a + i)] * x[(size_t) (a + i + lag)];
        c[(size_t) lag] = s;
        if (lag >= lo && lag <= hi) top = std::max (top, s);
    }
    int best = hi;
    for (int lag = lo; lag <= hi; ++lag)
        if (c[(size_t) lag] >= top * .8 && c[(size_t) lag] >= c[(size_t) (lag - 1)] && c[(size_t) lag] >= c[(size_t) (lag + 1)]) { best = lag; break; }
    const double y0 = c[(size_t) (best - 1)], y1 = c[(size_t) best], y2 = c[(size_t) (best + 1)];
    const double den = y0 - 2 * y1 + y2, sh = den != 0 ? .5 * (y0 - y2) / den : 0;
    return SR / (best + sh);
}

// how strong one frequency is in a stretch of sound
static double toneAt (const std::vector<float>& x, int a, int len, double hz)
{
    double re = 0, im = 0;
    for (int i = 0; i < len; ++i) { const double w = 2 * synthPi * hz * i / SR, h = .5 - .5 * std::cos (2 * synthPi * i / len); re += x[(size_t) (a + i)] * h * std::cos (w); im += x[(size_t) (a + i)] * h * std::sin (w); }
    return std::sqrt (re * re + im * im) / len;
}

static double cents (double f, double want) { return 1200 * std::log2 (f / want); }

int main()
{
    const double a440 = 440;

    // 1. every kind of instrument sounds, at the right pitch, and dies away after the note ends
    struct Case { const char* name; PartPatch p; const NoteBuffer* buf; int key; double tol; };
    store.push_back (renderPianoNote (69, SR));  const NoteBuffer* piano = &store.back();
    store.push_back (renderPluckNote (57, SR, Pluck::nylon)); const NoteBuffer* nylon = &store.back();
    store.push_back (renderPluckNote (45, SR, Pluck::bassgtr)); const NoteBuffer* bassgtr = &store.back();
    { const auto up = renderPluckNote (33, SR, Pluck::upright), fi = renderPluckNote (33, SR, Pluck::bassgtr); if (up.empty() || up.size() >= fi.size()) { std::printf ("FAIL upright bass should ring shorter than the fingered bass\n"); return 1; } }

    auto pianoP = part (0, Kind::buffer); pianoP.vol = .12f; pianoP.velCutoff = true; pianoP.bufRelease = .22f;
    auto pluckP = part (0, Kind::buffer); pluckP.vol = .32f; pluckP.bufRelease = .35f; pluckP.holdMin = .4f;
    auto sine = part (0, Kind::voice); sine.wave = Wave::sine; sine.cutoff = 3200;
    auto saw = part (0, Kind::voice); saw.wave = Wave::saw; saw.cutoff = 2400; saw.vol = .11f; saw.unison = .5f; saw.fenv = .4f;
    auto tri = part (0, Kind::voice); tri.wave = Wave::triangle; tri.decay = .35f;
    auto sq = part (0, Kind::voice); sq.wave = Wave::square; sq.vol = .09f; sq.cutoff = 6000;
    auto org = part (0, Kind::voice); org.wave = Wave::sine; org.cutoff = 7000; org.vol = .19f; org.numLayers = 2; org.layers[0] = { 2, .7f, Wave::sine }; org.layers[1] = { 3, .45f, Wave::sine };
    auto fm = part (0, Kind::fm); fm.vol = .32f;

    const Case cases[] = {
        { "sine", sine, nullptr, 69, 3 }, { "saw unison+filter env", saw, nullptr, 69, 6 }, { "triangle pluck", tri, nullptr, 69, 3 }, { "square", sq, nullptr, 69, 3 },
        { "organ layers", org, nullptr, 69, 3 }, { "electric piano", fm, nullptr, 69, 8 },
        { "piano", pianoP, piano, 69, 8 }, { "nylon", pluckP, nylon, 57, 8 }, { "fingered bass", pluckP, bassgtr, 45, 8 },
        { "juno keys", junoPatch (0), nullptr, 69, 10 }, { "prophet keys", prophetPatch (0), nullptr, 69, 10 },
    };
    for (const auto& c : cases)
    {
        Synth sy; sy.prepare (SR); sy.setPatch (single (c.p, c.buf, c.key));
        const int total = (int) (SR * 4.2), offAt = (int) (SR * .6);
        auto r = run (sy, { { 0, on (0, c.key, 92) }, { offAt, off (0, c.key) } }, total);
        const double want = 440 * std::pow (2.0, (c.key - 69) / 12.0);
        const float pk = peakOf (r.l, 0, offAt);
        CHECK (pk > .002f, "%s is silent (peak %f)", c.name, pk);
        CHECK (pk < 1.0f, "%s too loud (%f)", c.name, pk);
        const double f = pitchOf (r.l, (int) (SR * .1), 8192);
        CHECK (std::abs (cents (f, want)) < c.tol, "%s pitch %.2f Hz, wanted %.2f", c.name, f, want);
        CHECK (rmsOf (r.l, total - 4800, total) < pk * .01f, "%s did not die away", c.name);
        for (float v : r.l) CHECK (std::isfinite (v), "%s not finite", c.name);
        printf ("ok: %-22s peak %.3f  %.1f Hz\n", c.name, pk, f);
    }

    // the electric piano's sound is whole harmonics of the note, none in between (real FM ratio 1:1)
    {
        Synth sy; sy.prepare (SR); sy.setPatch (single (fm));
        auto r = run (sy, { { 0, on (0, 69, 92) } }, (int) (SR * 1.2));
        const int a = (int) (SR * .9), len = 16384;
        const double h1 = toneAt (r.l, a, len, 440), h2 = toneAt (r.l, a, len, 880), sub = toneAt (r.l, a, len, 440 / 3.0), between = toneAt (r.l, a, len, 660);
        CHECK (h1 > sub * 20 && h1 > between * 20, "electric piano not harmonic: 440=%g 147=%g 660=%g", h1, sub, between);
        printf ("ok: electric piano harmonics 440=%.4f 880=%.4f 147=%.5f 660=%.5f\n", h1, h2, sub, between);
    }

    // 2. the pitch bend moves the note by the exact amount (12 semitones = full range; +2 = 8192 + 8192/6)
    {
        Synth sy; sy.prepare (SR); sy.setPatch (single (sine));
        const int v = 8192 + 8192 / 6;
        Event b { 0, 0xE0, (unsigned char) (v & 127), (unsigned char) (v >> 7) };
        auto r = run (sy, { { 0, on (0, 69, 92) }, { 100, b } }, (int) (SR * .5));
        const double f = pitchOf (r.l, (int) (SR * .15), 8192);
        CHECK (std::abs (cents (f, a440 * std::pow (2.0, 2 / 12.0))) < 4, "bend gave %.2f Hz", f);
        printf ("ok: pitch bend %.2f Hz\n", f);
    }

    // 3. a note starts on the exact sample, whatever the block size
    for (int block : { 64, 256, 1024 })
    {
        Synth sy; sy.prepare (SR); auto p = sine; p.attack = .0001f; sy.setPatch (single (p));
        const int at = 1234;
        auto r = run (sy, { { at, on (0, 69, 92) } }, 4096, block);
        int first = -1;
        for (int i = 0; i < 4096; ++i) if (std::abs (r.l[(size_t) i]) > 1.0e-5f) { first = i; break; }
        CHECK (first >= at && first <= at + 3, "block %d: note began at %d, wanted %d", block, first, at);
    }
    printf ("ok: notes start on the exact sample\n");

    // 4. every drum in every kit sounds, and a kick has its energy low, a hat high
    {
        KitPatch k808, ac; ac.lp = 12000; ac.kick = { 110, 50, .28f, .5f }; ac.snare = { 210, .9f, .24f, 1100 }; ac.hat = { .05f, 7000 }; ac.open = { .35f, 6000 }; ac.rim = { 1200, .05f }; ac.crash = { 1.6f, 3500 }; ac.clap = .18f;
        KitPatch lofi; lofi.lp = 3200; lofi.kick = { 95, 48, .35f, .2f }; lofi.snare = { 190, .7f, .2f, 900 }; lofi.hat = { .06f, 5000 }; lofi.open = { .25f, 4500 }; lofi.rim = { 900, .05f }; lofi.crash = { 1, 2500 }; lofi.clap = .2f;
        KitPatch el; el.kick = { 220, 48, .42f, .8f }; el.snare = { 230, .8f, .18f, 2200 }; el.hat = { .035f, 9500 }; el.open = { .22f, 8500 }; el.rim = { 2200, .03f }; el.crash = { 1.2f, 6000 }; el.clap = .24f;
        for (const KitPatch* kit : { &k808, &ac, &lofi, &el })
            for (int key : { 36, 38, 39, 42, 46, 37, 49 })
            {
                auto s = std::make_shared<SynthPatch>(); s->drumCh = 9; s->kit = *kit;
                Synth sy; sy.prepare (SR); sy.setPatch (s);
                auto r = run (sy, { { 0, on (9, key, 100) } }, (int) (SR * 2.2));
                const float pk = peakOf (r.l, 0, (int) (SR * .1));
                CHECK (pk > .005f, "drum %d silent (peak %f)", key, pk);
                CHECK (sy.activeVoices() == 0, "drum %d never ended", key);
                for (float v : r.l) CHECK (std::isfinite (v), "drum %d not finite", key);
                int zc = 0; const int m = (int) (SR * .03);
                for (int i = 1; i < m; ++i) zc += (r.l[(size_t) i] < 0) != (r.l[(size_t) (i - 1)] < 0);
                if (kit == &k808 && key == 36) CHECK (zc < 30, "kick is not low: %d crossings", zc);
                if (kit == &k808 && key == 42) CHECK (zc > 100, "hat is not high: %d crossings", zc);
            }
        printf ("ok: 7 drums x 4 kits\n");
    }

    // 5. the mixer strip: level scales the part, pan moves it
    {
        auto quiet = sine; quiet.vol = .015f;                  // below the safety compressor, which is level-dependent
        Synth a; a.prepare (SR); a.setPatch (single (quiet));
        auto half = quiet; half.level = .5f; Synth b; b.prepare (SR); b.setPatch (single (half));
        auto left = quiet; left.pan = -1; Synth c; c.prepare (SR); c.setPatch (single (left));
        const std::vector<std::pair<int, Event>> e { { 0, on (0, 69, 92) } };
        const int t = (int) (SR * .4);
        auto ra = run (a, e, t), rb = run (b, e, t), rc = run (c, e, t);
        const float m = (float) (SR * .1);
        CHECK (std::abs (rmsOf (rb.l, (int) m, t) / rmsOf (ra.l, (int) m, t) - .5f) < .01f, "level");
        CHECK (rmsOf (rc.r, (int) m, t) < rmsOf (rc.l, (int) m, t) * .01f, "pan left");
        printf ("ok: level and pan\n");
    }

    // 6. the worst case: far more notes than voices, polish on, room on. Never over full scale, never NaN.
    {
        auto s = std::make_shared<SynthPatch>();
        NoteBuffers nb {}; nb.fill (nullptr);
        auto loud = saw; loud.vol = .4f; loud.attack = .001f;
        s->parts.push_back (loud); s->buffers.push_back (nb);
        s->polish.on = true; s->roomMix = .35f; s->roomT60 = 4.4f; s->drumCh = 9;
        Synth sy; sy.prepare (SR); sy.setPatch (s);
        std::vector<std::pair<int, Event>> e;
        for (int i = 0; i < 400; ++i) e.push_back ({ i * 40, on (0, 30 + i % 70, 127) });
        auto r = run (sy, e, (int) (SR * 3));
        float pk = std::max (peakOf (r.l, 0, (int) r.l.size()), peakOf (r.r, 0, (int) r.r.size()));
        CHECK (pk <= 1.0f, "over full scale: %f", pk);
        CHECK (sy.activeVoices() <= Synth::maxVoices, "too many voices");
        for (float v : r.l) CHECK (std::isfinite (v), "not finite under load");
        printf ("ok: 400 stacked notes, peak %.3f\n", pk);
    }

    // 7. the room rings on after the note, and stops by itself
    {
        auto s = single (sine); s->roomMix = .18f; s->roomT60 = .97f;
        Synth sy; sy.prepare (SR); sy.setPatch (s);
        const int offAt = (int) (SR * .3);
        auto r = run (sy, { { 0, on (0, 69, 92) }, { offAt, off (0, 69) } }, (int) (SR * 4));
        const float tail = rmsOf (r.l, (int) (SR * 1.0), (int) (SR * 1.1));
        CHECK (tail > 1.0e-5f, "no reverb tail");
        CHECK (rmsOf (r.l, (int) (SR * 3.8), (int) (SR * 4)) < tail * .05f, "reverb never fades");
        printf ("ok: room tail\n");
    }

    // 7b. the room must never feed itself: bright stabs for half a minute, then silence must really be silence.
    // (Regression: the room once had a loop gain above 1 at high frequencies and rose to full scale by itself.)
    for (float mix : { .18f, .35f })
    {
        auto bright = part (0, Kind::fm); bright.vol = .3f;
        auto s = single (bright); s->roomMix = mix; s->roomT60 = mix > .2f ? 4.4f : .97f; s->polish.on = true;
        Synth sy; sy.prepare (SR); sy.setPatch (s);
        std::vector<std::pair<int, Event>> e;
        const int stab = (int) (SR * .3), secs = 30;
        for (int i = 0; i * stab < (int) SR * secs; ++i)
            for (int key : { 60, 61, 65, 68, 84, 96 }) { e.push_back ({ i * stab, on (0, key, 110) }); e.push_back ({ i * stab + stab / 2, off (0, key) }); }
        auto r = run (sy, e, (int) (SR * (secs + 10)));
        const float during = rmsOf (r.l, (int) (SR * 10), (int) (SR * 12)), late = rmsOf (r.l, (int) (SR * 25), (int) (SR * 27));
        CHECK (late < during * 1.5f, "room is growing by itself: %.3f then %.3f", during, late);
        const float after = rmsOf (r.l, (int) (SR * (secs + 9)), (int) (SR * (secs + 10)));
        CHECK (after < 1.0e-3f, "still sounding 9 s after the last note: %.4f", after);
        printf ("ok: room stays stable (mix %.2f): %.3f, %.3f, then %.6f\n", mix, during, late, after);
    }

    // 8. no patch: silent, and quiet about it. audible=0: silent, while the voices keep their place.
    {
        Synth sy; sy.prepare (SR);
        auto r = run (sy, { { 0, on (0, 69, 92) } }, 4096);
        CHECK (peakOf (r.l, 0, 4096) == 0, "no patch should be silent");
        sy.setPatch (single (sine));
        std::vector<Event> ev { on (0, 69, 92) };
        std::vector<float> L (4096, 0.0f), R (4096, 0.0f);
        sy.process (ev, 4096, L.data(), R.data(), 0);          // ramps down from 1 inside this block
        std::fill (L.begin(), L.end(), 0.0f);
        ev.clear();
        sy.process (ev, 4096, L.data(), R.data(), 0);
        CHECK (peakOf (L, 0, 4096) == 0, "audible 0 should be silent");
        CHECK (sy.activeVoices() == 1, "the voice should keep running");
        sy.process (ev, 4096, L.data(), R.data(), 1);
        CHECK (peakOf (L, 2048, 4096) > .01f, "should come back");
        printf ("ok: silent without a patch, gate works\n");
    }

    // 9. the same song renders the same way twice, and a new patch never disturbs a held note
    {
        const std::vector<std::pair<int, Event>> e { { 0, on (0, 60, 92) }, { 0, on (0, 64, 92) } };
        Synth a, b; a.prepare (SR); b.prepare (SR); a.setPatch (single (saw)); b.setPatch (single (saw));
        auto ra = run (a, e, 8192), rb = run (b, e, 8192);
        CHECK (ra.l == rb.l, "not repeatable");
        auto p2 = single (sine);
        Synth c; c.prepare (SR); c.setPatch (single (saw));
        std::vector<float> L (4096, 0.0f), R (4096, 0.0f);
        std::vector<Event> ev { on (0, 60, 92) };
        c.process (ev, 4096, L.data(), R.data()); ev.clear();
        c.setPatch (p2);
        c.process (ev, 4096, L.data(), R.data());
        CHECK (c.activeVoices() == 1, "the held note should carry on");
        printf ("ok: repeatable, patch change is safe\n");
    }

    // 10. the handover: a rendered section takes over from the engine across its own beats, and only those
    {
        auto c = std::make_shared<Clip>();
        c->sr = 48000; c->frames = 48000; c->pcm.assign (96000, 3000);
        c->mb = { 0, 8 }; c->ms = { 0, 4 };
        auto bank = std::make_shared<Bank>();
        bank->items.push_back ({ c, 8.0 });
        ClipPlayer cp;
        CHECK (! cp.covers (9), "nothing is covered before the first block");
        cp.setBank (bank);
        std::vector<float> L (256, 0.0f), R (256, 0.0f);
        cp.process (true, 8, 120, SR, 256, 32, 0, L.data(), R.data());
        CHECK (peakOf (L, 0, 256) > .05f, "the clip should play");
        CHECK (! cp.covers (7.99), "before the section the engine plays");
        CHECK (cp.covers (8) && cp.covers (15.99), "inside the section the clip plays");
        CHECK (! cp.covers (16), "after the section the engine plays again");
        printf ("ok: rendered sections take over from the engine, and only for their own beats\n");
    }

    // automation lanes: a part's volume and pan follow the song position
    {
        Synth sy; sy.prepare (SR);
        auto pt = std::make_shared<SynthPatch>();
        PartPatch p; p.ch = 0; p.kind = Kind::voice; p.wave = Wave::sine; p.vol = .2f; p.cutoff = 8000; p.attack = .001f; p.level = 1;
        pt->parts.push_back (p);
        Lane vol; vol.ch = 0; vol.pts = { { 0, 1.0f }, { 2, 1.0f }, { 2.001, .25f }, { 4, .25f } };
        Lane pan; pan.ch = 0; pan.pan = true; pan.pts = { { 0, 0.0f }, { 2, 0.0f }, { 2.001, -1.0f }, { 4, -1.0f } };
        pt->lanes = { vol, pan };
        sy.setPatch (pt);
        const int blk = 480; std::vector<float> L ((size_t) blk), R ((size_t) blk);
        const double bps = 2.0 / SR;     // 120 BPM
        double beat = 0; std::vector<Event> ev; Event on {}; on.status = 0x90; on.d1 = 69; on.d2 = 100; on.offset = 0; ev.push_back (on);
        double a0 = 0, r1 = 0, l1 = 0; int n0 = 0, n1 = 0;
        while (beat < 3.5)
        {
            std::fill (L.begin(), L.end(), 0.f); std::fill (R.begin(), R.end(), 0.f);
            sy.process (ev, blk, L.data(), R.data(), 1, beat, bps); ev.clear();
            for (int i = 0; i < blk; ++i)
            {
                if (beat > .5 && beat < 1.8) { a0 += L[(size_t) i] * L[(size_t) i]; ++n0; }
                if (beat > 2.5) { l1 += L[(size_t) i] * L[(size_t) i]; r1 += R[(size_t) i] * R[(size_t) i]; ++n1; }
            }
            beat += blk * bps;
        }
        const double a1 = std::sqrt (l1 / n1), g = a1 / std::sqrt (a0 / n0), rr = std::sqrt (r1 / n1);
        if (g < .2 || g > .5 || rr > a1 * .1) { printf ("FAIL lanes: gain %.3f, right %.4f left %.4f\n", g, rr, a1); return 1; }
        printf ("ok: volume and pan lanes follow the song position\n");
    }

    printf (failures ? "%d synth tests FAILED\n" : "all synth tests passed\n", failures);
    return failures ? 1 : 0;
}
