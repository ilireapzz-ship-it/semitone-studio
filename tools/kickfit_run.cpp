// Runs Kick Fit's processing on raw sound files, for the test bench and the listening test (tools/kickfit_bench.py).
//   kickfit_run kick.f32 side.f32 out.f32 rate tune thump punch click tail level [sitLo sitMid sitHi]
// Files are mono 32-bit floats. "-" for side = no sidechain. With the three sit numbers it does what the Sit it button does:
// plays everything once to measure, sets the level, then plays it again. Prints one line of JSON with what it found.
// Build: cl /std:c++17 /O2 /EHsc /Ikickfit tools\kickfit_run.cpp   (or clang++ -std=c++17 -O2 -Ikickfit tools/kickfit_run.cpp)
#include "KickFit.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static std::vector<float> load (const char* path)
{
    std::vector<float> v; FILE* f = std::fopen (path, "rb"); if (! f) return v;
    std::fseek (f, 0, SEEK_END); const long n = std::ftell (f) / 4; std::fseek (f, 0, SEEK_SET);
    v.resize ((size_t) n); if (n > 0 && std::fread (v.data(), 4, (size_t) n, f) != (size_t) n) v.clear();
    std::fclose (f); return v;
}

int main (int argc, char** argv)
{
    if (argc < 11) { std::fprintf (stderr, "see the top of tools/kickfit_run.cpp\n"); return 2; }
    const auto kick = load (argv[1]); const bool hasSide = std::strcmp (argv[2], "-") != 0;
    auto side = hasSide ? load (argv[2]) : std::vector<float>();
    if (kick.empty() || (hasSide && side.empty())) { std::fprintf (stderr, "could not read the input\n"); return 2; }
    side.resize (kick.size(), 0.0f);
    const double sr = std::atof (argv[4]);
    kf::Params p; p.tune = (float) std::atof (argv[5]); p.thump = (float) std::atof (argv[6]); p.punch = (float) std::atof (argv[7]);
    p.click = (float) std::atof (argv[8]); p.tail = (float) std::atof (argv[9]); p.gainDb = (float) std::atof (argv[10]);
    const bool sit = argc >= 14;
    double before = 0, after = 0, change = 0, note = 0, spread = 0; bool measured = false, moved = false; int latency = 0; float peak = 0;
    std::vector<float> l, r;
    for (int pass = 0; pass < (sit ? 2 : 1); ++pass)
    {
        kf::Engine e; e.setParams (p); e.prepare (sr);
        l = kick; r = kick;
        for (size_t i = 0; i < l.size(); i += 512)
        {
            const int n = (int) std::min<size_t> (512, l.size() - i);
            e.process (l.data() + i, r.data() + i, hasSide ? side.data() + i : nullptr, hasSide ? side.data() + i : nullptr, n);
        }
        note = e.note(); spread = e.noteSpreadCents(); latency = e.latency();
        double now = 0; const bool ok = e.sitLevel (now);
        if (pass == 0)
        {
            measured = ok; before = after = now;
            if (sit && ok && e.sitGain (std::atof (argv[11]), std::atof (argv[12]), std::atof (argv[13]), change)) { moved = change != 0; p.gainDb += (float) change; }
            if (! moved) break;
        }
        else after = now;
    }
    for (float v : l) peak = std::max (peak, std::abs (v));
    if (FILE* f = std::fopen (argv[3], "wb")) { std::fwrite (l.data(), 4, l.size(), f); std::fclose (f); }
    std::printf ("{\"note\": %.3f, \"spread\": %.1f, \"latency\": %d, \"measured\": %s, \"before\": %.2f, \"after\": %.2f, \"change\": %.2f, \"peak\": %.4f}\n",
                 note, spread, latency, measured ? "true" : "false", before, after, change, peak);
    return 0;
}
