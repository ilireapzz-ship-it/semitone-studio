#pragma once
// Made by tools/kickfit_targets.py from data/kickfit-targets.json (measured 2026-11-21). Do not edit by hand.
namespace kf
{
struct ModeTarget
{
    const char* id; const char* name; int songs; bool measured;
    float sitLo, sitMid, sitHi;        // kick lows (30-150 Hz) against the whole mix at kick hits, dB: quarter, middle, three quarters of the songs
    float noteLo, noteMid, noteHi;     // the kick's boom note in those songs, Hz (0 = too few songs with a steady note)
    float dipDb;                       // how far the bass's lows drop under the kick in those songs, dB (middle)
};
inline constexpr ModeTarget modeTargets[] = {
    { "trap-hiphop", "Trap / hip-hop", 0, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f },
    { "pop", "Pop", 53, true, -18.67f, -5.39f, -2.47f, 48.7f, 51.6f, 60.9f, -0.14f },
    { "rnb", "R&B", 2, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f },
    { "house-edm", "House / EDM", 0, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f },
    { "dnb", "Drum & bass", 0, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f },
    { "rock", "Rock", 242, true, -6.04f, -4.14f, -3.07f, 44.1f, 50.5f, 60.2f, -0.55f },
    { "afro", "Afrobeats / amapiano", 0, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f },
    { "lofi", "Lo-fi", 0, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f },
};
inline constexpr int numModes = 8;
// from 1947 kick samples: what is left of a kick above 180 Hz (what a phone plays), dB against the whole hit
inline constexpr float kickAbove180P10 = -23.36f, kickAbove180Median = -14.50f;
}
