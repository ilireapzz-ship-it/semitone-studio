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
                {   // a plug-in plays the drums: the built-in kit stays quiet
                    auto q = std::make_shared<SynthPatch>(); q->drumCh = 9; q->kit = *kit; q->drumExt = true;
                    Synth quiet; quiet.prepare (SR); quiet.setPatch (q);
                    auto qr = run (quiet, { { 0, on (9, key, 100) } }, (int) (SR * .2));
                    CHECK (peakOf (qr.l, 0, (int) (SR * .2)) == 0.f, "drum %d still plays under a plug-in", key);
                }
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

    // part effects: a filter darkens, an echo repeats, an effect lane moves its parameter, nothing runs away
    {
        auto run = [&] (std::vector<FxDesc> list, std::vector<Lane> lanes, double seconds, double from, double to)
        {
            Synth sy; sy.prepare (SR);
            auto pt = std::make_shared<SynthPatch>();
            PartPatch p; p.ch = 0; p.kind = Kind::voice; p.wave = Wave::saw; p.vol = .2f; p.cutoff = 12000; p.attack = .001f; p.release = .01f; p.level = 1;
            pt->parts.push_back (p); pt->lanes = lanes; pt->fx = makeFxBank (list, SR);
            sy.setPatch (pt);
            const int blk = 480; std::vector<float> L ((size_t) blk), R ((size_t) blk); std::vector<Event> ev; double t = 0, e = 0; int c = 0;
            Event on {}; on.status = 0x90; on.d1 = 81; on.d2 = 100; ev.push_back (on);
            bool off = false;
            while (t < seconds)
            {
                if (! off && t >= .5) { Event o {}; o.status = 0x80; o.d1 = 81; ev.push_back (o); off = true; }
                std::fill (L.begin(), L.end(), 0.f); std::fill (R.begin(), R.end(), 0.f);
                sy.process (ev, blk, L.data(), R.data(), 1, t * 2, 2.0 / SR); ev.clear();
                for (int i = 0; i < blk; ++i) { if (! std::isfinite (L[(size_t) i])) return -1.0; if (t >= from && t < to) { e += L[(size_t) i] * L[(size_t) i]; ++c; } }
                t += blk / SR;
            }
            return std::sqrt (e / std::max (1, c));
        };
        FxDesc lp; lp.ch = 0; lp.type = "filter"; lp.style = "Low-pass"; lp.p = { { "cutoff", 300 }, { "res", .1f }, { "mix", 1 } };
        FxDesc ec; ec.ch = 0; ec.type = "echo"; ec.style = "Digital"; ec.p = { { "sec", .6f }, { "fb", .5f }, { "tone", 6000 }, { "send", 1 }, { "mix", .5f } };
        const double dry = run ({}, {}, 2, .1, .45), dark = run ({ lp }, {}, 2, .1, .45), tail0 = run ({}, {}, 2, .7, .9), tail = run ({ ec }, {}, 2, .7, .9);
        Lane open; open.ch = 0; open.fxSlot = 0; open.fxParam = 0; open.pts = { { 0, 16000.f } };
        const double opened = run ({ lp }, { open }, 2, .1, .45);
        if (dry <= 0 || dark > dry * .9 || opened < dark * 1.1 || tail < tail0 * 5 + .001) { printf ("FAIL fx: dry %.4f dark %.4f opened %.4f tail %.5f vs %.5f\n", dry, dark, opened, tail, tail0); return 1; }
        FxDesc pl; pl.ch = 0; pl.type = "plate"; pl.p = { { "decay", 6 }, { "mix", 1 } };
        FxDesc hot = ec; hot.slot = 1; hot.p["fb"] = .92f; hot.style = "Tape";
        const double late = run ({ pl, hot }, {}, 20, 19, 20);
        for (const char* t : { "crush", "lofi", "autopan", "phaser", "wide" })
        {
            FxDesc x; x.ch = 0; x.type = t; x.style = ""; x.p = { { "mix", 1 }, { "width", 1 }, { "sec", .5f }, { "depth", .8f }, { "rate", .35f }, { "stages", 4 }, { "bits", 8 }, { "fb", .8f } };
            const double v = run ({ x }, {}, 2, .1, .45);
            if (v <= 0 || v > 2) { printf ("FAIL fx %s: %.4f\n", t, v); return 1; }
        }
        if (late < 0 || late > .5) { printf ("FAIL fx runaway %.4f\n", late); return 1; }
        printf ("ok: part effects shape the sound, follow their lanes and stay stable\n");
    }

    // transitions fire at their beat, and a take plays the right part of its file at the right beat
    {
        Synth sy; sy.prepare (SR);
        auto pt = std::make_shared<SynthPatch>();
        pt->trans = std::make_shared<std::vector<TransEv>>();
        TransEv im; im.kind = TransEv::impact; im.at = 2; im.dur = 1.7f; pt->trans->push_back (im);
        auto audio = std::make_shared<TakeAudio>(); audio->rate = SR; audio->data.assign ((size_t) SR * 4, 0.f);
        for (size_t i = (size_t) SR * 2; i < (size_t) SR * 3; ++i) audio->data[i] = .5f;   // silent, then 1 s of level
        pt->takes = std::make_shared<std::vector<TakeSeg>>();
        TakeSeg g; g.audio = audio; g.beat = 8; g.end = 12; g.start = 1; pt->takes->push_back (g);   // file second 1 lands on beat 8
        sy.setPatch (pt);
        const int blk = 240; std::vector<float> L ((size_t) blk), R ((size_t) blk);
        double beat = 0, bps = 2.0 / SR, firstT = -1, firstK = -1; const std::vector<Event> none;
        while (beat < 13)
        {
            std::fill (L.begin(), L.end(), 0.f); std::fill (R.begin(), R.end(), 0.f);
            sy.process (none, blk, L.data(), R.data(), 1, beat, bps);
            for (int i = 0; i < blk; ++i)
            {
                const double b = beat + i * bps;
                if (firstT < 0 && std::abs (L[(size_t) i]) > .001f) firstT = b;
                if (b > 7 && firstK < 0 && std::abs (L[(size_t) i]) > .05f) firstK = b;
            }
            beat += blk * bps;
        }
        // the take is silent for its first second (2 beats), so sound starts at beat 10
        if (firstT < 1.95 || firstT > 2.1 || firstK < 9.95 || firstK > 10.05) { printf ("FAIL transitions at %.3f, take at %.3f\n", firstT, firstK); return 1; }
        printf ("ok: transitions and takes land on their beats\n");
    }

    // plucked strings are in tune across the keyboard, at both common sample rates
    for (double rate : { 44100.0, 48000.0 })
        for (Pluck kind : { Pluck::nylon, Pluck::steel, Pluck::harp, Pluck::bassgtr, Pluck::upright })
            for (int key : { 33, 45, 57, 69, 81, 93 })
            {
                if ((kind == Pluck::bassgtr || kind == Pluck::upright) && key > 69) continue;
                const auto d = renderPluckNote (key, rate, kind);
                const double f = 440 * std::pow (2.0, (key - 69) / 12.0);
                const int s = (int) (rate * .05), n = std::min ((int) d.size() - s - 4000, (int) (rate * .25));
                int lo = (int) (rate / (f * 1.1)), hi = (int) (rate / (f / 1.1)), lag = lo; double best = -1e9;
                for (int l = lo; l <= hi; ++l) { double c = 0; for (int i = 0; i < n; ++i) c += d[(size_t) (s + i)] * d[(size_t) (s + i + l)]; if (c > best) { best = c; lag = l; } }
                auto y = [&] (int l) { double c = 0; for (int i = 0; i < n; ++i) c += d[(size_t) (s + i)] * d[(size_t) (s + i + l)]; return c; };
                const double A = y (lag - 1), B = y (lag), C = y (lag + 1), got = rate / (lag + .5 * (A - C) / (A - 2 * B + C)), off = std::abs (cents (got, f));
                if (off > 8) { printf ("FAIL pluck kind %d key %d at %.0f: %.1f cents\n", (int) kind, key, rate, off); return 1; }
            }
    printf ("ok: plucked strings are in tune across the keyboard\n");

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

    // FL Studio's "Macro 1": once the host moves it, the macro's controls follow the host value, not the song's line
    {
        Synth sy; sy.prepare (SR);
        std::array<std::atomic<float>, 8> mv {}; std::array<std::atomic<bool>, 8> on {};
        for (int i = 0; i < 8; ++i) { mv[(size_t) i] = .5f; on[(size_t) i] = false; }
        sy.macroVals = mv.data(); sy.macroOn = on.data();
        auto pt = std::make_shared<SynthPatch>();
        PartPatch p; p.ch = 0; p.kind = Kind::voice; p.wave = Wave::sine; p.vol = .2f; p.cutoff = 8000; p.attack = .001f; p.level = 1;
        pt->parts.push_back (p);
        Lane line; line.ch = 0; line.pts = { { 0, 1.0f }, { 100, 1.0f } };
        Lane mac; mac.ch = 0; mac.macro = 0; for (int j = 0; j < 17; ++j) mac.tab[(size_t) j] = 1.0f - .9f * (float) j / 16.0f;   // macro 0 → 1.0, 1 → 0.1
        pt->lanes = { line, mac };
        sy.setPatch (pt);
        const int blk = 480; std::vector<float> L ((size_t) blk), R ((size_t) blk);
        const double bps = 2.0 / SR;
        auto level = [&] (double& beat, std::vector<Event>& ev, double secs)
        {
            double a = 0; int n = 0; const int blocks = (int) (secs * SR / blk);
            for (int b = 0; b < blocks; ++b)
            {
                std::fill (L.begin(), L.end(), 0.f); std::fill (R.begin(), R.end(), 0.f);
                sy.process (ev, blk, L.data(), R.data(), 1, beat, bps); ev.clear();
                if (b > blocks / 3) for (int i = 0; i < blk; ++i) { a += L[(size_t) i] * L[(size_t) i]; ++n; }
                beat += blk * bps;
            }
            return std::sqrt (a / std::max (1, n));
        };
        double beat = 0; std::vector<Event> ev; Event e {}; e.status = 0x90; e.d1 = 69; e.d2 = 100; e.offset = 0; ev.push_back (e);
        const double before = level (beat, ev, .5);           // FL hasn't touched it: the song's line (1.0)
        on[0] = true; mv[0] = 1.0f;
        const double after = level (beat, ev, .5);            // FL turned Macro 1 up: the table's 0.1
        CHECK (before > 0 && after / before > .05 && after / before < .2, "macro: before %.4f after %.4f", before, after);
        if (after / before < .2) printf ("ok: FL Studio's macro takes over its controls in the engine\n");
    }

    // your plug-ins join the mix on their strip: fader, pan, and they keep running when the engine would go idle
    {
        auto p = std::make_shared<SynthPatch> (); p->hasExt = true; p->extLevel[1] = 1; p->polish.on = false;
        Synth sy; sy.prepare (SR); sy.setPatch (p);
        std::vector<float> in (512), L (512), R (512);
        for (int i = 0; i < 512; ++i) in[(size_t) i] = .02f * (float) std::sin (2 * M_PI * 468.75 * i / SR);   // a whole number of cycles
        double el = 0, er = 0;
        auto level = [&] (int blocks) { for (int k = 0; k < blocks; ++k) { el = er = 0; std::fill (L.begin(), L.end(), 0.f); std::fill (R.begin(), R.end(), 0.f); sy.setExternal (1, in.data(), in.data()); sy.process ({}, 512, L.data(), R.data()); for (int i = 0; i < 512; ++i) { el += L[(size_t) i] * L[(size_t) i]; er += R[(size_t) i] * R[(size_t) i]; } } return std::sqrt (el / 512); };
        const double full = level (60);
        CHECK (full > .004 && full < .2, "plug-in level through the mix %f", full);
        p = std::make_shared<SynthPatch> (*p); p->extLevel[1] = .5f; sy.setPatch (p);
        const double half = level (60);
        CHECK (std::abs (half / full - .5) < .05, "fader halves it %f", half / full);
        p = std::make_shared<SynthPatch> (*p); p->extLevel[1] = 1; p->extPan[1] = -1; sy.setPatch (p); level (60);
        CHECK (el > er * 16, "pan moves it left %f %f", el, er);
        CHECK (level (1500) > .004, "still sounding after the idle time");
        printf ("ok: plug-ins on their mixer strip\n");
    }

    // sidechain: the part dips at each hit, comes back, and the meter sees it
    {
        Duck d; d.ch = 1; d.depth = .8f; d.release = .3f; d.beats = { 4, 8 };
        const double spb = .5;
        CHECK (d.gainAt (3.9, spb) == 1, "no dip before the first hit");
        CHECK (std::abs (d.gainAt (4.02, spb) - .2f) < .05, "full dip just after a hit %f", d.gainAt (4.02, spb));
        CHECK (d.gainAt (4.4, spb) > .6f && d.gainAt (7.9, spb) > .99f, "recovers %f %f", d.gainAt (4.4, spb), d.gainAt (7.9, spb));
        Duck q = d; q.beats = { 4, 4.1 };   // a second hit while still down: no jump back up
        CHECK (q.gainAt (4.1001, spb) < .5f, "no pop between close hits %f", q.gainAt (4.1001, spb));

        auto p = std::make_shared<SynthPatch> (); p->hasExt = true; p->extLevel[1] = 1; p->polish.on = false;
        p->ducks.push_back (d);
        Synth sy; sy.prepare (SR); sy.setPatch (p);
        std::vector<float> in (512, .1f), L (512), R (512);
        sy.setExternal (1, in.data(), in.data());
        auto at = [&] (double beat) { std::fill (L.begin(), L.end(), 0.f); std::fill (R.begin(), R.end(), 0.f); sy.process ({}, 512, L.data(), R.data(), 1, beat, 2.0 / SR); return std::abs (L[500]); };
        at (3); const double up = at (3.5); const double down = at (4.01);
        CHECK (down < up * .4, "engine ducks the strip %f %f", down, up);
        CHECK (sy.takeChannelPeak (1) > .01f && sy.takeChannelPeak (1) == 0, "channel meter reads then clears");
        printf ("ok: sidechain and channel meters\n");
    }

    // your effect plug-ins on a built-in part: a chain that changes nothing sounds exactly the same, one that does is heard
    {
        auto mk = [] { auto p = single (junoPatch (0)); p->polish.on = false; return p; };
        std::vector<std::pair<int, Event>> evs { { 100, on (0, 57, 100) }, { 30000, off (0, 57) } };
        Synth a; a.prepare (SR); a.setPatch (mk()); const auto dry = run (a, evs, 48000);
        Synth b; b.prepare (SR); b.setPatch (mk()); int calls = 0;
        b.plugFx = [&] (int ch, float*, float*, int) { if (ch == 0) ++calls; }; b.plugFxMask = 1u;
        const auto same = run (b, evs, 48000);
        float diff = 0; for (size_t i = 0; i < dry.l.size(); ++i) diff = std::max (diff, std::max (std::abs (dry.l[i] - same.l[i]), std::abs (dry.r[i] - same.r[i])));
        CHECK (calls > 100 && diff < 1e-5f && peakOf (dry.l, 0, 48000) > .01f, "a do-nothing plug-in chain keeps the sound %d %g", calls, diff);
        Synth c; c.prepare (SR); c.setPatch (mk());
        c.plugFx = [] (int, float* L, float* R, int n) { for (int i = 0; i < n; ++i) { L[i] *= .25f; R[i] *= .25f; } }; c.plugFxMask = 1u;
        const auto quiet = run (c, evs, 48000);
        const float rd = rmsOf (dry.l, 2000, 28000), rq = rmsOf (quiet.l, 2000, 28000);
        CHECK (rq < rd * .3f && rq > rd * .2f, "the plug-in's change is heard %f %f", rq, rd);
        Synth d; d.prepare (SR); d.setPatch (mk()); d.plugFx = [] (int, float* L, float*, int n) { for (int i = 0; i < n; ++i) L[i] = 0; }; d.plugFxMask = 2u;   // effects on another channel leave this one alone
        const auto other = run (d, evs, 48000); float d2 = 0; for (size_t i = 0; i < dry.l.size(); ++i) d2 = std::max (d2, std::abs (dry.l[i] - other.l[i]));
        CHECK (d2 < 1e-5f, "a plug-in on another part changes nothing here %g", d2);
        CHECK (c.takeChannelPeak (0) > 0, "the part's meter reads after its plug-ins");
        printf ("ok: plug-in effects on built-in parts\n");
    }

    // audio tracks: each on its own channel with its own level and pan; stereo takes keep their sides; Mono sums a channel
    {
        auto run = [&] (std::shared_ptr<SynthPatch> pt, double& outL, double& outR, int ch)
        {
            Synth sy; sy.prepare (SR); sy.setPatch (pt);
            const int blk = 256; std::vector<float> L ((size_t) blk), R ((size_t) blk); const std::vector<Event> none;
            outL = outR = 0; double beat = 0, bps = 2.0 / SR;
            for (int k = 0; k < 200; ++k)
            {
                std::fill (L.begin(), L.end(), 0.f); std::fill (R.begin(), R.end(), 0.f);
                sy.process (none, blk, L.data(), R.data(), 1, beat, bps);
                for (int i = 0; i < blk; ++i) { outL += std::abs (L[(size_t) i]); outR += std::abs (R[(size_t) i]); }
                beat += blk * bps;
            }
            return sy.takeChannelPeak (ch);
        };
        auto st = std::make_shared<TakeAudio>(); st->rate = SR; st->chans = 2; st->data.assign ((size_t) SR * 4 * 2, 0.f);
        for (size_t i = 0; i < (size_t) SR * 4; ++i) st->data[i * 2] = .4f;              // left only
        auto mk = [&] (int ch, float pan, uint32_t mono)
        {
            auto pt = std::make_shared<SynthPatch>();
            pt->takes = std::make_shared<std::vector<TakeSeg>>();
            TakeSeg g; g.audio = st; g.beat = 0; g.end = 64; g.start = 0; g.ch = ch; pt->takes->push_back (g);
            pt->takeChPan[(size_t) ch] = pan; pt->monoMask = mono;
            return pt;
        };
        double l0, r0, l1, r1, l2, r2;
        const float pk = run (mk (4, 0, 0), l0, r0, 4);
        run (mk (4, 0, 1u << 4), l1, r1, 4);
        run (mk (6, -1, 1u << 6), l2, r2, 6);
        bool ok = l0 > 1 && r0 < l0 * .01 && pk > 0;           // stereo take: left stays left, and channel 4's meter moves
        ok = ok && l1 > 1 && std::abs (l1 - r1) < l1 * .01;   // Mono: both sides alike
        ok = ok && l2 > 1 && r2 < l2 * .01;                    // mono and panned hard left
        auto mo = std::make_shared<TakeAudio>(); mo->rate = SR; mo->data.assign ((size_t) SR * 4, .3f);
        auto pt = std::make_shared<SynthPatch>(); pt->takes = std::make_shared<std::vector<TakeSeg>>();
        TakeSeg g; g.audio = mo; g.beat = 0; g.end = 64; g.start = -.5; g.ch = 15; pt->takes->push_back (g);   // starts half a second in
        double l3, r3; run (pt, l3, r3, 15);
        ok = ok && l3 > 1 && std::abs (l3 - r3) < l3 * .01;
        pt->takeChLevel[15] = 0; double l4, r4; run (pt, l4, r4, 15);
        ok = ok && l4 < 1e-6;
        if (! ok) { printf ("FAIL audio tracks: st %.2f/%.2f pk %.3f mono %.2f/%.2f left %.2f/%.2f vox %.2f/%.2f off %.4f\n", l0, r0, pk, l1, r1, l2, r2, l3, r3, l4); ++failures; }
        else printf ("ok: audio tracks: own channel, level, pan, stereo sides and Mono\n");
    }

    // routing: a part into a bus (its fader and effects), sends into a return (post or pre fader), bus meters
    {
        auto mk = [] (float lvl) { auto p = std::make_shared<SynthPatch> (); p->hasExt = true; p->extLevel[1] = lvl; p->polish.on = false; return p; };
        std::vector<float> in (512), zero (512, 0.f), L (512), R (512);
        for (size_t i = 0; i < in.size(); ++i) in[i] = .004f * (float) std::sin (2 * synthPi * 750 * (double) i / SR);   // quiet: under the safety compressor
        auto blockOf = [&] (Synth& sy, const std::vector<float>& src) { std::fill (L.begin(), L.end(), 0.f); std::fill (R.begin(), R.end(), 0.f); sy.setExternal (1, src.data(), src.data()); sy.process ({}, 512, L.data(), R.data(), 1, -1, 0); double e = 0; for (float x : L) e += (double) x * x; return (float) std::sqrt (e / 512); };
        auto settle = [&] (Synth& sy, const std::vector<float>& src) { float v = 0; for (int k = 0; k < 8; ++k) v = blockOf (sy, src); return v; };
        Synth a; a.prepare (SR); a.setPatch (mk (1)); const float dry = settle (a, in);
        auto pb = mk (1); pb->rbus.push_back ({ .5f, 0, false }); pb->rOut[1] = 0; pb->routeMask = 2;
        Synth b; b.prepare (SR); b.setPatch (pb); const float bus = settle (b, in);
        CHECK (dry > 1e-3f && std::abs (bus - dry * .5f) < dry * .02f, "a part routed into a bus at -6 dB comes out at half %f %f", bus, dry);
        CHECK (b.takeChannelPeak (17) > 1e-3f && b.takeChannelPeak (17) == 0, "the bus has its own meter (channel 17)");
        // a return with a plate: the send adds a tail that keeps going after the part stops
        auto pc = mk (1); pc->rbus.push_back ({ 1, 0, false }); pc->rSend[1][0] = .5f; pc->routeMask = 2;
        FxDesc pl; pl.type = "plate"; pl.ch = 17; pl.slot = 0; pl.p = { { "decay", 1.8f }, { "pre", 20 }, { "damp", 7000 }, { "mix", 1 } };
        pc->fx = makeFxBank ({ pl }, SR);
        Synth c; c.prepare (SR); c.setPatch (pc); settle (c, in); for (int k = 0; k < 6; ++k) blockOf (c, zero); const float tail = blockOf (c, zero);
        Synth a2; a2.prepare (SR); a2.setPatch (mk (1)); settle (a2, in); for (int k = 0; k < 6; ++k) blockOf (a2, zero); const float none = blockOf (a2, zero);
        CHECK (tail > dry * .01f && none < dry * 1e-3f, "the Plate return rings on after the part stops %g %g", tail, none);
        // pre-fader: with the part's fader down the send still feeds the return; post-fader it follows the fader
        auto pp = mk (0); pp->rbus.push_back ({ 1, 0, false }); pp->rSend[1][0] = 1; pp->rPre[1] = 1; pp->routeMask = 2;
        Synth d; d.prepare (SR); d.setPatch (pp); const float pre = settle (d, in);
        auto po = mk (0); po->rbus.push_back ({ 1, 0, false }); po->rSend[1][0] = 1; po->routeMask = 2;
        Synth e; e.prepare (SR); e.setPatch (po); const float post = settle (e, in);
        CHECK (std::abs (pre - dry) < dry * .02f && post < dry * 1e-3f, "pre-fader sends ignore the fader, post-fader ones follow it %f %f %f", pre, post, dry);
        // with plug-in effects on the part (the second pass), routing still holds
        Synth f; f.prepare (SR); f.setPatch (pb); f.plugFx = [] (int, float*, float*, int) {}; f.plugFxMask = 2u; const float bus2 = settle (f, in);
        CHECK (std::abs (bus2 - bus) < dry * .02f, "routing is the same when the part has plug-in effects %f %f", bus2, bus);
        printf ("ok: routing: buses, returns, pre and post sends, bus meters\n");
    }

    // Smart EQ: the engine's bands answer like the page's BiquadFilterNodes (getFrequencyResponse values from the page, 48 kHz)
    {
        const double sr = 48000; const double F[] = { 50, 100, 200, 400, 1000, 2500, 5000, 8000, 12000 };
        struct Set { const char* name; std::vector<std::pair<std::string, float>> p; double want[9]; };
        const std::vector<Set> sets = {
            { "bell", { { "n", 1 }, { "t0", 0 }, { "f0", 1000 }, { "g0", 6 }, { "q0", 2 } }, { 0, .02, .07, .34, 6, .34, .06, .02, .01 } },
            { "shelves", { { "n", 2 }, { "t0", 1 }, { "f0", 200 }, { "g0", -4 }, { "t1", 2 }, { "f1", 6000 }, { "g1", 3 } }, { -3.98, -3.76, -2, -.24, 0, .08, .94, 2.37, 2.91 } },
            { "cuts 24/48", { { "n", 2 }, { "t0", 3 }, { "f0", 150 }, { "s0", 24 }, { "t1", 4 }, { "f1", 7000 }, { "s1", 48 } }, { -38.17, -14.25, -.41, 0, 0, 0, -.01, -11.29, -49.12 } },
            { "tilt", { { "n", 1 }, { "t0", 6 }, { "f0", 1000 }, { "g0", 6 } }, { -3, -3, -2.99, -2.85, 0, 2.85, 2.99, 3, 3 } },
            { "band pass", { { "n", 1 }, { "t0", 7 }, { "f0", 1000 }, { "q0", 1.5f } }, { -29.54, -23.47, -17.24, -10.4, 0, -10.47, -17.55, -22.33, -27.16 } },
            { "cut 12", { { "n", 1 }, { "t0", 3 }, { "f0", 300 }, { "s0", 12 } }, { -31.13, -19.14, -7.83, -1.19, -.03, 0, 0, 0, 0 } } };
        for (auto& s : sets)
        {
            FxDesc d; d.type = "peq"; for (auto& kv : s.p) d.p[kv.first] = kv.second;
            for (int k = 0; k < 9; ++k)
            {
                PeqFx eq (d, sr); const int n = (int) sr; double in2 = 0, out2 = 0;
                for (int i = 0; i < n; ++i) { float l = (float) std::sin (2 * synthPi * F[k] * i / sr), r = l; const float x = l; eq.run (l, r); if (i > n / 2) { in2 += x * x; out2 += l * l; } }
                const double db = 10 * std::log10 (std::max (1e-14, out2 / in2));
                CHECK (std::abs (db - s.want[k]) < (s.want[k] < -30 ? 1.5 : .15), "smart EQ %s at %g Hz: engine %.2f dB, page %.2f dB", s.name, F[k], db, s.want[k]);
            }
        }
        FxDesc d; d.type = "peq"; d.ch = 3; d.p = { { "n", 1 }, { "t0", 5 }, { "f0", 2500 }, { "q0", 4 } };
        auto bank = makeFxBank ({ d }, sr); CHECK (bank->eq[3] && bank->any[3] && ! bank->eq[2], "smart EQ lands on its strip");
        float l = 0, r = 0, peakOut = 0; for (int i = 0; i < 48000; ++i) { l = r = (float) std::sin (2 * synthPi * 2500 * i / sr); bank->run (3, l, r); if (i > 24000) peakOut = std::max (peakOut, std::abs (l)); }
        CHECK (peakOut < .01f, "smart EQ notch removes 2.5 kHz (peak %.4f)", peakOut);
    }


    // Smart compressor: each character in the engine matches the page's worklet (PcDsp) on the same signal, sample for sample
    {
        const double sr = 48000; const int N = 19200;
        const double want[5][15] = { {-0.127932,0.129838,-0.131658,1.020103,-0.394286,0.332191,-0.319604,0.079558,-0.092087,0.109400,-0.126995,0.427329,-0.342411,0.315749,-0.308161},
{-0.132443,0.134498,-0.136441,1.006484,-0.325814,0.279637,-0.271496,0.068220,-0.086219,0.104504,-0.122385,0.404591,-0.289134,0.270418,-0.266004},
{-0.137097,0.139224,-0.141236,0.526981,-0.465305,0.470424,-0.475161,0.123265,-0.166386,0.202747,-0.228606,0.449617,-0.426212,0.427465,-0.428355},
{-0.126690,0.128655,-0.130394,1.054196,-0.513033,0.390909,-0.361503,0.088886,-0.093410,0.104162,-0.119789,0.413728,-0.409627,0.366347,-0.346995},
{-0.116689,0.118499,-0.120211,0.974517,-0.706556,0.490086,-0.380377,0.081915,-0.079910,0.081173,-0.083046,0.255365,-0.260924,0.264121,-0.266099} };
        const char* nm[5] = { "Clean", "Punch", "Fast", "Smooth", "Glue" };
        auto sig = [&] (int i) { const double a = i < 4800 ? .1 : i < 9600 ? .8 : i < 14400 ? .2 : .6; return a * std::sin (2 * synthPi * 220 * i / sr); };
        for (int c = 0; c < 5; ++c)
        {
            FxDesc d; d.type = "pcomp"; d.p = { { "amt", .5f }, { "c", (float) c }, { "bpm", 120 } }; PcompFx pc (d, sr); double worst = 0; int k = 0;
            for (int i = 0; i < N; ++i) { float l = (float) sig (i), r = (float) (sig (i) * .7); pc.run (l, r); if (i >= 1237 && (i - 1237) % 1201 == 0 && k < 15) worst = std::max (worst, std::abs (l - want[c][k++])); }
            CHECK (worst < 2e-6, "compressor %s: engine differs from the page by %.7f", nm[c], worst);
        }
        FxDesc a; a.type = "pcomp"; a.ch = 2; a.slot = -1; a.p = { { "amt", .5f }, { "c", 2 }, { "bpm", 120 } };
        FxDesc w; w.type = "width"; w.ch = 2; w.slot = -1; w.p = { { "w", 0 } };
        auto bank = makeFxBank ({ a, w }, sr); CHECK (bank->comp[2] && bank->width[2] && ! bank->comp[1], "compressor and width land on their strip");
        float l = .5f, r = -.5f; bank->run (2, l, r); CHECK (std::abs (l - r) < 1e-6f, "width 0 is mono");
        // Comp off is level-matched like the page: about as loud as on, not the raw part
        auto rms = [&] (bool on) { FxDesc d; d.type = "pcomp"; d.p = { { "amt", .8f }, { "c", 0 }, { "bpm", 120 } }; if (! on) d.p["on"] = 0; PcompFx pc (d, sr); double e = 0; int n = 0;
            for (int i = 0; i < 4 * 48000; ++i) { float x = (float) (.5 * std::sin (2 * synthPi * 220 * i / sr)), y = x; pc.run (x, y); if (i > 3 * 48000) { e += x * x; ++n; } } return std::sqrt (e / n); };
        const double on = rms (true), off = rms (false), dry = .5 / std::sqrt (2.0);
        CHECK (std::abs (20 * std::log10 (off / on)) < .5, "compressor bypass is level-matched: %.2f dB from on", 20 * std::log10 (off / on));
        CHECK (std::abs (20 * std::log10 (dry / on)) > 3, "the compressor really turns this down (%.1f dB)", 20 * std::log10 (dry / on));
    }

    printf (failures ? "%d synth tests FAILED\n" : "all synth tests passed\n", failures);
    return failures ? 1 : 0;
}
