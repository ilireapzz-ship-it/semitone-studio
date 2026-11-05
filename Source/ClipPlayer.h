// Semitone Studio — plays Semitone's own instruments inside the DAW.
// The interface renders each song section to audio (a "clip"); this player puts the clips
// on the host's timeline and plays them locked to the playhead (PPQ), at any host tempo.
// Header-only and JUCE-free so it can be unit-tested anywhere.
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace st
{
struct Clip
{
    int sr = 48000;
    int frames = 0;
    std::vector<int16_t> pcm;          // interleaved stereo
    std::vector<double> mb, ms;        // tempo map: beat (from the section start) -> seconds into the clip

    // seconds into the clip at a beat measured from the section start (bpm: the host tempo, used before and after the map)
    double secondsAt (double beat, double bpm) const
    {
        const double spb = 60.0 / std::max (1.0, bpm);
        if (mb.size() < 2) return beat * spb;
        if (beat <= mb.front()) return ms.front() + (beat - mb.front()) * spb;
        if (beat >= mb.back()) return ms.back() + (beat - mb.back()) * spb;
        auto it = std::upper_bound (mb.begin(), mb.end(), beat);
        const size_t j = (size_t) (it - mb.begin());
        const double b0 = mb[j - 1], b1 = mb[j], s0 = ms[j - 1], s1 = ms[j];
        return s0 + (beat - b0) / std::max (1.0e-9, b1 - b0) * (s1 - s0);
    }
};

struct Placed
{
    std::shared_ptr<const Clip> clip;
    double at = 0;   // song beat where the section starts
};

struct Bank
{
    std::vector<Placed> items;
};

class ClipPlayer
{
public:
    void setBank (std::shared_ptr<const Bank> b) { std::atomic_store (&pending, std::move (b)); }

    // Audio thread. Adds the clips into L/R (R may equal L for mono).
    // loopLen/offset: the timeline the DAW plays (song beats offset..offset+loopLen, looped).
    void process (bool playing, double ppqStart, double bpm, double sampleRate, int n,
                  double loopLen, double offset, float* L, float* R)
    {
        current = std::atomic_load (&pending);
        if (! playing || ! current || current->items.empty() || loopLen <= 0 || bpm <= 0 || sampleRate <= 0 || n <= 0) return;
        const double bps = bpm / 60.0 / sampleRate;
        int i = 0, guard = 0;
        while (i < n && guard++ < 64)
        {
            const double beat = ppqStart + i * bps;
            double pos = std::fmod (beat, loopLen); if (pos < 0) pos += loopLen;
            const double toWrap = loopLen - pos;
            int m = (int) std::ceil (toWrap / bps - 1.0e-9);
            m = std::max (1, std::min (n - i, m));
            const double sb0 = pos + offset, sb1 = sb0 + m * bps;
            for (const auto& p : current->items)
            {
                const Clip* c = p.clip.get();
                if (! c || c->frames < 2) continue;
                if (p.at < offset - 1.0e-6 || p.at >= offset + loopLen - 1.0e-6) continue; // outside what the DAW plays
                const double p0 = c->secondsAt (sb0 - p.at, bpm) * c->sr, p1 = c->secondsAt (sb1 - p.at, bpm) * c->sr;
                if (p1 <= 0 || p0 >= c->frames - 1) continue;
                const double step = (p1 - p0) / m;
                const int16_t* d = c->pcm.data();
                constexpr float k = 1.0f / 32768.0f;
                for (int s = 0; s < m; ++s)
                {
                    const double fp = p0 + step * s;
                    if (fp < 0) continue;
                    const int idx = (int) fp;
                    if (idx >= c->frames - 1) break;
                    const float fr = (float) (fp - idx);
                    const int16_t* a = d + (size_t) idx * 2;
                    const float l = (a[0] + (a[2] - a[0]) * fr) * k, r = (a[1] + (a[3] - a[1]) * fr) * k;
                    if (R == L) L[i + s] += 0.5f * (l + r);
                    else { L[i + s] += l; if (R) R[i + s] += r; }
                }
            }
            i += m;
        }
    }

private:
    std::shared_ptr<const Bank> pending, current;
};
} // namespace st
