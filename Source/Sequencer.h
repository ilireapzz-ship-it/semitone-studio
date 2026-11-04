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

struct Timeline
{
    std::vector<Note> notes; // sorted by start
    double length = 0;       // loop length in beats
    void sort() { std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; }); }
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
        if (t != current) { allNotesOff (out, 0); current = std::move (t); }

        if (! playing || ! current || current->length <= 0 || bpm <= 0 || sampleRate <= 0 || numSamples <= 0)
        {
            if (wasPlaying) allNotesOff (out, 0);
            wasPlaying = false;
            return;
        }

        const double beatsPerSample = bpm / 60.0 / sampleRate;
        const double ppqEnd = ppqStart + numSamples * beatsPerSample;

        // Transport jumped (loop in the host, user moved the playhead): release everything first.
        if (wasPlaying && std::abs (ppqStart - lastEnd) > 1.0e-3) allNotesOff (out, 0);
        wasPlaying = true;
        lastEnd = ppqEnd;

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
        while (a < ppqEnd && guard++ < 64)
        {
            const double base = std::floor (a / L) * L;
            const double from = a - base;
            const double to = std::min (ppqEnd - base, L);
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
            a = base + to;
            if (to >= L) a = base + L;
        }
    }

    void allNotesOff (std::vector<Event>& out, int offset)
    {
        for (auto& n : active) out.push_back ({ offset, (unsigned char) (0x80 | n.ch), (unsigned char) n.key, 0 });
        active.clear();
    }

    size_t activeCount() const { return active.size(); }

private:
    struct Active { double endAbs; int ch, key; };
    std::shared_ptr<const Timeline> pending, current;
    std::vector<Active> active;
    bool wasPlaying = false;
    double lastEnd = 0;
};
} // namespace st
