// Unit test for st::Sequencer (no JUCE needed): g++ -std=c++17 -I../Source sequencer_test.cpp
#include "Sequencer.h"
#include "ClipPlayer.h"
#include <cassert>
#include <cstdio>
#include <map>

int main()
{
    auto tl = std::make_shared<st::Timeline>();
    tl->length = 8; // two bars of 4/4
    tl->notes = { { 0, 1, 0, 60, 100 }, { 1, 2, 1, 64, 90 }, { 4, 8, 2, 36, 110 }, { 7.5, 7.75, 9, 38, 120 } };
    tl->sort();
    st::Sequencer seq;
    seq.setTimeline (tl);

    const double sr = 48000, bpm = 120; const int block = 512;
    std::vector<st::Event> out;
    std::map<int, int> ons, offs; int total = 0;
    double ppq = 0;
    // play 24 beats (three loops) in blocks
    while (ppq + block * bpm / 60.0 / sr <= 24)
    {
        out.clear();
        seq.process (true, ppq, bpm, sr, block, out);
        for (auto& e : out) { if ((e.status & 0xF0) == 0x90) ons[e.d1]++; else offs[e.d1]++; total++; assert (e.offset >= 0 && e.offset < block); }
        ppq += block * bpm / 60.0 / sr;
    }
    out.clear(); seq.process (false, ppq, bpm, sr, block, out); // stop: releases the held bass note
    for (auto& e : out) offs[e.d1]++;
    std::printf ("ons: 60=%d 64=%d 36=%d 38=%d | offs: 60=%d 64=%d 36=%d 38=%d | active after stop=%zu\n", ons[60], ons[64], ons[36], ons[38], offs[60], offs[64], offs[36], offs[38], seq.activeCount()); std::fflush(stdout);
    assert (ons[60] == 3 && ons[64] == 3 && ons[36] == 3 && ons[38] == 3);
    assert (offs[60] == ons[60] && offs[64] == ons[64] && offs[36] == ons[36] && offs[38] == ons[38]);
    assert (seq.activeCount() == 0);

    // jump in the middle of a held note: must release it
    out.clear(); seq.process (true, 4.0, bpm, sr, block, out);
    out.clear(); seq.process (true, 0.0, bpm, sr, block, out); // user jumped back
    int rel = 0; for (auto& e : out) if ((e.status & 0xF0) == 0x80 && e.d1 == 36) rel++;
    assert (rel == 1);
    std::printf ("jump release ok; all tests passed\n");
    
    // pitch bends play inside their block, RPN range is announced first, and bends reset on stop
    {
        auto tl = std::make_shared<st::Timeline>();
        tl->length = 4; tl->bendRange = 12;
        tl->notes.push_back ({ 0.0, 1.0, 0, 60, 100 });
        tl->ctls.push_back ({ 0.5, 0xE0, 0, 80 });
        tl->sort();
        st::Sequencer s; s.setTimeline (tl);
        std::vector<st::Event> out;
        s.process (true, 0.0, 120.0, 48000.0, 48000, out); // 2 beats
        int rpn = 0, bends = 0; for (auto& e : out) { if ((e.status & 0xF0) == 0xB0 && e.d1 == 6 && e.d2 == 12) ++rpn; if ((e.status & 0xF0) == 0xE0) ++bends; }
        if (rpn != 1 || bends != 1) { std::printf ("FAIL bends rpn=%d bends=%d\n", rpn, bends); return 1; }
        out.clear(); s.process (false, 2.0, 120.0, 48000.0, 512, out);
        bool reset = false; for (auto& e : out) if (e.status == 0xE0 && e.d2 == 64) reset = true;
        if (! reset) { std::printf ("FAIL bend reset\n"); return 1; }
        std::printf ("ok: pitch bends\n");
    }
    // started mid-song: a held note sounds straight away and ends on time; short notes and finished ones don't
    {
        auto t = std::make_shared<st::Timeline>(); t->length = 16;
        t->notes.push_back ({ 0, 8, 1, 60, 90 }); t->notes.push_back ({ 3, 3.1, 9, 36, 100 }); t->notes.push_back ({ 0, 1, 2, 40, 90 }); t->sort();
        st::Sequencer s; s.setTimeline (t); s.chaseOnNextBlock();
        std::vector<st::Event> e; s.process (true, 3.0 + .001, 120.0, 48000.0, 512, e);
        int ons = 0; bool chord = false; for (auto& x : e) if ((x.status & 0xF0) == 0x90) { ++ons; chord = chord || (x.d1 == 60 && x.offset == 0 && x.d2 == 90); }
        if (ons != 1 || ! chord || s.activeCount() != 1) { std::printf ("FAIL chase ons=%d active=%d\n", ons, (int) s.activeCount()); return 1; }
        e.clear(); s.process (true, 7.9995, 120.0, 48000.0, 512, e); bool off = false; for (auto& x : e) off = off || (x.status & 0xF0) == 0x80;
        if (! off) { std::printf ("FAIL chase end\n"); return 1; }
        std::printf ("ok: held notes sound when starting mid-song\n");
    }
    // the same song sent again while playing (a volume moved): nothing is released; a changed song: released, then held notes sound again
    {
        auto mk = [] (int key) { auto t = std::make_shared<st::Timeline>(); t->length = 16; t->notes.push_back ({ 0, 8, 1, key, 90 }); t->sort(); return t; };
        st::Sequencer s; s.setTimeline (mk (60)); std::vector<st::Event> e; s.process (true, 0.0, 120.0, 48000.0, 512, e);
        e.clear(); s.setTimeline (mk (60)); s.process (true, 512 * 2.0 / 48000.0, 120.0, 48000.0, 512, e);
        int offs = 0; for (auto& x : e) if ((x.status & 0xF0) == 0x80) ++offs;
        if (offs != 0 || s.activeCount() != 1) { std::printf ("FAIL same song resent: offs=%d active=%d\n", offs, (int) s.activeCount()); return 1; }
        e.clear(); s.setTimeline (mk (64)); s.process (true, 1024 * 2.0 / 48000.0, 120.0, 48000.0, 512, e);
        int off60 = 0, on64 = 0; for (auto& x : e) { if ((x.status & 0xF0) == 0x80 && x.d1 == 60) ++off60; if ((x.status & 0xF0) == 0x90 && x.d1 == 64) ++on64; }
        if (off60 != 1 || on64 != 1) { std::printf ("FAIL changed song: off60=%d on64=%d\n", off60, on64); return 1; }
        std::printf ("ok: resending the same song keeps notes; a changed one re-sounds held notes\n");
    }
    // clips play locked to PPQ: section at beat 4, clip has a click at beat 0 of the section (0.1 s pre-roll)
    {
        auto c = std::make_shared<st::Clip>();
        c->sr = 48000; c->frames = 48000 * 4; c->pcm.assign ((size_t) c->frames * 2, 0);
        const int clickAt = (int) (0.1 * 48000); c->pcm[(size_t) clickAt * 2] = 16000; c->pcm[(size_t) clickAt * 2 + 1] = 16000;
        c->mb = { 0.0, 4.0 }; c->ms = { 0.1, 2.1 };               // 4 beats at 120 bpm
        auto b = std::make_shared<st::Bank>(); b->items.push_back ({ c, 4.0 });
        st::ClipPlayer pl; pl.setBank (b);
        // host at 120 bpm: beat 4 is 2 s in = sample 96000. Render 3 s in one go, song looped every 16 beats.
        std::vector<float> L (144000, 0.0f), R (144000, 0.0f);
        pl.process (true, 0.0, 120.0, 48000.0, 144000, 16.0, 0.0, L.data(), R.data());
        int peak = -1; float mx = 0; for (int i = 0; i < 144000; ++i) if (L[(size_t) i] > mx) { mx = L[(size_t) i]; peak = i; }
        if (std::abs (peak - 96000) > 2 || mx < 0.2f) { std::printf ("FAIL clip lock peak=%d mx=%f\n", peak, mx); return 1; }
        // host at 60 bpm: beat 4 is 4 s in, still on the click (varispeed)
        std::vector<float> L2 (240000, 0.0f);
        for (int o = 0; o < 240000; o += 480) pl.process (true, o * (1.0 / 48000.0), 60.0, 48000.0, 480, 16.0, 0.0, L2.data() + o, L2.data() + o);
        peak = -1; mx = 0; for (int i = 0; i < 240000; ++i) if (L2[(size_t) i] > mx) { mx = L2[(size_t) i]; peak = i; }
        if (std::abs (peak - 192000) > 3) { std::printf ("FAIL clip tempo peak=%d\n", peak); return 1; }
        // looped section mode (offset 4, length 4): the click lands at the start of every loop
        std::vector<float> L3 (96000 * 2, 0.0f);
        pl.process (true, 0.0, 120.0, 48000.0, 96000 * 2, 4.0, 4.0, L3.data(), nullptr);
        int hits = 0; for (int i = 0; i < 96000 * 2; ++i) if (L3[(size_t) i] > 0.2f) ++hits;
        if (hits != 2) { std::printf ("FAIL clip loop hits=%d\n", hits); return 1; }
        std::printf ("ok: clips locked to the playhead\n");
    }
    return 0;
}
