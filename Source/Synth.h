// Semitone Studio — the native sound engine.
// Makes Semitone's own instruments inside the plugin, live, from the same note events the MIDI
// output uses, so the DAW can play the song without the web page (window closed, nothing rendered yet).
// The recipes are ported from the web version: voice(), drumHit(), fmEP(), anaSynth(), renderPianoNote(),
// renderPluck(), newMaster() and buildPolish() in web/semitone-studio.html. The page stays the source of
// truth for the patches: it sends their settings with the timeline (see nativeSoundInfo() there).
// Not ported yet: per-part effects, mix automation, transitions, recorded takes, samples and SoundFonts.
// Sections rendered by the page (ClipPlayer) still carry those.
// Header-only and JUCE-free so it can be unit-tested anywhere.
#pragma once
#include <cstring>
#include <string>
#include "Sequencer.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace st
{
constexpr double synthPi = 3.14159265358979323846;

enum class Wave { sine, triangle, saw, square, pulse };
enum class Kind { voice, fm, buffer, juno, prophet };
enum class Pluck { nylon, steel, harp, pizz, bassgtr, upright };

struct Layer { float ratio = 1, gain = 0; Wave wave = Wave::sine; };

// Everything one part needs. Plain data: a voice copies it at note-on, so a new patch never disturbs held notes.
struct PartPatch
{
    int ch = -1;
    Kind kind = Kind::voice;
    float vol = .2f;            // voice(): the patch volume. Other kinds: the role volume.
    float velRef = 92;          // the MIDI velocity that means "as written" for this part
    float level = 1, pan = 0;   // mixer strip
    bool stereo = false;        // this part's bus is stereo in Web Audio (see panGains)
    // voice()
    Wave wave = Wave::triangle;
    float attack = .01f, release = .3f, cutoff = 4000, decay = 0, unison = 0, fenv = 0, vib = 0, vibRate = 5.6f;
    std::array<Layer, 6> layers {};
    int numLayers = 0;
    // sampled-model notes: instr 0 = piano, 1 + Pluck = the plucked models
    int instr = -1;
    float bufRelease = .22f, holdMin = 0, holdMax = 0;
    bool velCutoff = false;
    // anaSynth()
    float saw = 0, pulse = 0, pw = .5f, pwm = 0, sub = 0, noise = 0;
    Wave aWave = Wave::saw, bWave = Wave::triangle;
    float aGain = 0, aPw = .5f, bGain = 0, bPw = .5f, det = 0, bMul = 1, pm = 0;
    float cut = 1000, res = .1f, env = 1, kt = .5f, drift = .3f, gain = 1, scoop = 0, hpHz = 40;
    int chorus = 0;             // 0 none, 1 = I, 2 = II, 3 = I+II
    std::array<float, 4> fe { .01f, .3f, .5f, .3f }, ae { .01f, .3f, .8f, .3f }, lfo { 0, 0, 0, 0 };
};

struct KitPatch
{
    float lp = 16000;
    std::array<float, 4> kick { 160, 42, .6f, 0 }, snare { 185, .5f, .2f, 1800 };
    std::array<float, 2> hat { .045f, 8000 }, open { .3f, 7000 }, rim { 1700, .035f }, crash { 1.3f, 5000 };
    float clap = .22f;
};

struct PolishPatch
{
    float shelfLo = 0, shelfHi = 0;
    bool on = false;            // false: the safety compressor only
    float hp = 28, thr = -18, ratio = 2, att = .03f, rel = .25f, make = 1, lim = -2, drive = .8f;
};

using NoteBuffer = std::vector<float>;
using NoteBuffers = std::array<const NoteBuffer*, 128>;

// One automation lane: value at song beat, straight lines between points, held at the ends.
// Volume is a multiplier on the part's level; pan replaces the part's pan. ch -1 = the master bus.
struct Lane
{
    int ch = -1;
    bool pan = false;
    int fxSlot = -1, fxParam = -1;     // set: this lane drives an effect's parameter, with absolute values
    std::string fxName;
    std::vector<std::pair<double, float>> pts;
    // a macro that FL Studio automates: the host value (0-1) looks up this target's value in `tab` (17 steps) instead of `pts`
    int macro = -1;
    std::array<float, 17> tab {};
    float fromMacro (float u) const
    {
        const float x = juce_clamp01 (u) * 16.0f; const int i = (int) x; if (i >= 16) return tab[16];
        return tab[(size_t) i] + (tab[(size_t) i + 1] - tab[(size_t) i]) * (x - (float) i);
    }
    static float juce_clamp01 (float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

    float at (double beat) const
    {
        if (pts.empty()) return pan ? 0.0f : 1.0f;
        if (beat <= pts.front().first) return pts.front().second;
        if (beat >= pts.back().first) return pts.back().second;
        for (size_t i = 1; i < pts.size(); ++i)
            if (beat <= pts[i].first)
            {
                const auto& a = pts[i - 1]; const auto& b = pts[i];
                const double d = b.first - a.first;
                return d <= 0 ? b.second : a.second + (b.second - a.second) * (float) ((beat - a.first) / d);
            }
        return pts.back().second;
    }
};

// Sidechain: a channel dips at each hit of a drum row, then comes back. Worked out from the song position alone,
// so seeking and looping need no state. Same shape as duckSchedule() in the page.
struct Duck
{
    int ch = 0;
    float depth = .6f, release = .2f;
    std::vector<double> beats;         // sorted

    static constexpr double attack = .005;

    float gainAt (double beat, double secPerBeat) const
    {
        auto it = std::upper_bound (beats.begin(), beats.end(), beat);
        if (it == beats.begin() || secPerBeat <= 0) return 1;
        const double hit = *(it - 1), tau = std::max (.005, (double) release / 3), dt = (beat - hit) * secPerBeat;
        double env;
        if (dt >= attack) env = std::exp (-(dt - attack) / tau);
        else
        {
            // still falling from an earlier hit: start from where that one had got to
            double was = 0;
            if (it - 1 != beats.begin())
            {
                const double gap = (hit - *(it - 2)) * secPerBeat;
                was = gap < attack ? 1 : std::exp (-(gap - attack) / tau);
            }
            env = was + (1 - was) * dt / attack;
        }
        return (float) (1 - depth * env);
    }
};

struct FxDesc;
class FxBank;
struct TransEv;
struct TakeSeg;
struct SynthPatch
{
    std::shared_ptr<std::vector<TransEv>> trans;   // sorted by beat
    std::shared_ptr<std::vector<TakeSeg>> takes;
    std::vector<std::string> takeFiles;
    std::array<float, 16> takeChLevel = [] { std::array<float, 16> v; v.fill (1); return v; }(), takeChPan {};   // each audio track's fader and pan, by channel
    uint32_t monoMask = 0;                                 // channels summed to mono before their pan (one bit each); bit 16: the whole mix
    float transLevel = 1, transPan = 0;
    int transCh = -1;                                      // the transitions' own channel (fader, pan, effects, EQ, comp, meter); -1: straight to the mix
    std::array<float, 16> extLevel {}, extPan {};    // your plug-ins' strips, by channel
    bool hasExt = false;
    std::vector<PartPatch> parts;
    std::vector<Lane> lanes;
    std::vector<Duck> ducks;
    // routing (the page's buses and returns): bus b is FX channel 17 + b; each part channel goes to a bus or the master and sends to returns
    struct RouteBus { float level = 1, pan = 0; bool mono = false; };
    std::vector<RouteBus> rbus;
    std::array<int8_t, 16> rOut = [] { std::array<int8_t, 16> v; v.fill (-1); return v; }();
    std::array<std::array<float, 8>, 16> rSend {};
    std::array<uint8_t, 16> rPre {};
    uint32_t routeMask = 0;
    std::shared_ptr<std::vector<FxDesc>> fxList;   // what the page asked for
    std::shared_ptr<FxBank> fx;                    // built off the audio thread by the caller; running state lives here
    std::vector<NoteBuffers> buffers;   // one per part (the buffers are owned by the caller's cache)
    int drumCh = -1;
    bool drumExt = false;    // a plug-in plays the drum notes, so the built-in kit stays quiet
    float drumLevel = 1, drumPan = 0;
    KitPatch kit;
    float bendRange = 12;
    float roomMix = 0, roomT60 = 1;
    PolishPatch polish;
};

// ---- small building blocks ----
struct Noise
{
    uint32_t s = 0x9E3779B9u;
    float next()
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) ((double) s / 2147483648.0 - 1.0);
    }
};

// The same filters as Web Audio's BiquadFilterNode (lowpass and highpass Q are in dB, bandpass Q is linear).
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] { 0, 0 }, z2[2] { 0, 0 };

    void set (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0); a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
    void lowpass (double f, double qDb, double sr)
    {
        const double w = 2 * synthPi * std::min (f, sr * .49) / sr, c = std::cos (w), al = std::sin (w) / (2 * std::pow (10.0, qDb / 20));
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double f, double qDb, double sr)
    {
        const double w = 2 * synthPi * std::min (f, sr * .49) / sr, c = std::cos (w), al = std::sin (w) / (2 * std::pow (10.0, qDb / 20));
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void bandpass (double f, double q, double sr)
    {
        const double w = 2 * synthPi * std::min (f, sr * .49) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }
    float run (float x, int c = 0)
    {
        const float y = b0 * x + z1[c];
        z1[c] = b1 * x - a1 * y + z2[c];
        z2[c] = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
};

inline float polyBlep (double t, double dt)
{
    if (t < dt) { t /= dt; return (float) (t + t - t * t - 1); }
    if (t > 1 - dt) { t = (t - 1) / dt; return (float) (t * t + t + t + 1); }
    return 0;
}

// Browsers scale their saw, square and pulse waves down so the ripple at the edges (Gibbs overshoot) peaks at 1.
constexpr float edgyWaveScale = 1 / 1.179f;

struct Osc
{
    double ph = 0;
    float tick (Wave w, double dt, float width = .5f)
    {
        dt = std::clamp (dt, -.49, .49);                 // heavy FM drives the frequency below zero
        const double step = std::max (std::abs (dt), 1.0e-9);
        float o = 0;
        switch (w)
        {
            case Wave::sine:     o = (float) std::sin (2 * synthPi * ph); break;
            case Wave::triangle: o = (float) (ph < .25 ? 4 * ph : ph < .75 ? 2 - 4 * ph : 4 * ph - 4); break;
            case Wave::saw:      // as in Web Audio: starts at 0 rising, jumps half way through the cycle
            {
                double p = ph + .5; if (p >= 1) p -= 1;
                o = ((float) (2 * p - 1) - polyBlep (p, step)) * edgyWaveScale;
                break;
            }
            case Wave::square: case Wave::pulse:
            {
                const double pw = w == Wave::square ? .5 : std::clamp ((double) width, .02, .98);
                double p2 = ph + 1 - pw; if (p2 >= 1) p2 -= 1;
                o = ((ph < pw ? 1.0f : -1.0f) + polyBlep (ph, step) - polyBlep (p2, step)) * edgyWaveScale;
                break;
            }
        }
        ph += dt;
        if (ph >= 1) ph -= 1; else if (ph < 0) ph += 1;
        return o;
    }
};

// Attack, then decay to sustain, then release: anaSynth's adsr(), value running between base and peak.
struct Adsr
{
    float value = 0, base = 0, peak = 1, sus = 0, inc = 0, kD = 0, kR = 0;
    int attackLeft = 0; bool rel = false;
    void start (double sr, float a, float d, float s, float r, float pk, float bs)
    {
        base = bs; peak = pk; value = bs; sus = bs + (pk - bs) * s; rel = false;
        attackLeft = std::max (1, (int) (std::max (.001f, a) * sr));
        inc = (pk - bs) / (float) attackLeft;
        kD = (float) (1 - std::exp (-1 / (std::max (.005f, d / 3) * sr)));
        kR = (float) (1 - std::exp (-1 / (std::max (.005f, r / 3) * sr)));
    }
    float tick()
    {
        if (attackLeft > 0 && ! rel) { value += inc; --attackLeft; }
        else value += ((rel ? base : sus) - value) * (rel ? kR : kD);
        return value;
    }
};

// Equal-power panning as Web Audio's StereoPannerNode: a mono bus sits 3 dB down in each ear when centred,
// a stereo bus passes through unchanged.
inline void panGains (float pan, bool stereo, float& l, float& r)
{
    const double p = std::clamp ((double) pan, -1.0, 1.0);
    if (stereo)
    {
        const double x = (p <= 0 ? p + 1 : p) * synthPi / 2;
        l = p <= 0 ? 1.0f : (float) std::cos (x);
        r = p <= 0 ? (float) std::sin (x) : 1.0f;
    }
    else
    {
        const double x = (p + 1) * synthPi / 4;
        l = (float) std::cos (x); r = (float) std::sin (x);
    }
}

// ---- pre-rendered instruments (each note is rendered once, away from the audio thread) ----
inline void normaliseNote (NoteBuffer& d)
{
    float pk = 0;
    for (float v : d) pk = std::max (pk, std::abs (v));
    if (pk > 0) for (float& v : d) v *= .9f / pk;
}

inline NoteBuffer renderPianoNote (int m, double sr)
{
    const double f0 = 440 * std::pow (2.0, (m - 69) / 12.0);
    const int len = (int) (sr * std::min (5.0, std::max (1.3, 7 - (m - 21) * .07)));
    NoteBuffer d ((size_t) len, 0.0f);
    const double B = .00007 * std::pow (2.0, (m - 48) / 14.0), base = std::max (.45, 4.5 - (m - 21) * .055);
    const int N = std::min (22, (int) (sr * .45 / f0));
    const bool two = m > 40;
    for (int n = 1; n <= N; ++n)
    {
        const double fn = f0 * n * std::sqrt (1 + B * n * n);
        if (fn > sr * .45) break;
        const double amp = std::exp (-n * .09) / std::pow ((double) n, 1.05), t60 = base / (1 + n * .2);
        const double k1 = std::exp (-6.91 / (t60 * sr)), k0 = std::exp (-6.91 / (t60 * .18 * sr));
        for (int dd = 0; dd < (two ? 2 : 1); ++dd)
        {
            const double det = two ? (dd == 0 ? -.5 : .5) : 0;
            const double w = 2 * synthPi * fn * std::pow (2.0, det / 1200) / sr, c = std::cos (w), s = std::sin (w);
            double x = 1, y = 0, e1 = amp * .55, e0 = amp * .45;
            for (int i = 0; i < len; ++i)
            {
                const double nx = x * c - y * s;
                y = x * s + y * c; x = nx;
                d[(size_t) i] += (float) (y * (e0 + e1));
                e0 *= k0; e1 *= k1;
            }
        }
    }
    Noise rng; rng.s ^= (uint32_t) m * 2654435761u;
    double lp = 0; const int hl = (int) (sr * .01);
    for (int i = 0; i < hl && i < len; ++i) { lp = lp * .55 + rng.next() * .45; d[(size_t) i] += (float) (lp * .18 * (1.0 - (double) i / hl)); }
    const int fl = (int) (sr * .004);
    for (int i = 0; i < fl && i < len; ++i) d[(size_t) i] *= (float) i / (float) fl;
    normaliseNote (d);
    return d;
}

inline NoteBuffer renderPluckNote (int m, double sr, Pluck kind)
{
    static constexpr double t60s[] { 2.2, 3.2, 3.4, .45, 2.4, 1.2 }, brights[] { .4, .8, .55, .35, .28, .2 };
    const double f0 = 440 * std::pow (2.0, (m - 69) / 12.0), per = sr / f0;
    // the loop's two-point average reads one sample newer, so it delays -0.5: N - 0.5 + frac = one period
    const double D = per + .5;
    const int N = std::max (2, (int) std::floor (D - .2));
    const double frac = D - N, C = (1 - frac) / (1 + frac);
    const double T60 = t60s[(int) kind] * std::pow (220 / f0, .35), rho = std::pow (10.0, -3 / (T60 * f0)), bright = brights[(int) kind];
    const int len = (int) (sr * std::min (5.0, T60 * 1.15 + .1));
    NoteBuffer d ((size_t) len, 0.0f);
    std::vector<double> buf ((size_t) N, 0.0);
    Noise rng; rng.s ^= (uint32_t) (m + 1000 * ((int) kind + 1)) * 2654435761u;
    double lp = 0;
    for (auto& b : buf) { lp += (rng.next() - lp) * bright; b = lp; }
    const int pick = std::max (1, (int) (N * (kind == Pluck::steel ? .13 : .2)));
    for (int i = N - 1; i >= pick; --i) buf[(size_t) i] -= buf[(size_t) (i - pick)] * .6;
    int idx = 0; double x1 = 0, y1 = 0;
    for (int i = 0; i < len; ++i)
    {
        const double a = buf[(size_t) idx], b = buf[(size_t) ((idx + 1) % N)];
        d[(size_t) i] = (float) a;
        const double v = rho * .5 * (a + b), y = C * v + x1 - C * y1;
        x1 = v; y1 = y; buf[(size_t) idx] = y;
        idx = (idx + 1) % N;
    }
    normaliseNote (d);
    return d;
}

// ---- one sounding note or drum hit ----
struct DrumLayer
{
    bool noise = false;
    Wave wave = Wave::sine;
    Biquad f;
    Osc osc;
    double hz = 0, hzK = 1; int rampLeft = 0;
    int delay = 0, attackLeft = 0, left = 0;
    float amp = 0, inc = 0, k = 1;
};

class Voice
{
public:
    bool active = false, released = false, isDrum = false;
    int ch = 0, key = 0;
    uint32_t order = 0;
    PartPatch p;
    float outL = 0, outR = 0;

    void startNote (const PartPatch& patch, int k, float mul, double sampleRate, const NoteBuffer* buf, Noise& rng)
    {
        p = patch; sr = sampleRate; key = k; ch = p.ch; active = true; released = false; isDrum = false;
        age = 0; hz = 440 * std::pow (2.0, (k - 69) / 12.0); bend = 1; ctl = 0;
        fL.reset(); fR.reset(); f2.reset(); hp.reset();
        for (auto& o : osc) o.ph = 0;
        lfoPh = 0; lfoPh2 = 0; noise = rng; noise.s ^= (uint32_t) k * 2654435761u; if (noise.s == 0) noise.s = 1;
        const double dt = 1 / sr;
        if (p.kind == Kind::voice)
        {
            threeOsc = p.unison > .02f;
            const float v = p.vol * mul / (threeOsc ? 1.7f : 1.0f);
            level = 0; peak = v; attackLeft = std::max (1, (int) (p.attack * sr)); inc = v / (float) attackLeft;
            kDecay = p.decay > 0 ? (float) (1 - std::exp (-dt / p.decay)) : 0;
            kRelease = (float) (1 - std::exp (-dt / std::max (.0025f, p.release / 4)));
            cutNow = p.cutoff; qDb = 1;
            if (p.fenv > 0)
            {
                qDb = 1 + p.fenv * 3;
                cutNow = std::min (16000.0f, p.cutoff * (1 + p.fenv * 7));
                kCut = (float) (1 - std::exp (-dt / (.05f + .25f * (1 - p.fenv))));
            }
            for (int i = 0; i < 3; ++i)
            {
                detune[(size_t) i] = threeOsc ? std::pow (2.0, (i - 1) * p.unison * 16 / 1200.0) : 1.0;
                float pl = 1, pr = 1;
                if (threeOsc) panGains ((i - 1) * .6f * std::min (1.0f, p.unison * 1.5f), false, pl, pr);
                oscL[(size_t) i] = pl; oscR[(size_t) i] = pr;
            }
            setFilterFromCut();
        }
        else if (p.kind == Kind::fm)
        {
            const float vol = p.vol * mul;
            level = 0; peak = vol; attackLeft = std::max (1, (int) (.004 * sr)); inc = vol / (float) attackLeft;
            kDecay = (float) (1 - std::exp (-dt / 1.1));
            kRelease = (float) (1 - std::exp (-dt / .14));
            fmIdx = hz * 2.4; fmIdxEnd = std::max (1.0, hz * .25); fmIdxK = std::pow (fmIdxEnd / fmIdx, dt / .7);
            tine = vol * .12f; tineK = (float) std::pow (.0001 / std::max (1.0e-6, (double) tine), dt / .1);
        }
        else if (p.kind == Kind::buffer)
        {
            sample = buf; pos = 0;
            level = p.vol * mul;
            kRelease = (float) (1 - std::exp (-dt / std::max (.003f, p.bufRelease / 3)));
            fL.lowpass (p.velCutoff ? 2200 + level * 30000 : 18000, 1, sr);
            hold = (int) (p.holdMin * sr); maxHold = p.holdMax > 0 ? (int) (p.holdMax * sr) : 0;
            if (sample == nullptr) active = false;
        }
        else
        {
            const float vol = p.vol * mul, vel = std::clamp (vol / .25f, .3f, 1.3f);
            const float driftCents = p.drift * (noise.next() * .5f) * 6;
            hz *= std::pow (2.0, driftCents / 1200.0);
            const float kt = (float) std::pow (hz / 261.6, p.kt);
            baseCut = std::min (16000.0f, p.cut * kt * (.7f + .3f * vel) * (1 + p.drift * noise.next() * .04f));
            const float top = std::min (18000.0f, baseCut * (float) std::pow (2.0, p.env * (.6f + .4f * vel)));
            cutEnv.start (sr, p.fe[0], p.fe[1], p.fe[2], p.fe[3], top, baseCut);
            ampEnv.start (sr, p.ae[0], p.ae[1], p.ae[2], p.ae[3], vol * .55f * p.gain, 0);
            hp.highpass (p.hpHz, 1, sr);
            pmNow = hz * p.pm; pmEnd = pmNow * .08; pmK = 1 - std::exp (-dt / std::max (.002, p.fe[1] / 2.0));
            scoopLeft = p.scoop > 0 ? (int) (.08 * sr) : 0; scoopRatio = std::pow (2.0, -p.scoop / 12.0);
            scoopK = scoopLeft > 0 ? std::pow (1 / scoopRatio, 1.0 / scoopLeft) : 1;
            setAnaFilter (baseCut);
        }
    }

    void startDrum (const KitPatch& K, int k, float v, int channel, float lvl, float pan, double sampleRate, Noise& rng)
    {
        sr = sampleRate; key = k; ch = channel; active = true; released = false; isDrum = true; numDrum = 0; age = 0;
        p.level = lvl; p.pan = pan; p.stereo = false;
        noise = rng; noise.s ^= (uint32_t) k * 2654435761u; if (noise.s == 0) noise.s = 1;
        fL.reset(); fL.lowpass (K.lp, 1, sr);
        const auto nz = [&] (float d, float f, float a, int type, float at, float q) {
            auto& l = addLayer (d, a, at);
            l.noise = true;
            if (type == 0) l.f.highpass (f, q, sr); else l.f.bandpass (f, q, sr);
        };
        const auto tn = [&] (float f0, float f1, float d, float a, Wave w) {
            auto& l = addLayer (d, a, 0);
            l.noise = false; l.wave = w; l.hz = f0; l.hzK = 1; l.rampLeft = 0;
            if (f1 > 0 && f1 != f0) { l.rampLeft = std::max (1, (int) (d * .35 * sr)); l.hzK = std::pow ((double) f1 / f0, 1.0 / l.rampLeft); }
        };
        if (k == 36) { tn (K.kick[0], K.kick[1], K.kick[2], .95f * v, Wave::sine); if (K.kick[3] > 0) nz (.012f, 3000, .25f * K.kick[3] * v, 0, 0, .7f); }
        else if (k == 38) { const float f = K.snare[0]; tn (f, f * .8f, K.snare[2] * .6f, .35f * v, Wave::triangle); nz (K.snare[2], K.snare[3], .5f * K.snare[1] * v, 0, 0, .7f); }
        else if (k == 39) { for (float o : { 0.0f, .011f, .022f }) nz (.02f, 1200, .45f * v, 1, o, 1.2f); nz (K.clap, 1200, .3f * v, 1, .03f, 1); }
        else if (k == 42) nz (K.hat[0], K.hat[1], .28f * v, 0, 0, .7f);
        else if (k == 46) nz (K.open[0], K.open[1], .22f * v, 0, 0, .7f);
        else if (k == 37) { tn (K.rim[0], K.rim[0], K.rim[1], .3f * v, Wave::square); nz (.01f, 2000, .15f * v, 0, 0, .7f); }
        else if (k == 49) nz (K.crash[0], K.crash[1], .22f * v, 0, 0, .7f);
        else active = false;
    }

    void noteOff() { released = true; }
    void kill() { active = false; }

    // One sample into outL/outR (before the mixer strip). bendNow: the channel's pitch-bend ratio.
    void tick (double bendNow)
    {
        if ((ctl++ & 15) == 0) control (bendNow);
        ++age;
        switch (isDrum ? -1 : (int) p.kind)
        {
            case -1:                  tickDrum(); break;
            case (int) Kind::voice:   tickVoice(); break;
            case (int) Kind::fm:      tickFm(); break;
            case (int) Kind::buffer:  tickBuffer(); break;
            default:                  tickAna(); break;
        }
    }

private:
    double sr = 48000, hz = 440, bend = 1;
    uint32_t ctl = 0;
    int age = 0;
    Biquad fL, fR, f2, hp;
    std::array<Osc, 12> osc {};
    Noise noise;
    double lfoPh = 0, lfoPh2 = 0;
    // voice / fm
    bool threeOsc = false;
    float level = 0, peak = 0, inc = 0, kDecay = 0, kRelease = 0, cutNow = 0, kCut = 0, qDb = 1;
    int attackLeft = 0;
    std::array<double, 3> detune {};
    std::array<float, 3> oscL {}, oscR {};
    double vibRatio = 1, fmIdx = 0, fmIdxEnd = 0, fmIdxK = 1;
    float tine = 0, tineK = 1;
    // buffer
    const NoteBuffer* sample = nullptr;
    double pos = 0;
    int hold = 0, maxHold = 0;
    // ana
    Adsr cutEnv, ampEnv;
    float baseCut = 1000;
    double pmNow = 0, pmEnd = 0, pmK = 0, scoopRatio = 1, scoopK = 1;
    int scoopLeft = 0;
    // drums
    std::array<DrumLayer, 5> drum {};
    int numDrum = 0;

    void setFilterFromCut()
    {
        fL.lowpass (cutNow, qDb, sr);
        fR.b0 = fL.b0; fR.b1 = fL.b1; fR.b2 = fL.b2; fR.a1 = fL.a1; fR.a2 = fL.a2;
    }
    void setAnaFilter (float cutHz)
    {
        fL.lowpass (cutHz, p.res * 16, sr);
        f2.lowpass (cutHz, p.res * 3, sr);
    }

    void control (double bendNow)
    {
        bend = bendNow;
        vibRatio = 1;
        const double sec = age / sr;
        if (! isDrum && p.kind == Kind::voice)
        {
            if (p.vib > 0 && sec > .28)
            {
                const double depth = (std::pow (2.0, p.vib / 12.0) - 1) * std::min (1.0, (sec - .28) / .3);
                vibRatio = 1 + depth * std::sin (2 * synthPi * lfoPh);
            }
            lfoPh += 16 * p.vibRate / sr; if (lfoPh >= 1) lfoPh -= 1;
            if (p.fenv > 0 && sec > p.attack)
            {
                cutNow += (p.cutoff - cutNow) * (1 - std::pow (1 - kCut, 16.0f));
                setFilterFromCut();
            }
        }
        else if (! isDrum && (p.kind == Kind::juno || p.kind == Kind::prophet))
        {
            float cutHz = cutEnv.value;
            if (p.lfo[0] > 0)
            {
                const double s = std::sin (2 * synthPi * lfoPh);
                if (p.lfo[2] > 0) cutHz += baseCut * p.lfo[2] * (float) s;
                if (p.lfo[3] > 0) vibRatio = 1 + (std::pow (2.0, p.lfo[3] * .5 / 12.0) - 1) * std::min (1.0, sec / .35) * s;
                lfoPh += 16 * p.lfo[0] / sr; if (lfoPh >= 1) lfoPh -= 1;
            }
            setAnaFilter (std::clamp (cutHz, 20.0f, 20000.0f));
        }
    }

    void tickVoice()
    {
        if (attackLeft > 0) { level += inc; --attackLeft; }
        else if (released) level -= level * kRelease;
        else if (kDecay > 0) level += (peak * .25f - level) * kDecay;
        if (released && attackLeft <= 0 && level < 1.0e-4f) { active = false; outL = outR = 0; return; }

        const double f = hz * bend * vibRatio / sr;
        float l = 0, r = 0;
        const int n = threeOsc ? 3 : 1;
        for (int i = 0; i < n; ++i)
        {
            const float o = osc[(size_t) i].tick (p.wave, f * detune[(size_t) i]);
            l += o * oscL[(size_t) i]; r += o * oscR[(size_t) i];
        }
        for (int i = 0; i < p.numLayers; ++i)
        {
            const auto& ly = p.layers[(size_t) i];
            const float o = osc[(size_t) (3 + i)].tick (ly.wave, f * ly.ratio) * ly.gain;
            l += o; r += o;
        }
        if (threeOsc) { outL = fL.run (l) * level; outR = fR.run (r) * level; }
        else outL = outR = fL.run (l) * level;
    }

    void tickFm()
    {
        if (attackLeft > 0) { level += inc; --attackLeft; }
        else if (released) level -= level * kRelease;
        else level += (peak * .3f - level) * kDecay;
        if (released && attackLeft <= 0 && level < 1.0e-4f) { active = false; outL = outR = 0; return; }

        const double f = hz * bend;
        const float m = osc[1].tick (Wave::sine, f / sr);
        if (fmIdx > fmIdxEnd) fmIdx *= fmIdxK;
        const float c = osc[0].tick (Wave::sine, (f + fmIdx * m) / sr);
        const float t = osc[2].tick (Wave::sine, f * 14 / sr) * tine;
        tine *= tineK;
        outL = outR = c * level + t;
    }

    void tickBuffer()
    {
        if (maxHold > 0 && age >= maxHold) released = true;
        const bool rel = released && age >= hold;
        if (rel) { level -= level * kRelease; if (level < 1.0e-4f) { active = false; outL = outR = 0; return; } }
        const int i = (int) pos;
        const int n = (int) sample->size();
        if (i + 1 >= n) { active = false; outL = outR = 0; return; }
        const float fr = (float) (pos - i), a = (*sample)[(size_t) i], b = (*sample)[(size_t) (i + 1)];
        outL = outR = fL.run (a + (b - a) * fr) * level;
        pos += bend;
    }

    void tickAna()
    {
        double f = hz * bend * vibRatio;
        if (scoopLeft > 0) { f *= scoopRatio; scoopRatio *= scoopK; --scoopLeft; }
        float mix = 0;
        if (p.kind == Kind::juno)
        {
            if (p.saw > 0) mix += osc[0].tick (Wave::saw, f / sr) * p.saw;
            if (p.pulse > 0)
            {
                float width = p.pw;
                if (p.pwm > 0)
                {
                    width += .42f * p.pwm * (float) std::sin (2 * synthPi * lfoPh2);
                    lfoPh2 += (p.lfo[0] > 0 ? p.lfo[0] : .6) / sr; if (lfoPh2 >= 1) lfoPh2 -= 1;
                }
                mix += osc[1].tick (Wave::pulse, f / sr, width) * p.pulse * (p.pwm > 0 ? .9f : 1.0f);
            }
            if (p.sub > 0) mix += osc[2].tick (Wave::square, f * .5 / sr) * p.sub * .6f;
        }
        else
        {
            const float b = osc[1].tick (p.bWave, f * p.bMul * std::pow (2.0, p.det / 1200.0) / sr, p.bPw);
            double fa = f;
            if (p.pm > 0) { fa += pmNow * b; pmNow += (pmEnd - pmNow) * pmK; }
            if (p.aGain > 0) mix += osc[0].tick (p.aWave, fa / sr, p.aPw) * p.aGain;
            mix += b * p.bGain;
        }
        if (p.noise > 0) mix += noise.next() * p.noise;
        cutEnv.rel = ampEnv.rel = released;
        cutEnv.tick();
        const float a = ampEnv.tick();
        if (released && a < 1.0e-4f) { active = false; outL = outR = 0; return; }
        outL = outR = f2.run (fL.run (hp.run (mix * .5f))) * a;
    }

    DrumLayer& addLayer (float d, float a, float at)
    {
        auto& l = drum[(size_t) std::min (numDrum++, (int) drum.size() - 1)];
        l.f.reset(); l.osc.ph = 0;
        l.delay = (int) (at * sr);
        l.attackLeft = std::max (1, (int) (.002 * sr));
        l.left = l.attackLeft + std::max (1, (int) (d * sr));
        l.amp = 0; l.inc = a / (float) l.attackLeft;
        l.k = (float) std::pow (.0008 / std::max (.001, (double) a), 1.0 / std::max (1.0, (d - .002) * sr));
        return l;
    }

    void tickDrum()
    {
        float s = 0; bool alive = false;
        for (int i = 0; i < std::min (numDrum, (int) drum.size()); ++i)
        {
            auto& l = drum[(size_t) i];
            if (l.left <= 0) continue;
            alive = true;
            if (l.delay > 0) { --l.delay; continue; }
            if (l.attackLeft > 0) { l.amp += l.inc; --l.attackLeft; } else l.amp *= l.k;
            --l.left;
            float o;
            if (l.noise) o = l.f.run (noise.next());
            else
            {
                o = l.osc.tick (l.wave, l.hz / sr);
                if (l.rampLeft > 0) { l.hz *= l.hzK; --l.rampLeft; }
            }
            s += o * l.amp;
        }
        if (! alive) { active = false; outL = outR = 0; return; }
        outL = outR = fL.run (s);
    }
};

// ---- the Juno-style chorus: anaChorus() in the page. One per mode, shared by the notes that use it. ----
class Chorus
{
public:
    void prepare (double sampleRate, int mode)
    {
        sr = sampleRate;
        static constexpr float rates[] { .513f, .863f, 9.75f }, lows[] { .0016f, .0016f, .0033f }, highs[] { .0053f, .0053f, .0037f };
        const int m = std::clamp (mode - 1, 0, 2);
        rate = rates[m]; mid = (lows[m] + highs[m]) / 2 * (float) sr; dep = (highs[m] - lows[m]) / 2 * (float) sr;
        buf.assign ((size_t) (.02 * sr) + 4, 0.0f);
        w = 0; ph = 0;
        tone.reset(); tone.lowpass (9000, 1, sr);
    }
    void run (float in, float& l, float& r)
    {
        const float t = (float) (ph < .25 ? 4 * ph : ph < .75 ? 2 - 4 * ph : 4 * ph - 4);
        ph += rate / sr; if (ph >= 1) ph -= 1;
        buf[(size_t) w] = tone.run (in);
        l = .75f * (in + read (mid + dep * t));
        r = .75f * (in + read (mid - dep * t));
        if (++w >= (int) buf.size()) w = 0;
    }
private:
    float read (float delay) const
    {
        const int n = (int) buf.size();
        float pos = (float) w - delay;
        while (pos < 0) pos += (float) n;
        const int i = (int) pos % n, j = (i + 1) % n;
        const float fr = pos - std::floor (pos);
        return buf[(size_t) i] + (buf[(size_t) j] - buf[(size_t) i]) * fr;
    }
    double sr = 48000, ph = 0;
    float rate = .5f, mid = 100, dep = 50;
    std::vector<float> buf;
    int w = 0;
    Biquad tone;
};

// ---- the room: stands in for the page's convolution reverb (eight feedback delay lines) ----
class Room
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        static constexpr int base[] { 1499, 1889, 2297, 2711, 3163, 3571, 4013, 4493 };
        for (size_t i = 0; i < 8; ++i)
        {
            len[i] = std::max (8, (int) (base[i] * sr / 48000));
            line[i].assign ((size_t) len[i], 0.0f);
            pos[i] = 0; lp[i] = 0;
        }
        pre.highpass (180, 1, sr);
        setDecay (t60);
    }
    void setDecay (float seconds)
    {
        t60 = std::max (.1f, seconds);
        for (size_t i = 0; i < 8; ++i) fb[i] = (float) std::pow (10.0, -3.0 * len[i] / (sr * t60));
    }
    void run (float inL, float inR, float& outL, float& outR)
    {
        const float in = pre.run (.5f * (inL + inR));
        float o[8], sum = 0;
        for (size_t i = 0; i < 8; ++i)
        {
            o[i] = line[i][(size_t) pos[i]];
            lp[i] += (o[i] - lp[i]) * .55f;                 // the walls soak up the top end
            sum += lp[i];
        }
        sum *= .25f;                                        // Householder mixing, of the filtered signals so it keeps
        for (size_t i = 0; i < 8; ++i)                      // the loop gain below 1 at every frequency
        {
            line[i][(size_t) pos[i]] = (lp[i] - sum) * fb[i] + in * .35f;
            if (++pos[i] >= len[i]) pos[i] = 0;
        }
        outL = o[0] - o[2] + o[4] - o[6] + o[1];
        outR = o[1] - o[3] + o[5] - o[7] + o[2];
    }

private:
    double sr = 48000;
    float t60 = 1;
    std::array<std::vector<float>, 8> line;
    std::array<int, 8> len {}, pos {};
    std::array<float, 8> lp {}, fb {};
    Biquad pre;
};

// ---- the master: the page's "Polish" chain ----
class Compressor
{
public:
    void set (double sr, float threshDb, float ratio, float attack, float release)
    {
        thr = threshDb; slope = 1 - 1 / std::max (1.0f, ratio);
        kA = (float) std::exp (-1 / (attack * sr)); kR = (float) std::exp (-1 / (release * sr));
        // Web Audio's compressor adds this make-up gain by itself
        makeup = (float) std::pow (10.0, -thr * slope * .6 / 20);
    }
    float gain (float level)
    {
        const float db = 20 * std::log10 (std::max (level, 1.0e-6f));
        const float want = db > thr ? (db - thr) * slope : 0;
        reduce = want > reduce ? want + (reduce - want) * kA : want + (reduce - want) * kR;
        return std::pow (10.0f, -reduce / 20) * makeup;
    }
private:
    float thr = 0, slope = 0, kA = 0, kR = 0, reduce = 0, makeup = 1;
};

inline void polishLowShelf (Biquad& b, double f, double dB, double sr)
{
    const double A = std::pow (10.0, dB / 40), w = 2 * synthPi * f / sr, c = std::cos (w), s = 2 * std::sqrt (A) * std::sin (w) / 2 * std::sqrt (2.0);
    b.set (A * ((A + 1) - (A - 1) * c + s), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - s), (A + 1) + (A - 1) * c + s, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - s);
}
inline void polishHighShelf (Biquad& b, double f, double dB, double sr)
{
    const double A = std::pow (10.0, dB / 40), w = 2 * synthPi * f / sr, c = std::cos (w), s = 2 * std::sqrt (A) * std::sin (w) / 2 * std::sqrt (2.0);
    b.set (A * ((A + 1) + (A - 1) * c + s), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - s), (A + 1) - (A - 1) * c + s, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - s);
}

class Polish
{
public:
    void set (const PolishPatch& p, double sr)
    {
        patch = p;
        hp.highpass (p.on ? p.hp : 20, .7, sr);
        if (p.on) { glue.set (sr, p.thr, p.ratio, p.att, p.rel); lim.set (sr, p.lim, 20, .002f, .09f); }
        else lim.set (sr, -24, 12, .003f, .25f);
        make = p.on ? std::pow (10.0f, p.make / 20) * .867f : 1.0f;    // .867: measured against the page across its level range
        shelfL.reset(); shelfH.reset();
        hasShelf = p.on && (p.shelfLo != 0 || p.shelfHi != 0);
        if (hasShelf) { polishLowShelf (shelfL, 90, p.shelfLo, sr); polishHighShelf (shelfH, 10000, p.shelfHi, sr); }
    }
    void run (float& l, float& r)
    {
        l = hp.run (l, 0); r = hp.run (r, 1);
        if (hasShelf) { l = shelfH.run (shelfL.run (l, 0), 0); r = shelfH.run (shelfL.run (r, 1), 1); }
        if (patch.on)
        {
            const float g = glue.gain (std::max (std::abs (l), std::abs (r))) * make;
            l *= g; r *= g;
        }
        const float g = lim.gain (std::max (std::abs (l), std::abs (r)));
        l *= g; r *= g;
        if (patch.on) { l = clip (l) * .891f; r = clip (r) * .891f; }
        else { l = std::clamp (l, -1.0f, 1.0f); r = std::clamp (r, -1.0f, 1.0f); }
    }
private:
    float clip (float x) const
    {
        const float t = patch.drive, a = std::abs (x);
        if (a <= t) return x;
        const float o = t + (1 - t) * std::tanh ((a - t) / (1 - t));
        return x < 0 ? -o : o;
    }
    PolishPatch patch;
    Biquad hp, shelfL, shelfH;
    Compressor glue, lim;
    float make = 1;
    bool hasShelf = false;
};

// ---- the engine ----
} // namespace st
#include "SynthFx.h"
#include "SynthTrans.h"
namespace st
{
class Synth
{
public:
    static constexpr int maxVoices = 96;

    Synth() { sorted.reserve (8192); }

    // Not on the audio thread.
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        room.prepare (sr);
        for (int i = 0; i < 3; ++i) choruses[(size_t) i].prepare (sr, i + 1);
        for (auto& v : voices) v.active = false;
        current.reset();
        fxBus = nullptr;
        for (auto& t : transVoices) t.active = false;
        sweepLeft = 0;
        pfxL.assign ((size_t) 16 * pfxCap, 0.0f); pfxR.assign ((size_t) 16 * pfxCap, 0.0f);
        preL.assign ((size_t) pfxCap, 0.0f); preR.assign ((size_t) pfxCap, 0.0f);
        rbufL.assign ((size_t) FxBank::buses * pfxCap, 0.0f); rbufR.assign ((size_t) FxBank::buses * pfxCap, 0.0f);
    }

    // Your effect plug-ins on Semitone's own parts (desktop app). Before process(), the audio thread says which
    // channels have some (one bit each); `plugFx` runs a channel's chain on one block. Set `plugFx` before audio starts.
    std::atomic<uint32_t> plugFxMask { 0 };
    std::function<void (int, float*, float*, int)> plugFx;
    static constexpr int pfxCap = 8192;

    // Any thread. The caller keeps the previous patch alive for a few seconds, as with ClipPlayer's banks.
    void setPatch (std::shared_ptr<const SynthPatch> p) { std::atomic_store (&pending, std::move (p)); }

    // Audio thread. Mixes into L/R (R may equal L for mono). `audible`: 0..1, ramped across the block;
    // the voices keep running while it is 0, so they are in the right place when it comes back.
    // `beat`: song position of the block's first sample for the automation lanes, -1 to leave them alone.
    void process (const std::vector<Event>& events, int n, float* L, float* R, float audible = 1, double beat = -1, double beatInc = 0)
    {
        auto next = std::atomic_load (&pending);
        if (next != current)
        {
            current = std::move (next);
            fxBus = current ? current->fx.get() : nullptr;
            transAt = 0; lastBeat = -1;
            busL.fill (0); busR.fill (0); chorusCh.fill (-1); busLevel.fill (1); busPan.fill (0); busStereo.fill (false);
            if (current)
            {
                room.setDecay (current->roomT60);
                polish.set (current->polish, sr);
                bendRange = current->bendRange;
            }
        }
        if (! current || n <= 0) return;
        const uint32_t pmask = plugFx ? plugFxMask.load() : 0u;
        const bool pfx = pmask != 0 && n <= pfxCap && (int) preL.size() >= n;
        // audio tracks and mono channels always mix through their channel bus, like a part with effects
        uint32_t takeMask = 0;
        if (current->takes) for (const auto& seg : *current->takes) takeMask |= 1u << (seg.ch & 15);
        if (current->trans && current->transCh >= 0) takeMask |= 1u << current->transCh;   // the transitions mix through their own channel strip
        const uint32_t forced = takeMask | (current->monoMask & 0xffffu) | current->routeMask;   // routed parts mix through their channel bus too
        const int nb = (int) std::min (current->rbus.size(), (size_t) FxBank::buses);
        const bool busMode = fxBus != nullptr || pfx || forced != 0;
        auto plug = [&] (size_t c) { return pfx && ((pmask >> c) & 1u); };
        auto mono = [&] (size_t c) { return ((current->monoMask >> c) & 1u) != 0; };
        auto via = [&] (size_t c) { return (fxBus && fxBus->any[c]) || plug (c) || ((forced >> c) & 1u); };

        // nothing sounding for a while (the room has rung out): use no CPU at all
        if (events.empty() && activeVoices() == 0 && ! current->trans && ! current->takes && ! current->hasExt && ! pfx)
        {
            if (idle >= (int64_t) (sr * 8)) { gate = audible; return; }
            idle += n;
        }
        else idle = 0;

        sorted.assign (events.begin(), events.end());
        std::stable_sort (sorted.begin(), sorted.end(), [] (const Event& a, const Event& b) { return a.offset < b.offset; });

        const float dg = (audible - gate) / (float) n;
        secPerBeat = beatInc > 0 ? 1 / (beatInc * sr) : 0;
        // same order as the page: master gain, room, master effects, filter sweep, master fader, polish
        const bool monoOut = ((current->monoMask >> 16) & 1u) != 0;   // "Mono check" on the master: hear the mix as a mono speaker plays it
        auto finish = [&] (int i, float l, float r)
        {
                l *= .6f; r *= .6f;
                if (monoOut) l = r = .5f * (l + r);
                if (current->roomMix > 0)
                {
                    float wl, wr;
                    room.run (l, r, wl, wr);
                    l += wl * current->roomMix * roomGain; r += wr * current->roomMix * roomGain;
                }
                if (fxBus && fxBus->any[FxBank::master]) fxBus->run (FxBank::master, l, r);
                if (sweepLeft > 0)
                {
                    if ((sweepLeft & 31) == 0) { const double t = 1 - (double) sweepLeft / sweepTotal; sweep.lowpass (18000 * std::pow (700.0 / 18000, t), .9, sr); }
                    l = sweep.run (l, 0); r = sweep.run (r, 1); --sweepLeft;
                }
                l *= masterGain; r *= masterGain;
                polish.run (l, r);
                gate += dg;
                l *= gate; r *= gate;
                if (R == L || R == nullptr) L[i] += .5f * (l + r);
                else { L[i] += l; R[i] += r; }
        };
        size_t e = 0;
        for (int i = 0; i < n; ++i)
        {
            while (e < sorted.size() && sorted[e].offset <= i) handle (sorted[e++]);
            if ((i & 31) == 0)
            {
                updateLanes (beat < 0 ? -1 : beat + i * beatInc);
                if (beat >= 0) fireTrans (beat + i * beatInc);
            }
            float l = 0, r = 0;
            float rbL[FxBank::buses] {}, rbR[FxBank::buses] {};
            // a channel's panned, faded signal goes to its bus (or the mix) and to the returns it sends to; `dl`/`dr` is it before the fader
            auto toOut = [&] (size_t c, float ol, float orr, float dl, float dr)
            {
                const int o = current->rOut[c];
                if (o >= 0 && o < nb) { rbL[o] += ol; rbR[o] += orr; } else { l += ol; r += orr; }
                for (int b = 0; b < nb; ++b)
                {
                    const float g = current->rSend[c][(size_t) b];
                    if (g <= 0) continue;
                    if ((current->rPre[c] >> b) & 1u) { rbL[b] += dl * g; rbR[b] += dr * g; } else { rbL[b] += ol * g; rbR[b] += orr * g; }
                }
            };
            float tr = 0;
            for (auto& t : transVoices) if (t.active) tr += t.run (rng);
            if (tr != 0)
            {
                const int tc = current->transCh;
                if (tc >= 0) { busL[(size_t) tc] += tr; busR[(size_t) tc] += tr; busLevel[(size_t) tc] = current->transLevel; busPan[(size_t) tc] = current->transPan; busStereo[(size_t) tc] = false; }
                else { float pl, pr; panGains (0, false, pl, pr); l += tr * current->transLevel * pl; r += tr * current->transLevel * pr; }
            }
            if (beat >= 0 && current->takes)
            {
                const double now = beat + i * beatInc, secPerBeat = beatInc > 0 ? 1 / (beatInc * sr) : 0;
                for (const auto& seg : *current->takes)
                {
                    if (now < seg.beat || now >= seg.end || ! seg.audio) continue;
                    const double pos = (seg.start + (now - seg.beat) * secPerBeat) * seg.audio->rate;
                    if (pos < 0) continue;
                    const size_t a = (size_t) pos, k = (size_t) std::max (1, seg.audio->chans), at = a * k;
                    if (at + 2 * k > seg.audio->data.size()) continue;
                    const float fr = (float) (pos - std::floor (pos));
                    const auto& d = seg.audio->data;
                    const float tkl = d[at] * (1 - fr) + d[at + k] * fr, tkr = k == 2 ? d[at + 1] * (1 - fr) + d[at + 3] * fr : tkl;
                    const size_t c = (size_t) (seg.ch & 15);
                    busL[c] += tkl; busR[c] += tkr;
                    busLevel[c] = takeLevel * current->takeChLevel[c]; busPan[c] = current->takeChPan[c]; busStereo[c] = k == 2;
                }
            }
            // your plug-ins join here, so they get the part's fader, pan, effects, the room and the master chain
            if (current->hasExt)
                for (size_t c = 0; c < 16; ++c)
                {
                    if (extL[c] == nullptr) continue;
                    const float xl = extL[c][i] * extTrim, xr = extR[c][i] * extTrim;
                    if (via (c)) { busL[c] += xl; busR[c] += xr; busLevel[c] = current->extLevel[c]; busPan[c] = current->extPan[c]; busStereo[c] = true; continue; }
                    float pl, pr;
                    panGains (hasPan[c] ? chPan[c] : current->extPan[c], true, pl, pr);
                    l += xl * current->extLevel[c] * chGain[c] * pl; chSum[c] += xl * current->extLevel[c] * chGain[c];
                    r += xr * current->extLevel[c] * chGain[c] * pr;
                }
            float chorusIn[3] { 0, 0, 0 };
            for (auto& v : voices)
            {
                if (! v.active) continue;
                v.tick (bends[(size_t) (v.ch & 15)]);
                if (! v.active) continue;
                const int ci = v.ch & 15;
                if (! v.isDrum && v.p.chorus > 0) { chorusIn[(size_t) (v.p.chorus - 1)] += v.outL * (busMode ? 1.0f : v.p.level * chGain[(size_t) ci]); if (busMode) { chorusCh[(size_t) (v.p.chorus - 1)] = ci; busLevel[(size_t) ci] = v.p.level; busPan[(size_t) ci] = v.p.pan; busStereo[(size_t) ci] = true; } continue; }
                if (via ((size_t) ci)) { busL[(size_t) ci] += v.outL; busR[(size_t) ci] += v.outR; busLevel[(size_t) ci] = v.p.level; busPan[(size_t) ci] = v.p.pan; busStereo[(size_t) ci] = v.p.stereo; continue; }
                float pl, pr;
                panGains (hasPan[(size_t) ci] ? chPan[(size_t) ci] : v.p.pan, v.p.stereo, pl, pr);
                l += v.outL * v.p.level * chGain[(size_t) ci] * pl; chSum[(size_t) ci] += v.outL * v.p.level * chGain[(size_t) ci];
                r += v.outR * v.p.level * chGain[(size_t) ci] * pr;
            }
            for (size_t c = 0; c < 3; ++c)
            {
                float cl, cr;
                choruses[c].run (chorusIn[c], cl, cr);
                if (busMode)
                {
                    const int ci = chorusCh[c];
                    if (ci >= 0 && via ((size_t) ci)) { busL[(size_t) ci] += cl; busR[(size_t) ci] += cr; continue; }
                    if (ci >= 0) { cl *= busLevel[(size_t) ci] * chGain[(size_t) ci]; cr *= busLevel[(size_t) ci] * chGain[(size_t) ci]; }
                }
                l += cl; r += cr;
            }
            if (busMode)
                for (size_t c = 0; c < 16; ++c)
                {
                    if (! via (c)) continue;
                    float bl = busL[c], br = busR[c];
                    busL[c] = busR[c] = 0;
                    if (fxBus && fxBus->any[c]) fxBus->run ((int) c, bl, br);
                    if (mono (c)) { bl = br = .5f * (bl + br); }
                    // to your plug-ins: they need the whole block, so this part is mixed in by the second pass below
                    if (plug (c)) { pfxL[c * pfxCap + (size_t) i] = bl * busLevel[c]; pfxR[c * pfxCap + (size_t) i] = br * busLevel[c]; continue; }
                    float pl, pr;
                    panGains (hasPan[c] ? chPan[c] : busPan[c], busStereo[c] && ! mono (c), pl, pr);
                    const float g = busLevel[c] * chGain[c];
                    toOut (c, bl * g * pl, br * g * pr, bl, br); chSum[c] += bl * g;
                }
            for (size_t c = 0; c < 16; ++c)
                if (chSum[c] != 0) { chMax[c] = std::max (chMax[c], std::abs (chSum[c])); chSum[c] = 0; }
            if (pfx)
            {
                preL[(size_t) i] = l; preR[(size_t) i] = r;
                for (int b = 0; b < nb; ++b) { rbufL[(size_t) b * pfxCap + (size_t) i] = rbL[b]; rbufR[(size_t) b * pfxCap + (size_t) i] = rbR[b]; }
                continue;
            }
            runBuses (nb, rbL, rbR, l, r);
            finish (i, l, r);
        }
        if (pfx)
        {
            // second pass: each part with plug-ins runs its chain on the whole block, then joins the mix
            for (size_t c = 0; c < 16; ++c) if (plug (c)) plugFx ((int) c, &pfxL[c * pfxCap], &pfxR[c * pfxCap], n);
            for (int i = 0; i < n; ++i)
            {
                if ((i & 31) == 0) updateLanes (beat < 0 ? -1 : beat + i * beatInc);
                float l = preL[(size_t) i], r = preR[(size_t) i];
                float rbL[FxBank::buses] {}, rbR[FxBank::buses] {};
                for (int b = 0; b < nb; ++b) { rbL[b] = rbufL[(size_t) b * pfxCap + (size_t) i]; rbR[b] = rbufR[(size_t) b * pfxCap + (size_t) i]; }
                for (size_t c = 0; c < 16; ++c)
                {
                    if (! plug (c)) continue;
                    float bl = pfxL[c * pfxCap + (size_t) i], br = pfxR[c * pfxCap + (size_t) i];
                    if (mono (c)) bl = br = .5f * (bl + br);
                    float pl, pr;
                    panGains (hasPan[c] ? chPan[c] : busPan[c], busStereo[c] && ! mono (c), pl, pr);
                    const float ol = bl * chGain[c] * pl, orr = br * chGain[c] * pr;
                    const int o = current->rOut[c];
                    if (o >= 0 && o < nb) { rbL[o] += ol; rbR[o] += orr; } else { l += ol; r += orr; }
                    for (int b = 0; b < nb; ++b)
                    {
                        const float g = current->rSend[c][(size_t) b];
                        if (g <= 0) continue;
                        if ((current->rPre[c] >> b) & 1u) { rbL[b] += bl * g; rbR[b] += br * g; } else { rbL[b] += ol * g; rbR[b] += orr * g; }
                    }
                    chMax[c] = std::max (chMax[c], std::abs (bl * chGain[c]));
                }
                runBuses (nb, rbL, rbR, l, r);
                finish (i, l, r);
            }
        }
        gate = audible;
        for (size_t c = 0; c < chMax.size(); ++c)
            if (chMax[c] > 0) { chPeak[c].store (std::max (chPeak[c].load(), chMax[c])); chMax[c] = 0; }
    }

    // Any thread: the loudest this channel has been since the last call, for its meter (17 and up: the buses and returns).
    float takeChannelPeak (int ch) { return ch >= 0 && ch < (int) chPeak.size() ? chPeak[(size_t) ch].exchange (0) : 0.0f; }

    // Audio thread: cut everything off (the sound was switched to MIDI only).
    void silence()
    {
        for (auto& v : voices) v.active = false;
        bends.fill (1);
    }

    int activeVoices() const
    {
        int c = 0;
        for (auto& v : voices) c += v.active ? 1 : 0;
        return c;
    }

    static constexpr float roomGain = .32f;    // matches the page's normalised convolver

public:
    float takeLevel = 1;
    // Audio thread, before process(): one block of a plug-in's sound for this channel, or nullptr
    void setExternal (int ch, const float* l, const float* r) { if (ch >= 0 && ch < 16) { extL[(size_t) ch] = l; extR[(size_t) ch] = r; } }
    static constexpr float extTrim = 1.0f / .6f;   // cancels the master gain, which is matched to the page's quiet voices
private:
    void fireTrans (double now)
    {
        if (! current || ! current->trans) return;
        const auto& L = *current->trans;
        if (now < lastBeat) transAt = 0;   // looped or moved back
        while (transAt < L.size() && L[transAt].at <= now)
        {
            const auto& e = L[transAt++];
            if (e.at < now - .05) continue;   // jumped past it
            if (e.kind == TransEv::sweep) { sweepTotal = sweepLeft = std::max (32, (int) (e.dur * sr)); sweep.reset(); }
            else if (e.kind == TransEv::snare)
            {
                if (current->drumCh >= 0) { auto& v = freeVoice(); v.order = ++counter; v.startDrum (current->kit, 38, e.g0, current->drumCh, current->drumLevel, current->drumPan, sr, rng); rng.next(); }
            }
            else for (auto& t : transVoices) if (! t.active) { t.start (e, sr); break; }
        }
        lastBeat = now;
    }

    void updateLanes (double beat)
    {
        chGain.fill (1); hasPan.fill (false); masterGain = 1;
        if (beat < 0 || ! current) return;
        for (const auto& k : current->lanes)
        {
            if (k.macro >= 0 && (macroOn == nullptr || ! macroOn[k.macro].load (std::memory_order_relaxed))) continue;
            const float v = k.macro >= 0 ? k.fromMacro (macroVals[k.macro].load (std::memory_order_relaxed)) : k.at (beat);
            if (k.fxSlot >= 0) { if (fxBus) if (auto* f = fxBus->at (k.ch < 0 ? FxBank::master : k.ch, k.fxSlot)) f->set (k.fxParam, v); }
            else if (k.ch < 0) { if (! k.pan) masterGain = v; }
            else if (k.pan) { chPan[(size_t) (k.ch & 15)] = v; hasPan[(size_t) (k.ch & 15)] = true; }
            else chGain[(size_t) (k.ch & 15)] = v;
        }
        for (const auto& d : current->ducks) chGain[(size_t) (d.ch & 15)] *= d.gainAt (beat, secPerBeat);
    }

    void handle (const Event& e)
    {
        const int type = e.status & 0xF0, ch = e.status & 15;
        if (type == 0xE0)
        {
            const int v = e.d1 | (e.d2 << 7);
            bends[(size_t) ch] = std::pow (2.0, bendRange * (v - 8192) / 8192.0 / 12.0);
        }
        else if (type == 0x90 && e.d2 > 0) noteOn (ch, e.d1, e.d2);
        else if (type == 0x80 || type == 0x90)
        {
            for (auto& v : voices)
                if (v.active && ! v.isDrum && ! v.released && v.ch == ch && v.key == e.d1) { v.noteOff(); break; }
        }
    }

    Voice& freeVoice()
    {
        Voice* best = nullptr;
        for (auto& v : voices)
        {
            if (! v.active) return v;
            if (best == nullptr || (v.released && ! best->released) || (v.released == best->released && v.order < best->order)) best = &v;
        }
        return *best;
    }

    void noteOn (int ch, int key, int vel)
    {
        if (ch == current->drumCh)
        {
            if (current->drumExt) return;
            auto& v = freeVoice();
            v.order = ++counter;
            v.startDrum (current->kit, key, (float) vel / 100.0f, ch, current->drumLevel, current->drumPan, sr, rng);
            rng.next();
            return;
        }
        for (size_t i = 0; i < current->parts.size(); ++i)
        {
            const auto& p = current->parts[i];
            if (p.ch != ch) continue;
            const NoteBuffer* buf = i < current->buffers.size() ? current->buffers[i][(size_t) (key & 127)] : nullptr;
            if (p.kind == Kind::buffer && buf == nullptr) return;   // still being prepared
            auto& v = freeVoice();
            v.order = ++counter;
            v.startNote (p, key, (float) vel / p.velRef, sr, buf, rng);
            rng.next();
            return;
        }
    }

    double sr = 48000;
    float bendRange = 12, gate = 1;
    uint32_t counter = 0;
    int64_t idle = 0;
    Noise rng;
    std::array<Voice, maxVoices> voices {};
    std::array<float, 16> chGain { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, chPan {};
    std::array<bool, 16> hasPan {}, busStereo {};
    std::array<float, 16> busL {}, busR {}, busLevel {}, busPan {};
    std::array<const float*, 16> extL {}, extR {};
    std::array<int, 3> chorusCh { -1, -1, -1 };
    FxBank* fxBus = nullptr;
    std::array<TransVoice, 8> transVoices;
    size_t transAt = 0;
    double lastBeat = -1;
    int sweepLeft = 0, sweepTotal = 1;
    Biquad sweep;
    float masterGain = 1;
public:
    // FL Studio's "Macro 1-8" host parameters: values, and whether FL has taken each one over (set by the processor)
    const std::atomic<float>* macroVals = nullptr;
    const std::atomic<bool>* macroOn = nullptr;
private:
    double secPerBeat = 0;
    std::array<float, 16> chSum {};
    std::array<float, FxBank::count> chMax {};
    std::array<std::atomic<float>, FxBank::count> chPeak {};
    std::vector<float> rbufL, rbufR;   // the buses' input for a block, while your plug-ins' second pass runs
    // the buses and returns: their own EQ, compressor and inserts, then fader and pan, into the mix (same order as the page)
    void runBuses (int nb, const float* inL, const float* inR, float& l, float& r)
    {
        for (int b = 0; b < nb; ++b)
        {
            float bl = inL[b], br = inR[b];
            const int fc = FxBank::master + 1 + b;
            if (fxBus && fxBus->any[(size_t) fc]) fxBus->run (fc, bl, br);
            const auto& B = current->rbus[(size_t) b];
            if (B.mono) bl = br = .5f * (bl + br);
            float pl, pr;
            panGains (B.pan, ! B.mono, pl, pr);
            const float ol = bl * B.level * pl, orr = br * B.level * pr;
            l += ol; r += orr;
            chMax[(size_t) fc] = std::max (chMax[(size_t) fc], std::max (std::abs (ol), std::abs (orr)));
        }
    }
    std::array<double, 16> bends { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    std::vector<Event> sorted;
    Room room;
    std::array<Chorus, 3> choruses;
    Polish polish;
    std::shared_ptr<const SynthPatch> pending, current;
    std::vector<float> pfxL, pfxR, preL, preR;
};
} // namespace st
