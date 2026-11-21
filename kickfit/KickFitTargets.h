#pragma once
// Made by tools/kickfit_targets.py from data/kickfit-targets.json (measured 2026-11-21). Do not edit by hand.
namespace kf
{
struct ModeTarget
{
    const char* id; const char* name; int songs; bool measured;
    float sitLo, sitMid, sitHi;        // kick lows (30-150 Hz) against everything under 300 Hz at kick hits, dB: low edge, middle, high edge (see sit_range)
    float noteLo, noteMid, noteHi;     // the kick's boom note in those songs, Hz (0 = too few songs with a steady note)
    float dipDb;                       // how far the bass's lows drop under the kick in those songs, dB (middle)
};
inline constexpr ModeTarget modeTargets[] = {
    { "trap-hiphop", "Trap / hip-hop", 0, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f },
    { "pop", "Pop", 53, true, -12.47f, -3.01f, -0.88f, 48.9f, 52.1f, 61.6f, -0.10f },
    { "rnb", "R&B", 2, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f },
    { "house-edm", "House / EDM", 0, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f },
    { "dnb", "Drum & bass", 0, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f },
    { "rock", "Rock", 242, true, -3.60f, -2.09f, -1.12f, 43.7f, 50.5f, 60.3f, -0.59f },
    { "afro", "Afrobeats / amapiano", 0, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f },
    { "lofi", "Lo-fi", 0, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f },
};
inline constexpr int numModes = 8;
// from 1947 kick samples: what is left of a kick above 180 Hz (what a phone plays), dB against the whole hit
inline constexpr float kickAbove180P10 = -23.36f, kickAbove180Median = -14.50f;
}
