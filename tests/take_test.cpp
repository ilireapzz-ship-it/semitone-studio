// Unit test for st::TakeRecorder (no JUCE needed): clang++ -std=c++17 -O2 -ISource tests/take_test.cpp -pthread -o t && ./t
#include "TakeRecorder.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>

static std::vector<uint8_t> read (const char* p) { std::ifstream f (p, std::ios::binary); return { std::istreambuf_iterator<char> (f), {} }; }
static uint32_t u32 (const std::vector<uint8_t>& d, size_t o) { return d[o] | d[o+1] << 8 | d[o+2] << 16 | (uint32_t) d[o+3] << 24; }
static int32_t s24 (const std::vector<uint8_t>& d, size_t o) { int32_t v = d[o] | d[o+1] << 8 | d[o+2] << 16; return v & 0x800000 ? v - 0x1000000 : v; }

int main()
{
    const char* path = "take-test.wav";
    st::TakeRecorder r;
    assert (! r.start ("/no/such/folder/x.wav", 48000));
    assert (r.start (path, 48000) && r.active());
    std::vector<float> L (480), R (480);
    size_t pos = 0;
    for (int b = 0; b < 200; ++b)
    {
        for (int i = 0; i < 480; ++i, ++pos) { L[(size_t) i] = .4f * (float) std::sin (pos * .01); R[(size_t) i] = .1f; }
        r.push (L.data(), R.data(), 480);
        std::this_thread::sleep_for (std::chrono::microseconds (200));
    }
    auto res = r.stop();
    assert (! r.active() && res.frames == 96000 && res.dropped == 0);
    r.push (L.data(), R.data(), 480);    // after stop: ignored
    auto d = read (path);
    assert (d.size() == 44 + 96000 * 3 && std::memcmp (d.data(), "RIFF", 4) == 0 && std::memcmp (d.data() + 8, "WAVE", 4) == 0);
    assert (u32 (d, 4) == d.size() - 8 && u32 (d, 40) == 96000 * 3 && u32 (d, 24) == 48000);
    for (size_t i = 0; i < 96000; i += 997)
    {
        const double want = .4 * std::sin ((double) i * .01) + .1, got = s24 (d, 44 + i * 3) / 8388607.0;
        assert (std::abs (want - got) < 1e-3);
    }
    std::printf ("ok: a take is written whole, mixed to mono, sample for sample\n");

    assert (r.start (path, 1000));                 // tiny ring, nobody draining fast enough: must drop, not block
    std::vector<float> big (100000, .2f);
    r.push (big.data(), nullptr, 100000);
    res = r.stop();
    assert (res.dropped > 0 && res.frames + res.dropped == 100000 - 1 + 1);
    std::printf ("ok: a stalled disk loses audio and says so, without blocking the audio thread\n");

    // stereo: input 1 left, input 2 right, interleaved; header says 2 channels
    assert (r.start (path, 44100, st::TakeRecorder::stereo) && r.channels() == 2);
    for (int i = 0; i < 480; ++i) { L[(size_t) i] = .5f; R[(size_t) i] = -.25f; }
    r.push (L.data(), R.data(), 480);
    assert (std::abs (r.takePeak() - .5f) < 1e-6 && r.takePeak() == 0);
    res = r.stop();
    d = read (path);
    assert (res.frames == 480 && d[22] == 2 && u32 (d, 40) == 480 * 6 && d[32] == 6 && u32 (d, 28) == 44100 * 6);
    assert (std::abs (s24 (d, 44) / 8388607.0 - .5) < 1e-3 && std::abs (s24 (d, 47) / 8388607.0 + .25) < 1e-3);
    std::printf ("ok: stereo takes keep left and right apart\n");

    // one input of two: a guitar on input 2 records alone, in mono
    assert (r.start (path, 48000, st::TakeRecorder::in2) && r.channels() == 1);
    r.push (L.data(), R.data(), 480);
    res = r.stop();
    d = read (path);
    assert (res.frames == 480 && d[22] == 1 && std::abs (s24 (d, 44) / 8388607.0 + .25) < 1e-3);
    // stereo from a one-input device: both sides get that input
    assert (r.start (path, 48000, st::TakeRecorder::stereo));
    r.push (L.data(), nullptr, 480);
    r.stop();
    d = read (path);
    assert (std::abs (s24 (d, 47) / 8388607.0 - .5) < 1e-3);
    std::printf ("ok: input 1, input 2 or both, as the track asks\n");
    std::remove (path);
    std::printf ("all take tests passed\n");
}
