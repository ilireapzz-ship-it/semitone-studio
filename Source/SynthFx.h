#pragma once
// Part effects for the native engine: the page's filter, saturator, tube warmth, EQ, Smart EQ, echo, plate, chorus and tremolo.
// Included by Synth.h after Biquad and Room. The plate is a feedback network here, not the page's convolver.
#include <map>
#include <memory>
#include <string>

namespace st
{
struct FxDesc
{
    int ch = 0, slot = 0;           // ch 16 = the master bus
    std::string type, style;
    std::map<std::string, float> p;
    float get (const char* k, float d) const { auto i = p.find (k); return i == p.end() ? d : i->second; }
};

inline void shelfLow (Biquad& b, double f, double dB, double sr)
{
    const double A = std::pow (10.0, dB / 40), w = 2 * synthPi * std::min (f, sr * .49) / sr, c = std::cos (w), al = std::sin (w) / 2 * std::sqrt (2.0), s = 2 * std::sqrt (A) * al;
    b.set (A * ((A + 1) - (A - 1) * c + s), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - s), (A + 1) + (A - 1) * c + s, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - s);
}
inline void shelfHigh (Biquad& b, double f, double dB, double sr)
{
    const double A = std::pow (10.0, dB / 40), w = 2 * synthPi * std::min (f, sr * .49) / sr, c = std::cos (w), al = std::sin (w) / 2 * std::sqrt (2.0), s = 2 * std::sqrt (A) * al;
    b.set (A * ((A + 1) + (A - 1) * c + s), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - s), (A + 1) - (A - 1) * c + s, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - s);
}
inline void peak (Biquad& b, double f, double q, double dB, double sr)
{
    const double A = std::pow (10.0, dB / 40), w = 2 * synthPi * std::min (f, sr * .49) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
    b.set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
}
inline float shape (const std::string& k, float x)
{
    x = std::clamp (x, -1.0f, 1.0f);
    if (k == "Tube") return (x >= 0 ? std::tanh (x * 1.6f) : std::tanh (x * .8f)) / std::tanh (1.6f);
    if (k == "Transistor") return std::clamp (x * 1.8f, -1.0f, 1.0f);
    if (k == "Fuzz") return (x < 0 ? -1.0f : 1.0f) * (1 - std::exp (-std::abs (x) * 7)) / (1 - std::exp (-7.0f));
    if (k == "warm") return std::tanh (x + .35f * x * x) / std::tanh (1.35f);
    return std::tanh (x * 1.4f) / std::tanh (1.4f);
}

class DelayLine
{
public:
    void prepare (int n) { buf.assign ((size_t) std::max (8, n), 0.0f); w = 0; }
    void write (float x) { buf[(size_t) w] = x; w = (w + 1) % (int) buf.size(); }
    float read (float samples) const   // the sample written `samples` ago
    {
        const int n = (int) buf.size();
        samples = std::clamp (samples, 1.0f, (float) (n - 2));
        const float pos = (float) (w - 1) - samples + (float) n * 2;
        const int a = (int) pos % n, b = (a + 1) % n; const float t = pos - std::floor (pos);
        return buf[(size_t) a] * (1 - t) + buf[(size_t) b] * t;
    }
private:
    std::vector<float> buf; int w = 0;
};

class Fx
{
public:
    virtual ~Fx() = default;
    virtual void run (float& l, float& r) = 0;
    virtual int param (const std::string&) const { return -1; }
    virtual void set (int, float) {}
protected:
    float mix = 1;
    static double two() { return 2 * synthPi; }
};

class FilterFx : public Fx
{
public:
    FilterFx (const FxDesc& d, double rate) : sr (rate)
    {
        kind = d.style == "High-pass" ? 1 : d.style == "Band-pass" ? 2 : 0;
        cutoff = d.get ("cutoff", 1400); res = d.get ("res", .35f); depth = d.get ("depth", .5f); mix = d.get ("mix", 1); inc = d.get ("sec", 0) > 0 ? 1 / (d.get ("sec", 1) * sr) : 0;
        update (0);
    }
    int param (const std::string& k) const override { return k == "cutoff" ? 0 : k == "res" ? 1 : k == "depth" ? 2 : k == "mix" ? 3 : -1; }
    void set (int i, float v) override { if (i == 0) cutoff = v; else if (i == 1) res = v; else if (i == 2) depth = v; else if (i == 3) mix = v; }
    void run (float& l, float& r) override
    {
        if (++tick >= 32) { tick = 0; update (inc > 0 ? std::sin (two() * ph) : 0); }
        ph += inc * 1; if (ph >= 1) ph -= 1;
        const float wl = f.run (l, 0), wr = f.run (r, 1);
        l = l * (1 - mix) + wl * mix; r = r * (1 - mix) + wr * mix;
    }
private:
    void update (double lfo)
    {
        const double hz = cutoff * std::pow (2.0, depth * 2400 * lfo / 1200 * (inc > 0 ? 1 : 0));
        if (kind == 2) f.bandpass (hz, .5 + res * 12, sr);
        else if (kind == 1) f.highpass (hz, res * 18, sr);
        else f.lowpass (hz, res * 18, sr);
    }
    Biquad f; double sr, ph = 0, inc = 0; int kind = 0, tick = 0; float cutoff, res, depth;
};

class SatFx : public Fx
{
public:
    SatFx (const FxDesc& d, double rate) : sr (rate), style (d.style.empty() ? "Tape" : d.style)
    {
        drive = d.get ("drive", .4f); tone = d.get ("tone", 9000); out = d.get ("out", 1); mix = d.get ("mix", 1); k0 = 1 + drive * 24;
        hp.highpass (25, 1, sr); lp.lowpass (tone, 1, sr);
    }
    int param (const std::string& k) const override { return k == "drive" ? 0 : k == "tone" ? 1 : k == "out" ? 2 : k == "mix" ? 3 : -1; }
    void set (int i, float v) override { if (i == 0) drive = v; else if (i == 1) { tone = v; lp.lowpass (tone, 1, sr); } else if (i == 2) out = v; else if (i == 3) mix = v; }
    void run (float& l, float& r) override
    {
        const float k = 1 + drive * 24, post = out / std::sqrt (k0);
        const float wl = lp.run (hp.run (shape (style, l * k), 0), 0) * post, wr = lp.run (hp.run (shape (style, r * k), 1), 1) * post;
        l = l * (1 - mix) + wl * mix; r = r * (1 - mix) + wr * mix;
    }
private:
    double sr; std::string style; float drive, tone, out, k0; Biquad hp, lp;
};

class WarmFx : public Fx
{
public:
    WarmFx (const FxDesc& d, double rate) : sr (rate)
    {
        drive = d.get ("drive", .35f); mix = d.get ("mix", 1); k0 = 1 + drive * 6;
        hp.highpass (25, 1, sr); shelfLow (ls, 120, d.get ("low", 3), sr); shelfHigh (hs, 6000, d.get ("high", -2), sr);
    }
    int param (const std::string& k) const override { return k == "drive" ? 0 : k == "low" ? 1 : k == "high" ? 2 : k == "mix" ? 3 : -1; }
    void set (int i, float v) override { if (i == 0) drive = v; else if (i == 1) shelfLow (ls, 120, v, sr); else if (i == 2) shelfHigh (hs, 6000, v, sr); else if (i == 3) mix = v; }
    void run (float& l, float& r) override
    {
        const float k = 1 + drive * 6, post = 1 / std::sqrt (k0);
        const float wl = hs.run (ls.run (hp.run (shape ("warm", l * k), 0), 0), 0) * post, wr = hs.run (ls.run (hp.run (shape ("warm", r * k), 1), 1), 1) * post;
        l = l * (1 - mix) + wl * mix; r = r * (1 - mix) + wr * mix;
    }
private:
    double sr; float drive, k0; Biquad hp, ls, hs;
};

class EqFx : public Fx
{
public:
    EqFx (const FxDesc& d, double rate) : sr (rate)
    {
        midF = d.get ("midF", 1000); mid = d.get ("mid", 0);
        shelfLow (ls, 100, d.get ("low", 0), sr); peak (pk, midF, .9, mid, sr); shelfHigh (hs, 8000, d.get ("high", 0), sr);
    }
    int param (const std::string& k) const override { return k == "low" ? 0 : k == "mid" ? 1 : k == "midF" ? 2 : k == "high" ? 3 : -1; }
    void set (int i, float v) override { if (i == 0) shelfLow (ls, 100, v, sr); else if (i == 1) { mid = v; peak (pk, midF, .9, mid, sr); } else if (i == 2) { midF = v; peak (pk, midF, .9, mid, sr); } else if (i == 3) shelfHigh (hs, 8000, v, sr); }
    void run (float& l, float& r) override { l = hs.run (pk.run (ls.run (l, 0), 0), 0); r = hs.run (pk.run (ls.run (r, 1), 1), 1); }
private:
    double sr; float midF, mid; Biquad ls, pk, hs;
};

// Smart EQ (docs/design/mixing §1): up to 24 bands per strip, run before the inserts. Same maths as the page's BiquadFilterNodes:
// bell/shelves/notch/band-pass are RBJ with linear Q, cuts are Butterworth cascades (12/24/48 dB/oct), tilt is a low and a high shelf at the pivot.
// p: n = band count; per band i: t<i> type (0 bell, 1 low shelf, 2 high shelf, 3 low cut, 4 high cut, 5 notch, 6 tilt, 7 band pass), f<i>, g<i>, q<i>, s<i> slope.
inline void peqNotch (Biquad& b, double f, double q, double sr)
{
    const double w = 2 * synthPi * std::min (f, sr * .49) / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
    b.set (1, -2 * c, 1, 1 + al, -2 * c, 1 - al);
}
inline int peqStages (float slope) { return slope >= 47 ? 4 : slope >= 23 ? 2 : 1; }
inline const double* peqQs (int n)
{
    static const double one[] = { .7071 }, two[] = { .5412, 1.3066 }, four[] = { .5098, .6013, .9000, 2.5629 };
    return n == 4 ? four : n == 2 ? two : one;
}
class PeqFx : public Fx
{
public:
    PeqFx (const FxDesc& d, double rate) : sr (rate)
    {
        const int n = std::clamp ((int) d.get ("n", 0), 0, 24);
        for (int i = 0; i < n; ++i)
        {
            const std::string k = std::to_string (i);
            const int t = (int) d.get (("t" + k).c_str(), 0); const double f = std::clamp ((double) d.get (("f" + k).c_str(), 1000), 10.0, 22000.0), g = d.get (("g" + k).c_str(), 0), q = std::max (.05, (double) d.get (("q" + k).c_str(), .9));
            if (t == 3 || t == 4)
            {
                const int st = peqStages (d.get (("s" + k).c_str(), 12)); const double* qs = peqQs (st);
                for (int j = 0; j < st; ++j) { Biquad b; if (t == 3) b.highpass (f, 20 * std::log10 (qs[j]), sr); else b.lowpass (f, 20 * std::log10 (qs[j]), sr); bq.push_back (b); }
            }
            else if (t == 6) { Biquad a, b; shelfLow (a, f, -g / 2, sr); shelfHigh (b, f, g / 2, sr); bq.push_back (a); bq.push_back (b); }
            else { Biquad b; if (t == 1) shelfLow (b, f, g, sr); else if (t == 2) shelfHigh (b, f, g, sr); else if (t == 5) peqNotch (b, f, q, sr); else if (t == 7) b.bandpass (f, q, sr); else peak (b, f, q, g, sr); bq.push_back (b); }
        }
        out = std::pow (10.0f, d.get ("out", 0) / 20);
    }
    void run (float& l, float& r) override { for (auto& b : bq) { l = b.run (l, 0); r = b.run (r, 1); } l *= out; r *= out; }
    size_t stages() const { return bq.size(); }
private:
    double sr; std::vector<Biquad> bq; float out = 1;
};

class EchoFx : public Fx
{
public:
    EchoFx (const FxDesc& d, double rate) : sr (rate), style (d.style.empty() ? "Tape" : d.style)
    {
        sec = (float) (std::min (3.9f, d.get ("sec", .375f)) * sr); fb = d.get ("fb", .4f); tone = d.get ("tone", 3500); wob = d.get ("wob", .3f); send = d.get ("send", 1); mix = d.get ("mix", .3f);
        pong = style == "Ping-pong"; a.prepare ((int) (sr * 4.1)); if (pong) b.prepare ((int) (sr * 4.1));
        fA.lowpass (tone, 1, sr); fB.lowpass (tone, 1, sr); hpf.highpass (style == "Digital" ? 20 : 160, 1, sr);
    }
    int param (const std::string& k) const override { return k == "fb" ? 0 : k == "tone" ? 1 : k == "wob" ? 2 : k == "send" ? 3 : k == "mix" ? 4 : -1; }
    void set (int i, float v) override { if (i == 0) fb = v; else if (i == 1) { tone = v; fA.lowpass (v, 1, sr); fB.lowpass (v, 1, sr); } else if (i == 2) wob = v; else if (i == 3) send = v; else if (i == 4) mix = v; }
    void run (float& l, float& r) override
    {
        const float in = .5f * (l + r) * send;
        float wl, wr;
        if (pong)
        {
            const float x = fA.run (a.read (sec), 0), y = fB.run (b.read (sec), 1);
            a.write (in + y * fb); b.write (x * fb);
            wl = x; wr = y;
        }
        else
        {
            float d = sec;
            if (style == "Tape") { t1 += .6 / sr; t2 += 6.3 / sr; d += (float) ((wob * .004 * std::sin (two() * t1) + wob * .0003 * std::sin (two() * t2)) * sr); }
            const float w = hpf.run (fA.run (a.read (d), 0), 0);
            a.write (in + (style == "Digital" ? w : shape ("Tape", w)) * fb);
            wl = wr = w;
        }
        l = l * (1 - mix) + wl * mix; r = r * (1 - mix) + wr * mix;
    }
private:
    double sr, t1 = 0, t2 = 0; std::string style; bool pong; float sec, fb, tone, wob, send; DelayLine a, b; Biquad fA, fB, hpf;
};

class PlateFx : public Fx
{
public:
    PlateFx (const FxDesc& d, double rate) : sr (rate)
    {
        mix = d.get ("mix", .3f); pre = d.get ("pre", 20) / 1000 * (float) sr; room.prepare (sr); room.setDecay (d.get ("decay", 1.8f));
        line.prepare ((int) (sr * .6)); damp.lowpass (d.get ("damp", 7000), 1, sr);
    }
    int param (const std::string& k) const override { return k == "pre" ? 0 : k == "damp" ? 1 : k == "mix" ? 2 : -1; }
    void set (int i, float v) override { if (i == 0) pre = v / 1000 * (float) sr; else if (i == 1) damp.lowpass (v, 1, sr); else if (i == 2) mix = v; }
    void run (float& l, float& r) override
    {
        const float x = line.read (pre); line.write (.5f * (l + r));
        float wl, wr; room.run (x, x, wl, wr);
        wl = damp.run (wl * gain, 0); wr = damp.run (wr * gain, 1);
        l = l * (1 - mix) + wl * mix; r = r * (1 - mix) + wr * mix;
    }
    static constexpr float gain = .32f;
private:
    double sr; float pre; Room room; DelayLine line; Biquad damp;
};

class ChorusFx : public Fx
{
public:
    ChorusFx (const FxDesc& d, double rate) : sr (rate), style (d.style.empty() ? "Chorus" : d.style)
    {
        hz = d.get ("rate", .8f); depth = d.get ("depth", .5f); fb = d.get ("fb", .2f); mix = style == "Vibrato" ? 1 : d.get ("mix", .5f);
        a.prepare ((int) (sr * .12)); b.prepare ((int) (sr * .12));
    }
    int param (const std::string& k) const override { return k == "rate" ? 0 : k == "depth" ? 1 : k == "fb" ? 2 : k == "mix" ? 3 : -1; }
    void set (int i, float v) override { if (i == 0) hz = v; else if (i == 1) depth = v; else if (i == 2) fb = v; else if (style != "Vibrato" && i == 3) mix = v; }
    void run (float& l, float& r) override
    {
        ph += hz / sr; if (ph >= 1) ph -= 1;
        const float o = (float) std::sin (two() * ph);
        float wl, wr;
        if (style == "Chorus")
        {
            const float dl = (.018f + o * depth * .006f) * (float) sr, dr = (.026f - o * depth * .006f) * (float) sr;
            wl = a.read (dl); wr = b.read (dr);
            a.write (l + wl * fb * .6f); b.write (r + wr * fb * .6f);
        }
        else
        {
            const bool fl = style == "Flanger"; const float d = ((fl ? .0045f : .006f) + o * depth * (fl ? .0032f : .004f)) * (float) sr;
            wl = wr = a.read (d); a.write (.5f * (l + r) + wl * (fl ? fb : 0));
        }
        l = l * (1 - mix) + wl * mix; r = r * (1 - mix) + wr * mix;
    }
private:
    double sr, ph = 0; std::string style; float hz, depth, fb; DelayLine a, b;
};

class TremFx : public Fx
{
public:
    TremFx (const FxDesc& d, double rate) : style (d.style.empty() ? "sine" : d.style) { depth = d.get ("depth", .7f); inc = 1 / (std::max (.01f, d.get ("sec", .25f)) * rate); }
    int param (const std::string& k) const override { return k == "depth" ? 0 : -1; }
    void set (int, float v) override { depth = v; }
    void run (float& l, float& r) override
    {
        ph += inc; if (ph >= 1) ph -= 1;
        const float o = style == "square" ? (ph < .5 ? 1.0f : -1.0f) : style == "sawtooth" ? (float) (2 * ph - 1) : style == "triangle" ? (float) (1 - 4 * std::abs (ph - .5)) : (float) std::sin (two() * ph);
        const float g = 1 - depth / 2 + depth / 2 * o;
        l *= g; r *= g;
    }
private:
    std::string style; double ph = 0, inc; float depth;
};


class CrushFx : public Fx
{
public:
    CrushFx (const FxDesc& d, double rate) : sr (rate)
    {
        crush = d.get ("crush", .6f); mix = d.get ("mix", .5f); c.set (sr, -34, 20, .001f, .06f); f.lowpass (d.get ("dark", 7000), 1, sr);
    }
    int param (const std::string& k) const override { return k == "crush" ? 0 : k == "dark" ? 1 : k == "mix" ? 2 : -1; }
    void set (int i, float v) override { if (i == 0) crush = v; else if (i == 1) f.lowpass (v, 1, sr); else if (i == 2) mix = v; }
    void run (float& l, float& r) override
    {
        const float k = 1 + crush * 12; float a = l * k, b = r * k;
        const float g = c.gain (std::max (std::abs (a), std::abs (b))); a *= g; b *= g;
        const float wl = f.run (shape ("Tape", a), 0) * wetTrim, wr = f.run (shape ("Tape", b), 1) * wetTrim;
        l = l * (1 - mix) + wl * mix; r = r * (1 - mix) + wr * mix;
    }
private:
    static constexpr float wetTrim = .38f;   // matched by level against the page's compressor, on a sustained note
    double sr; float crush; Compressor c; Biquad f;
};

class LofiFx : public Fx
{
public:
    LofiFx (const FxDesc& d, double rate) : sr (rate)
    {
        steps = std::pow (2.0f, d.get ("bits", 8) - 1); wob = d.get ("wob", .35f); hiss = d.get ("noise", .2f); mix = d.get ("mix", 1);
        hp.highpass (140, 1, sr); f.lowpass (d.get ("tone", 3800), .9, sr); nf.bandpass (4000, .6, sr); a.prepare ((int) (sr * .1)); b.prepare ((int) (sr * .1));
    }
    int param (const std::string& k) const override { return k == "tone" ? 0 : k == "wob" ? 1 : k == "noise" ? 2 : k == "mix" ? 3 : -1; }
    void set (int i, float v) override { if (i == 0) f.lowpass (v, .9, sr); else if (i == 1) wob = v; else if (i == 2) hiss = v; else if (i == 3) mix = v; }
    void run (float& l, float& r) override
    {
        t1 += .55 / sr; t2 += 7 / sr;
        const float dl = (float) ((.008 + wob * .003 * std::sin (two() * t1) + wob * .0004 * std::sin (two() * t2)) * sr);
        const float n = nf.run (rng.next()) * hiss * .025f;
        auto chain = [this] (float x, int c) { x = std::clamp (hp.run (x, c) * 2, -1.0f, 1.0f); x = std::round (x * steps) / steps * .5f; return f.run (x, c); };
        a.write (chain (l, 0)); b.write (chain (r, 1));
        const float wl = a.read (dl) + n, wr = b.read (dl) + n;
        l = l * (1 - mix) + wl * mix; r = r * (1 - mix) + wr * mix;
    }
private:
    double sr, t1 = 0, t2 = 0; float steps, wob, hiss; Biquad hp, f, nf; DelayLine a, b; Noise rng;
};

class AutopanFx : public Fx
{
public:
    AutopanFx (const FxDesc& d, double rate) : style (d.style.empty() ? "sine" : d.style) { depth = d.get ("depth", .8f); inc = 1 / (std::max (.01f, d.get ("sec", .5f)) * rate); }
    int param (const std::string& k) const override { return k == "depth" ? 0 : -1; }
    void set (int, float v) override { depth = v; }
    void run (float& l, float& r) override
    {
        ph += inc; if (ph >= 1) ph -= 1;
        const float o = style == "square" ? (ph < .5 ? 1.0f : -1.0f) : style == "triangle" ? (float) (1 - 4 * std::abs (ph - .5)) : (float) std::sin (two() * ph);
        const float pan = std::clamp (o * depth, -1.0f, 1.0f), x = (pan + 1) * .5f * (float) synthPi * .5f, m = .5f * (l + r);
        l = m * std::cos (x); r = m * std::sin (x);
    }
private:
    std::string style; double ph = 0, inc; float depth;
};

class PhaserFx : public Fx
{
public:
    PhaserFx (const FxDesc& d, double rate) : sr (rate)
    {
        n = std::clamp ((int) d.get ("stages", 4), 2, 8); hz = d.get ("rate", .35f); depth = d.get ("depth", .7f); center = d.get ("center", 900); fb = d.get ("fb", .4f); mix = d.get ("mix", .5f);
    }
    int param (const std::string& k) const override { return k == "rate" ? 0 : k == "depth" ? 1 : k == "center" ? 2 : k == "fb" ? 3 : k == "mix" ? 4 : -1; }
    void set (int i, float v) override { if (i == 0) hz = v; else if (i == 1) depth = v; else if (i == 2) center = v; else if (i == 3) fb = v; else if (i == 4) mix = v; }
    void run (float& l, float& r) override
    {
        ph += hz / sr; if (ph >= 1) ph -= 1;
        if (tick++ % 32 == 0)
        {
            const double f = std::min (sr * .49, center * std::pow (2.0, depth * 2400 * std::sin (two() * ph) / 1200)), w = two() * f / sr, al = std::sin (w) / (2 * .7), c = std::cos (w);
            for (int i = 0; i < n; ++i) { ap[(size_t) i].set (1 - al, -2 * c, 1 + al, 1 + al, -2 * c, 1 - al); }
        }
        float x = l + last[0] * fb, y = r + last[1] * fb;
        for (int i = 0; i < n; ++i) { x = ap[(size_t) i].run (x, 0); y = ap[(size_t) i].run (y, 1); }
        last[0] = x; last[1] = y;
        l = l * (1 - mix) + x * mix; r = r * (1 - mix) + y * mix;
    }
private:
    double sr, ph = 0; int n, tick = 0; float hz, depth, center, fb, last[2] { 0, 0 }; std::array<Biquad, 8> ap;
};

class WideFx : public Fx
{
public:
    WideFx (const FxDesc& d, double rate) : sr (rate)
    {
        const std::string a = d.style; c = a == "Subtle" ? 5.f : a == "Wide" ? 15.f : 9.f; dl = (a == "Subtle" ? .007f : a == "Wide" ? .012f : .009f) * (float) sr; dr = (a == "Subtle" ? .011f : a == "Wide" ? .019f : .014f) * (float) sr;
        mix = d.get ("width", .6f); len = .1f * (float) sr; x.prepare ((int) (sr * .3)); y.prepare ((int) (sr * .3)); slope = 1 - (float) std::pow (2.0, c / 1200.0);
    }
    int param (const std::string& k) const override { return k == "width" ? 0 : -1; }
    void set (int, float v) override { mix = v; }
    void run (float& l, float& r) override
    {
        const float in = .5f * (l + r); x.write (in); y.write (in);
        ph += 1 / len; if (ph >= 1) ph -= 1;
        auto tap = [this] (const DelayLine& d, float sign, float base)
        {
            float s = 0;
            for (int k = 0; k < 2; ++k)
            {
                float p = ph + k * .5f; if (p >= 1) p -= 1;
                const float g = std::sin ((float) synthPi * p);
                s += d.read (base + 16 + (sign > 0 ? p : 1 - p) * len * std::abs (slope)) * g * g;
            }
            return s;
        };
        const float wl = tap (x, 1, dl), wr = tap (y, -1, dr);
        l = l * (1 - mix) + wl * mix; r = r * (1 - mix) + wr * mix;
    }
private:
    double sr; float c, dl, dr, len, ph = 0, slope; DelayLine x, y;
};

class FxBank
{
public:
    static constexpr int master = 16;
    std::array<std::array<std::unique_ptr<Fx>, 3>, 17> slots;
    std::array<std::unique_ptr<Fx>, 17> eq;   // the strip's Smart EQ, before its inserts
    std::array<bool, 17> any {};

    Fx* at (int ch, int slot) const { return ch >= 0 && ch < 17 && slot >= 0 && slot < 3 ? slots[(size_t) ch][(size_t) slot].get() : nullptr; }
    void run (int ch, float& l, float& r) { if (auto& e = eq[(size_t) ch]) e->run (l, r); for (auto& f : slots[(size_t) ch]) if (f) f->run (l, r); }
};

inline std::shared_ptr<FxBank> makeFxBank (const std::vector<FxDesc>& list, double sr)
{
    auto b = std::make_shared<FxBank>();
    for (const auto& d : list)
    {
        std::unique_ptr<Fx> f;
        if (d.type == "filter") f = std::make_unique<FilterFx> (d, sr);
        else if (d.type == "sat") f = std::make_unique<SatFx> (d, sr);
        else if (d.type == "warm") f = std::make_unique<WarmFx> (d, sr);
        else if (d.type == "eq") f = std::make_unique<EqFx> (d, sr);
        else if (d.type == "echo") f = std::make_unique<EchoFx> (d, sr);
        else if (d.type == "plate") f = std::make_unique<PlateFx> (d, sr);
        else if (d.type == "chorus") f = std::make_unique<ChorusFx> (d, sr);
        else if (d.type == "trem") f = std::make_unique<TremFx> (d, sr);
        else if (d.type == "crush") f = std::make_unique<CrushFx> (d, sr);
        else if (d.type == "lofi") f = std::make_unique<LofiFx> (d, sr);
        else if (d.type == "autopan") f = std::make_unique<AutopanFx> (d, sr);
        else if (d.type == "phaser") f = std::make_unique<PhaserFx> (d, sr);
        else if (d.type == "wide") f = std::make_unique<WideFx> (d, sr);
        else if (d.type == "peq") { if (d.ch >= 0 && d.ch < 17) { b->eq[(size_t) d.ch] = std::make_unique<PeqFx> (d, sr); b->any[(size_t) d.ch] = true; } continue; }
        if (f && d.ch >= 0 && d.ch < 17 && d.slot >= 0 && d.slot < 3) { b->slots[(size_t) d.ch][(size_t) d.slot] = std::move (f); b->any[(size_t) d.ch] = true; }
    }
    return b;
}
}
