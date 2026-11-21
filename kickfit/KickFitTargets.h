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
    bool vocal; float vocLo, vocMid, vocHi;   // the singing against everything else (K-weighted), dB: quarter, middle, three quarters of the songs
    bool peaks; float kickPk[3], bassPk[3], vocPk[3];   // each part's usual peak against the song's loudest peak, dB (quarter, middle, three quarters); full songs, parts split by Demucs
};
inline constexpr ModeTarget modeTargets[] = {
    { "trap-hiphop", "Trap / hip-hop", 0, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f, false, 0.00f, 0.00f, 0.00f, false, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
    { "pop", "Pop", 53, true, -12.13f, -2.96f, -0.88f, 48.8f, 52.8f, 72.0f, -0.12f, true, -1.57f, 1.25f, 3.30f, true, { -17.61f, -4.55f, -2.07f }, { -17.11f, -13.30f, -10.23f }, { -9.34f, -7.85f, -5.17f } },
    { "rnb", "R&B", 2, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f, false, 0.00f, 0.00f, 0.00f, false, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
    { "house-edm", "House / EDM", 0, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f, false, 0.00f, 0.00f, 0.00f, false, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
    { "dnb", "Drum & bass", 0, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f, false, 0.00f, 0.00f, 0.00f, false, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
    { "rock", "Rock", 242, true, -3.69f, -2.10f, -1.14f, 44.0f, 50.0f, 60.2f, -0.56f, true, -2.54f, -0.84f, 1.26f, true, { -5.17f, -2.65f, -1.47f }, { -14.32f, -11.25f, -9.16f }, { -7.80f, -6.33f, -4.86f } },
    { "afro", "Afrobeats / amapiano", 0, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f, false, 0.00f, 0.00f, 0.00f, false, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
    { "lofi", "Lo-fi", 0, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f, false, 0.00f, 0.00f, 0.00f, false, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
};
inline constexpr int numModes = 8;
// from 1947 kick samples: what is left of a kick above 180 Hz (what a phone plays), dB against the whole hit
inline constexpr float kickAbove180P10 = -23.36f, kickAbove180Median = -14.50f;
}
