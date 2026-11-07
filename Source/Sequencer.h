// Semitone Studio — host-synced MIDI sequencer.
// Plays the song the web interface sends (one note list, each note on its own MIDI channel),
// locked to the host's musical position (PPQ), looping over the song length.
// Header-only and JUCE-free so it can be unit-tested anywhere.
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

namespace st
{
struct Note
{
    double start = 0, end = 0; // in quarter-note beats from the song start
    int ch = 0, key = 60, vel = 100; // MIDI channel 0-15, key 0-127, velocity 1-127
};

struct Ctl
{
    double at = 0;                       // beat
    unsigned char status = 0, d1 = 0, d2 = 0; // e.g. pitch bend 0xE0|ch, lsb, msb
};

struct Timeline
{
    std::vector<Note> notes; // sorted by start
    std::vector<Ctl> ctls;   // sorted by beat (pitch bend for melody articulations)
    double length = 0;       // loop length in beats
    int bendRange = 0;       // semitones; announced with RPN 0 when > 0
    void sort()
    {
        std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
        std::stable_sort (ctls.begin(), ctls.end(), [] (const Ctl& a, const Ctl& b) { return a.at < b.at; });
    }
};

struct Event
{
    int offset;                 // sample offset inside the block
    unsigned char status, d1, d2;
};

class Sequencer
{
public:
    Sequencer() { active.reserve (1024); }

    // Message thread: hand over a new song. The audio thread picks it up on its next block.
    void setTimeline (std::shared_ptr<const Timeline> t) { std::atomic_store (&pending, std::move (t)); }

    // Audio thread. Appends MIDI events for this block to `out` (note-offs first at equal offsets).
    void process (bool playing, double ppqStart, double bpm, double sampleRate, int numSamples, std::vector<Event>& out)
    {
        auto t = std::atomic_load (&pending);
        if (t != current) { allNotesOff (out, 0); current = std::move (t); needSetup = true; }

        if (! playing || ! current || current->length <= 0 || bpm <= 0 || sampleRate <= 0 || numSamples <= 0)
        {
            if (wasPlaying) { allNotesOff (out, 0); resetBends (out, 0); }
            wasPlaying = false;
            return;
        }

        const double beatsPerSample = bpm / 60.0 / sampleRate;
        const double ppqEnd = ppqStart + numSamples * beatsPerSample;

        // Transport jumped (loop in the host, user moved the playhead): release everything first.
        if (wasPlaying && std::abs (ppqStart - lastEnd) > 1.0e-3) { allNotesOff (out, 0); resetBends (out, 0); }
        if (! wasPlaying || needSetup) { setup (out); needSetup = false; }
        wasPlaying = true;
        lastEnd = ppqEnd;

        // Started mid-song by the app: sound the notes already held at this point, so a chord isn't missing until the next one.
        if (chaseNext.exchange (false) && current->length > 0)
        {
            const double base = std::floor (ppqStart / current->length) * current->length, pos = ppqStart - base;
            for (const auto& n : current->notes)
            {
                if (n.start >= pos) break;
                if (n.end - pos >= 0.25 && active.size() < active.capacity())
                {
                    out.push_back ({ 0, (unsigned char) (0x90 | (n.ch & 15)), (unsigned char) (n.key & 127), (unsigned char) std::clamp (n.vel, 1, 127) });
                    active.push_back ({ base + n.end, n.ch & 15, n.key & 127 });
                }
            }
        }

        auto offsetFor = [&] (double beat) {
            const int o = (int) std::floor ((beat - ppqStart) / beatsPerSample);
            return std::clamp (o, 0, numSamples - 1);
        };

        // Note-offs that fall inside this block.
        for (size_t i = 0; i < active.size();)
        {
            if (active[i].endAbs < ppqEnd)
            {
                out.push_back ({ offsetFor (active[i].endAbs), (unsigned char) (0x80 | active[i].ch), (unsigned char) active[i].key, 0 });
                active[i] = active.back();
                active.pop_back();
            }
            else ++i;
        }

        // Note-ons, wrapping around the loop as many times as the block needs.
        const double L = current->length;
        const auto& ns = current->notes;
        double a = ppqStart;
        int guard = 0;
        double k = std::floor (a / L);   // which pass of the loop; counted, not re-derived, so rounding can't send it back a pass
        while (a < ppqEnd && guard++ < 64)
        {
            const double base = k * L;
            const double from = std::max (0.0, a - base);
            const double to = std::min (ppqEnd - base, L);
            const auto& cs = current->ctls;
            auto ci = std::lower_bound (cs.begin(), cs.end(), from, [] (const Ctl& c, double v) { return c.at < v; });
            for (; ci != cs.end() && ci->at < to; ++ci)
            {
                out.push_back ({ offsetFor (base + ci->at), ci->status, ci->d1, ci->d2 });
                if ((ci->status & 0xF0) == 0xE0) bentChannels |= (1u << (ci->status & 15));
            }
            auto it = std::lower_bound (ns.begin(), ns.end(), from, [] (const Note& n, double v) { return n.start < v; });
            for (; it != ns.end() && it->start < to; ++it)
            {
                const double absStart = base + it->start;
                const double absEnd = base + std::max (it->end, it->start + 0.01);
                out.push_back ({ offsetFor (absStart), (unsigned char) (0x90 | (it->ch & 15)), (unsigned char) (it->key & 127), (unsigned char) std::clamp (it->vel, 1, 127) });
                if (absEnd < ppqEnd) // very short note: ends inside this block
                    out.push_back ({ std::max (offsetFor (absEnd), offsetFor (absStart)), (unsigned char) (0x80 | (it->ch & 15)), (unsigned char) (it->key & 127), 0 });
                else if (active.size() < active.capacity())
                    active.push_back ({ absEnd, it->ch & 15, it->key & 127 });
            }
            if (to >= L) { k += 1; a = k * L; }
            else a = ppqEnd;
        }
    }

    void allNotesOff (std::vector<Event>& out, int offset)
    {
        for (auto& n : active) out.push_back ({ offset, (unsigned char) (0x80 | n.ch), (unsigned char) n.key, 0 });
        active.clear();
    }

    size_t activeCount() const { return active.size(); }
    void chaseOnNextBlock() { chaseNext = true; }

    // centre the pitch bend on every channel that was bent
    void resetBends (std::vector<Event>& out, int offset)
    {
        for (int ch = 0; ch < 16; ++ch)
            if (bentChannels & (1u << ch)) out.push_back ({ offset, (unsigned char) (0xE0 | ch), 0, 64 });
        bentChannels = 0;
    }

private:
    // announce the pitch-bend range (RPN 0) on the channels that use bends
    void setup (std::vector<Event>& out)
    {
        if (! current || current->bendRange <= 0) return;
        unsigned chans = 0;
        for (auto& c : current->ctls) if ((c.status & 0xF0) == 0xE0) chans |= (1u << (c.status & 15));
        for (int ch = 0; ch < 16; ++ch)
        {
            if (! (chans & (1u << ch))) continue;
            const unsigned char cc = (unsigned char) (0xB0 | ch);
            out.push_back ({ 0, cc, 101, 0 });   out.push_back ({ 0, cc, 100, 0 });
            out.push_back ({ 0, cc, 6, (unsigned char) std::clamp (current->bendRange, 1, 24) }); out.push_back ({ 0, cc, 38, 0 });
            out.push_back ({ 0, cc, 101, 127 }); out.push_back ({ 0, cc, 100, 127 });
        }
    }

public:

private:
    struct Active { double endAbs; int ch, key; };
    std::shared_ptr<const Timeline> pending, current;
    std::vector<Active> active;
    bool wasPlaying = false, needSetup = true;
    std::atomic<bool> chaseNext { false };
    unsigned bentChannels = 0;
    double lastEnd = 0;
};
} // namespace st
