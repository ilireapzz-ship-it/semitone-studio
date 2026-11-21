#pragma once
// How Kick Fit tells Fit Bus what it is sitting on. No JUCE in it.
// The two are separate plug-in files, so they cannot share ordinary variables: they share a small named block of memory.
// Each Kick Fit takes a slot and keeps writing: what it is on (kick, bass, vocal), how strong its output is in six ranges
// of pitch, its recent peak and its Level. Fit Bus reads the slots, and may leave a request ("change Level by this much")
// that the Kick Fit carries out. Numbers only: no sound crosses.
#include "KickFit.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#ifdef _WIN32
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#else
 #include <fcntl.h>
 #include <sys/mman.h>
 #include <unistd.h>
#endif

namespace fl
{
constexpr int numBands = 6, maxSlots = 8;
constexpr double bandEdges[numBands - 1] = { 60, 150, 400, 1500, 5000 };      // upper edge of each range, Hz

// Strength in the six ranges, in 20 ms steps, smoothed over about a third of a second; and the recent peak (fades over ~4 s).
// Kick Fit runs one on its output and Fit Bus one on its input, the same way, so the two can be compared.
struct BandMeter
{
    void prepare (double sr)
    {
        for (int i = 0; i < numBands - 1; ++i) { a[i].lowpass (bandEdges[i], 0.5412, sr); b[i].lowpass (bandEdges[i], 1.3066, sr); a[i].reset(); b[i].reset(); }
        step = std::max (1, (int) (0.02 * sr)); n = 0; top = 0; peak = 0; for (auto& v : e) v = 0; for (auto& v : power) v = 0; for (auto& v : upTo) v = 0;
        smooth = 1 - std::exp (-0.02 / 0.3); fade = std::exp (-0.02 / 4.0);
    }
    // one sample (mono, and the louder side's size). true when a 20 ms step has just finished and the numbers are fresh.
    bool push (double mono, double size)
    {
        for (int i = 0; i < numBands; ++i) { const double upTo = i < numBands - 1 ? b[i].run (a[i].run (mono)) : mono; e[i] += upTo * upTo; }
        top = std::max (top, size);
        if (++n < step) return false;
        // e[i] is everything up to range i's upper edge. Smooth those first and take the differences afterwards: inside one 20 ms
        // step the filters' delays can make a difference negative, and cutting that off there would bend the numbers.
        for (int i = 0; i < numBands; ++i) upTo[i] += smooth * (e[i] / n - upTo[i]);
        for (int i = 0; i < numBands; ++i) power[i] = std::max (0.0, upTo[i] - (i ? upTo[i - 1] : 0.0));
        for (auto& v : e) v = 0;
        stepPeak = top; peak = std::max (top, peak * fade); top = 0; n = 0; return true;
    }
    double power[numBands] {}, upTo[numBands] {}, peak = 0, stepPeak = 0;
private:
    kf::Biquad a[numBands - 1], b[numBands - 1]; double e[numBands] {}, top = 0, smooth = 0.06, fade = 0.995; int step = 882, n = 0;
};

// A part's usual peak while it plays, the way tools/kick_analyse.py measures a stem: the peak of every 0.1 s, the quiet
// stretches left out (under a tenth of the loudest twentieth), then the middle one. Over the last 12 s.
struct UsualPeak
{
    void prepare (double sr) { step = std::max (1, (int) (0.1 * sr)); n = 0; top = 0; w = 0; count = 0; value = 0; }
    void push (double size)
    {
        top = std::max (top, size); if (++n < step) return;
        ring[w] = (float) top; w = (w + 1) % size_; count = std::min (count + 1, size_); top = 0; n = 0;
        float v[size_]; for (int i = 0; i < count; ++i) v[i] = ring[i];
        std::sort (v, v + count); const float floor = 0.1f * v[(int) (0.95 * (count - 1))];
        int from = 0; while (from < count && v[from] <= floor) ++from;
        value = count - from >= 5 ? v[from + (count - from) / 2] : 0.0;
    }
    double value = 0;            // 0 = not enough playing yet
private:
    static constexpr int size_ = 120; float ring[size_] {}; double top = 0; int step = 4410, n = 0, w = 0, count = 0;
};

struct Slot
{
    std::atomic<uint32_t> owner;                 // 0 = free
    std::atomic<int32_t> role;                   // 0 kick, 1 bass, 2 vocal
    std::atomic<int64_t> seenMs;                 // when it last wrote
    std::atomic<float> power[numBands], peak, usual, level;      // peak: the recent highest (a kick's hits); usual: see UsualPeak
    std::atomic<float> askDb; std::atomic<uint32_t> askSeq;      // from Fit Bus: change Level by askDb (a new askSeq = a new request)
};
struct Block { Slot slot[maxSlots]; };

inline int64_t nowMs() { return std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::steady_clock::now().time_since_epoch()).count(); }

// The shared block, or null when the system won't give one (then the two plug-ins simply don't see each other).
inline Block* shared()
{
    static Block* block = [] () -> Block*
    {
       #ifdef _WIN32
        HANDLE h = CreateFileMappingA (INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, (DWORD) sizeof (Block), "Local\\SemitoneFitLink2");
        if (h == nullptr) return nullptr;
        return static_cast<Block*> (MapViewOfFile (h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof (Block)));          // new memory starts as zeros: every slot free
       #else
        const int fd = shm_open ("/semitone-fitlink2", O_CREAT | O_RDWR, 0600);
        if (fd < 0) return nullptr;
        if (ftruncate (fd, (off_t) sizeof (Block)) != 0) { /* already sized by another plug-in */ }
        void* p = mmap (nullptr, sizeof (Block), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); close (fd);
        return p == MAP_FAILED ? nullptr : static_cast<Block*> (p);
       #endif
    }();
    return block;
}
// A slot for this Kick Fit: a free one, or one whose owner stopped writing 5 s ago (a crash, a closed project). -1 = none left.
inline int claim (Block* b, uint32_t id)
{
    if (b == nullptr) return -1;
    const int64_t t = nowMs();
    for (int i = 0; i < maxSlots; ++i)
    {
        uint32_t cur = b->slot[i].owner.load();
        if (cur != 0 && t - b->slot[i].seenMs.load() < 5000) continue;
        if (b->slot[i].owner.compare_exchange_strong (cur, id)) { b->slot[i].seenMs.store (t); return i; }
    }
    return -1;
}
inline void release (Block* b, int slot, uint32_t id) { if (b != nullptr && slot >= 0) b->slot[slot].owner.compare_exchange_strong (id, 0); }
inline bool alive (const Slot& s) { return s.owner.load() != 0 && nowMs() - s.seenMs.load() < 1000; }
}
