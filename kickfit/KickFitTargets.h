#pragma once
// Made by tools/kickfit_targets.py from data/kickfit-targets.json (measured 2026-11-22). Do not edit by hand.
namespace kf
{
struct ModeTarget
{
    const char* id; const char* name; int songs; bool measured;
    float sitLo, sitMid, sitHi;        // kick lows (30-150 Hz) against everything under 300 Hz at kick hits, dB: low edge, middle, high edge (see sit_range)
    float noteLo, noteMid, noteHi;     // the kick's boom note in those songs, Hz (0 = too few songs with a steady note)
    float dipDb;                       // how far the bass's lows drop under the kick in those songs, dB (middle)
    bool vocal; float vocLo, vocMid, vocHi;   // the singing against everything else (K-weighted), dB: quarter, middle, three quarters of the songs
    bool peaks; float bassVsKick[3], vocVsKick[3];   // the bass's / the vocal's usual peak against the kick's, dB, song by song (quarter, middle, three quarters); parts split by Demucs
    bool tone; float toneDb[6][3];   // tone balance of the finished songs: share of the sound in each of Fit Bus's six ranges, dB against the whole (quarter, middle, three quarters)
    bool loud; float lufs[3];   // integrated loudness of the finished songs, LUFS (quarter, middle, three quarters)
};
inline constexpr ModeTarget modeTargets[] = {
    { "trap-hiphop", "Trap / hip-hop", 0, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f, false, 0.00f, 0.00f, 0.00f, false, { 0, 0, 0 }, { 0, 0, 0 }, false, { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, false, { 0.00f, 0.00f, 0.00f } },
    { "pop", "Pop", 53, true, -12.13f, -2.96f, -0.88f, 48.8f, 52.8f, 72.0f, -0.12f, true, -1.57f, 1.25f, 3.30f, true, { -13.22f, -8.61f, -5.47f }, { -5.21f, -3.38f, -1.16f }, true, { { -14.95f, -10.06f, -7.30f }, { -8.33f, -6.22f, -5.42f }, { -7.80f, -5.89f, -4.23f }, { -6.65f, -5.72f, -4.45f }, { -14.82f, -11.43f, -10.25f }, { -25.74f, -22.15f, -16.53f } }, true, { -13.01f, -10.12f, -7.86f } },
    { "rnb", "R&B", 2, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f, false, 0.00f, 0.00f, 0.00f, false, { 0, 0, 0 }, { 0, 0, 0 }, false, { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, false, { 0.00f, 0.00f, 0.00f } },
    { "house-edm", "House / EDM", 0, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f, false, 0.00f, 0.00f, 0.00f, false, { 0, 0, 0 }, { 0, 0, 0 }, false, { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, false, { 0.00f, 0.00f, 0.00f } },
    { "dnb", "Drum & bass", 0, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f, false, 0.00f, 0.00f, 0.00f, false, { 0, 0, 0 }, { 0, 0, 0 }, false, { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, false, { 0.00f, 0.00f, 0.00f } },
    { "rock", "Rock", 242, true, -3.69f, -2.10f, -1.14f, 44.0f, 50.0f, 60.2f, -0.56f, true, -2.54f, -0.84f, 1.26f, true, { -9.88f, -8.25f, -6.84f }, { -5.41f, -3.42f, -2.00f }, true, { { -12.64f, -9.89f, -7.97f }, { -6.91f, -5.69f, -4.55f }, { -7.60f, -6.80f, -5.92f }, { -7.31f, -6.17f, -4.93f }, { -13.43f, -11.12f, -9.09f }, { -21.26f, -18.26f, -15.79f } }, true, { -10.64f, -8.50f, -7.52f } },
    { "afro", "Afrobeats / amapiano", 0, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f, false, 0.00f, 0.00f, 0.00f, false, { 0, 0, 0 }, { 0, 0, 0 }, false, { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, false, { 0.00f, 0.00f, 0.00f } },
    { "lofi", "Lo-fi", 0, false, 0.00f, 0.00f, 0.00f, 0.0f, 0.0f, 0.0f, 0.00f, false, 0.00f, 0.00f, 0.00f, false, { 0, 0, 0 }, { 0, 0, 0 }, false, { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } }, false, { 0.00f, 0.00f, 0.00f } },
};
inline constexpr int numModes = 8;
// from 1947 kick samples: what is left of a kick above 180 Hz (what a phone plays), dB against the whole hit
inline constexpr float kickAbove180P10 = -23.36f, kickAbove180Median = -14.50f;
}
