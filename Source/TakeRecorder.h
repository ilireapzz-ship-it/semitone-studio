#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

// Records the audio input to a mono 24-bit WAV (JUCE-free, tested by tests/take_test.cpp).
// push() runs on the audio thread and never blocks or allocates: it fills a ring buffer that
// a background thread writes to disk. A slow disk loses audio rather than glitching; dropped() says how much.
namespace st
{
class TakeRecorder
{
public:
    ~TakeRecorder() { stop(); }

    bool start (const std::string& path, double sampleRate)
    {
        stop();
        file = std::fopen (path.c_str(), "wb");
        if (file == nullptr) return false;
        sr = (int) sampleRate;
        ring.assign ((size_t) std::max (1024, sr * 8), 0.0f);   // 8 seconds of slack
        rd.store (0); wr.store (0); frames = 0; droppedFrames.store (0);
        writeHeader();
        running.store (true);
        worker = std::thread ([this] { drainLoop(); });
        return true;
    }

    bool active() const noexcept { return running.load (std::memory_order_acquire); }

    // audio thread: mixes the two input channels to mono
    void push (const float* L, const float* R, int n) noexcept
    {
        if (! active()) return;
        const size_t cap = ring.size();
        size_t w = wr.load (std::memory_order_relaxed);
        const size_t r = rd.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i)
        {
            const size_t next = (w + 1) % cap;
            if (next == r) { droppedFrames.fetch_add ((uint64_t) (n - i), std::memory_order_relaxed); break; }
            ring[w] = R != nullptr && R != L ? std::clamp (L[i] + R[i], -1.0f, 1.0f) : L[i];
            w = next;
        }
        wr.store (w, std::memory_order_release);
    }

    struct Result { uint64_t frames = 0, dropped = 0; };

    Result stop()
    {
        Result res;
        if (file == nullptr) return res;
        running.store (false, std::memory_order_release);
        if (worker.joinable()) worker.join();
        drain();
        finishHeader();
        std::fclose (file);
        file = nullptr;
        res.frames = frames; res.dropped = droppedFrames.load();
        return res;
    }

private:
    void drainLoop()
    {
        while (running.load (std::memory_order_acquire))
        {
            drain();
            std::this_thread::sleep_for (std::chrono::milliseconds (20));
        }
    }

    void drain()
    {
        const size_t cap = ring.size();
        size_t r = rd.load (std::memory_order_relaxed);
        const size_t w = wr.load (std::memory_order_acquire);
        uint8_t b[3];
        while (r != w)
        {
            const float s = std::clamp (ring[r], -1.0f, 1.0f);
            const int32_t v = (int32_t) (s * 8388607.0f);
            b[0] = (uint8_t) (v & 255); b[1] = (uint8_t) ((v >> 8) & 255); b[2] = (uint8_t) ((v >> 16) & 255);
            std::fwrite (b, 1, 3, file);
            ++frames;
            r = (r + 1) % cap;
        }
        rd.store (r, std::memory_order_release);
    }

    void put32 (uint32_t v) { uint8_t b[4] { (uint8_t) v, (uint8_t) (v >> 8), (uint8_t) (v >> 16), (uint8_t) (v >> 24) }; std::fwrite (b, 1, 4, file); }
    void put16 (uint16_t v) { uint8_t b[2] { (uint8_t) v, (uint8_t) (v >> 8) }; std::fwrite (b, 1, 2, file); }
    void writeHeader()
    {
        std::fwrite ("RIFF", 1, 4, file); put32 (36); std::fwrite ("WAVEfmt ", 1, 8, file);
        put32 (16); put16 (1); put16 (1); put32 ((uint32_t) sr); put32 ((uint32_t) sr * 3); put16 (3); put16 (24);
        std::fwrite ("data", 1, 4, file); put32 (0);
    }
    void finishHeader()
    {
        const uint32_t bytes = (uint32_t) (frames * 3);
        std::fseek (file, 4, SEEK_SET); put32 (36 + bytes);
        std::fseek (file, 40, SEEK_SET); put32 (bytes);
    }

    std::FILE* file = nullptr;
    std::thread worker;
    std::vector<float> ring;
    std::atomic<size_t> rd { 0 }, wr { 0 };
    std::atomic<bool> running { false };
    std::atomic<uint64_t> droppedFrames { 0 };
    uint64_t frames = 0;
    int sr = 48000;
};
}
