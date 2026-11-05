#pragma once
// Section transitions (risers, reverse swells, impacts, downlifters, filter sweeps) and recorded takes for the native
// engine. The recipes copy the page's fxNoise and scheduleTransitions. Included by Synth.h.
#include <cstdio>

namespace st
{
struct TransEv
{
    enum Kind { noise, riser, impact, sweep, snare } kind = noise;
    double at = 0;                  // song beat
    float dur = 1, f0 = 1000, f1 = 1000, g0 = .1f, g1 = .1f, q = 1;
    int filter = 0;                 // noise: 0 band-pass, 1 high-pass, 2 low-pass
};

struct TakeAudio { std::vector<float> data; double rate = 48000; };

struct TakeSeg
{
    std::shared_ptr<const TakeAudio> audio;
    double beat = 0, end = 0, start = 0;   // song beats it covers, seconds into the file at `beat`
};

// A mono 24-bit WAV as written by TakeRecorder. Returns null for anything else.
inline std::shared_ptr<TakeAudio> readTake (const std::string& path)
{
    std::FILE* f = std::fopen (path.c_str(), "rb");
    if (f == nullptr) return nullptr;
    unsigned char h[44];
    auto out = std::make_shared<TakeAudio>();
    if (std::fread (h, 1, 44, f) != 44 || std::memcmp (h, "RIFF", 4) != 0 || std::memcmp (h + 36, "data", 4) != 0 || h[22] != 1 || h[34] != 24) { std::fclose (f); return nullptr; }
    out->rate = h[24] | h[25] << 8 | h[26] << 16 | (unsigned) h[27] << 24;
    const uint32_t bytes = h[40] | h[41] << 8 | h[42] << 16 | (uint32_t) h[43] << 24;
    std::vector<unsigned char> raw ((size_t) bytes);
    const size_t got = std::fread (raw.data(), 1, raw.size(), f);
    std::fclose (f);
    out->data.resize (got / 3);
    for (size_t i = 0; i < out->data.size(); ++i)
    {
        int32_t v = raw[i * 3] | raw[i * 3 + 1] << 8 | raw[i * 3 + 2] << 16;
        if (v & 0x800000) v -= 0x1000000;
        out->data[i] = (float) v / 8388607.0f;
    }
    return out;
}

class TransVoice
{
public:
    bool active = false;
    void start (const TransEv& e, double sampleRate)
    {
        ev = e; sr = sampleRate; n = 0; total = (int) (e.dur * sr); active = true; ph = 0; tick = 0; f.reset(); lp.reset();
        if (e.kind == TransEv::riser) lp.lowpass (3000, 1, sr);
    }
    // adds this sound; the sweep is handled by the engine's master filter
    float run (Noise& rng)
    {
        if (n >= total) { active = false; return 0; }
        const float t = (float) n / (float) std::max (1, total);
        float out = 0;
        if (ev.kind == TransEv::noise)
        {
            if (tick++ % 32 == 0)
            {
                const double hz = ev.f0 * std::pow ((double) ev.f1 / ev.f0, (double) t);
                if (ev.filter == 1) f.highpass (hz, ev.q, sr); else if (ev.filter == 2) f.lowpass (hz, ev.q, sr); else f.bandpass (hz, ev.q, sr);
            }
            out = f.run (rng.next()) * ev.g0 * std::pow (ev.g1 / ev.g0, t);
        }
        else if (ev.kind == TransEv::riser)
        {
            const double hz = ev.f0 * std::pow ((double) ev.f1 / ev.f0, (double) t);
            ph += hz / sr; if (ph >= 1) ph -= 1;
            const float tail = (float) (ev.dur - .02f) / ev.dur;
            const float g = t < tail ? .0001f * std::pow (ev.g1 / .0001f, t / tail) : ev.g1 * (1 - (t - tail) / (1 - tail));
            out = lp.run ((float) (2 * ph - 1) / 1.179f) * g;
        }
        else if (ev.kind == TransEv::impact)
        {
            const double sec = n / sr, hz = sec < 1.2 ? 72 * std::pow (34.0 / 72, sec / 1.2) : 34;
            ph += hz / sr; if (ph >= 1) ph -= 1;
            const float g = sec < .008 ? .0001f * std::pow (.7f / .0001f, (float) (sec / .008)) : .7f * std::pow (.0001f / .7f, (float) ((sec - .008) / 1.592));
            out = (float) std::sin (2 * synthPi * ph) * g;
        }
        ++n;
        return out;
    }
private:
    TransEv ev; double sr = 48000, ph = 0; int n = 0, total = 0; unsigned tick = 0; Biquad f, lp;
};
}
