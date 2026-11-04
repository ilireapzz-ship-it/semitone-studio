// Unit test for st::Sequencer (no JUCE needed): g++ -std=c++17 -I../Source sequencer_test.cpp
#include "Sequencer.h"
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
    return 0;
}
