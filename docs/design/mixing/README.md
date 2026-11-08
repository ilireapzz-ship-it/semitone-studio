# Smart EQ, Calibrate, and Fix what I hear (agreed with Jamie 8 Nov 2026)

Pictures: `1-Smart-EQ.png`, `2-Calibrate.png`, `3-Fix-what-I-hear.png`. Exact values: `reference-*.dc.html`. Theme checks:
`../themes-check/TE-*`. Brain: **`data/mix-master.json`** (embedded as `mixMaster`, checked by `check_style_data.py`).

## The brain: `data/mix-master.json`
- **`engineers`:** 12 profiles, each with role (mix/master), the styles it suits, philosophy, priorities, moves (as ops), sources and confidence.
  Like producers, they're "X-style, based on how they're known to work". Nothing is copied, and low-confidence profiles say so on every card.
  - Mix: Andrew Scheps-style, Chris Lord-Alge-style, Tom Elmhirst-style, Serban Ghenea-style, Manny Marroquin-style, Shawn Everett-style,
    Mike Dean-style, Alan Meyerson-style (score).
  - Master: Bob Katz-style (K-System, dynamics), Bob Ludwig-style (translation, energy without squashing), Emily Lazar-style (artist's goal,
    colour and glue, clear target), Randy Merrill-style (low confidence: credits only, so modern-pop norms are used as a stand-in).
  - A **house** profile (best practice) covers anything else.
- **`bestFor`:** the best documented mix and master engineer for every style profile (all 26). It's the default in Calibrate, and the user can
  change it. It's also offered in the recipe as "mixed like · mastered like" (optional words after "produced like").
- **`bands`:** what each frequency zone means in plain words (sub, bass, warmth, mud, box, presence, harsh, sibilance, air).
- **`spectrum`:** per part role (from part-roles.json), where it lives, where to high-pass, what to watch, and its dynamics.
- **`interactions`:** the masking pairs to check (kick↔bass, vocal↔chords/pad/counter/arp/loop, chords↔pad, bass↔bass2, lead↔harmony,
  returns↔everything, the summed 200–400 Hz build-up).
- **`problems`:** 22 common problems in the user's words (muddy, boomy, thin, boxy, harsh, too much ess, dull, vocal buried, vocal too loud,
  cluttered, no punch, squashed, too wide, too narrow, washy, ringing, pops, noise/hum, uneven, bad on phone, too quiet, too loud). Each has
  what it usually is, how to find it, fixes, tools and limits.
- **`destinations`:** Streaming, Competitive pop, Club, Film, Broadcast (EBU R128), Podcast and Vinyl, each with a LUFS target, range, true peak
  and a note. These replace the old "Listen on" targets for the master meter.
- **`calibrate`:** the steps, the rules (level-matched A/B, never touch locks, max change per pass, every change is a switchable item with its
  reason and layer, re-runs are incremental) and the coach rules it serves.

## 1. Smart EQ (opens from the EQ curve on any strip, bus, return or master)
- **Pop-out panel** in the Mix drawer (⤢ makes it full screen; it can also float as a window in the desktop app). Inspired by the best modern
  EQs' workflow, but Semitone's own design and name.
- **Analyser:**
  - Real-time spectrum after the EQ (part colour, filled) and before it (grey).
  - One other part overlaid (dashed, choose with "Chords ▾", default = the biggest masker).
  - The **ideal corridor** for this role (green, dashed), from `spectrum` + the genre curve.
  - **Masking** zones hatched red where the other part covers this one while both play.
  - Zone names along the top (sub … air), with mud, harsh and sibilance tinted.
  - Log frequency and dB grid.
- **Bands:**
  - Click the curve to add a band, then drag it (⇧ fine, ⌥ to set Q). Up to 24 bands.
  - Types: Bell, Shelf, Cut (6–96 dB/oct), Notch, Tilt.
  - **Dynamic**: a band with a Range acts only when the frequency is loud (shown as a dotted range line on the node).
  - Stereo / Mid / Side / L / R per band.
- **The band panel** (pops up next to the selected band):
  - Freq, Gain, Q and Range knobs (the house LED-ring knob), plus a Threshold slider.
  - ◉ **Listen**: solo the band so you hear what you're cutting.
  - B (bypass) and 🔒 **Lock**: Calibrate never changes a locked band.
- **Band cards along the bottom:** number, type, value, a one-line *why* ("mud: chords + pad build-up"), and who set it (You, Smart, or the
  engineer style).
- **Smart side panel:**
  - **Suggest bands:** reads this part's role and the parts it interacts with, then proposes bands as green dashed ghosts with a reason.
    Click to keep, or drag to adjust first.
  - **Match reference…:** matches the tonal curve of a reference song or another part, gently (max ±4 dB per band).
  - **I hear…** chips (Muddy, Harsh, Too much ess, Boxy, Thin, Dull, Buried, Ringing, Pops, Too boomy): each one finds the spot and proposes a
    band.
  - **Checks for this part:** what's fine (✓), what needs a look (!), with one action each. "Fix in Chords" changes the *other* part when that's
    the right fix (masking).
- A/B with **level matching** on (louder never wins by accident), and Presets per role.
- Everything is one undo step. The native engine and export run the same EQ (port to `Source/SynthFx.h`, including dynamic bands and M/S).

### Multi-select, band groups and pro features (`4-Smart-EQ-multi-select-and-pro.png`, values in `reference-EQ2.dc.html`)
Checked against FabFilter Pro-Q 4's published feature list (Dec 2024). We take the ideas only, with our own design and names.
- **Select several bands:** ⇧-click nodes, drag a box on the display, or ⌘-click band cards.
  - Selected nodes fill in the part colour, and a dotted line links them.
  - The band panel becomes "**2 bands selected** · Low cut + De-ess" with ◉ solo-listen, B, 🔒 and ⌫ for all of them.
  - **Relative** knobs: Shift freq (semitones), Gain (dB), Width (×Q). They move each band by the same amount, and dragging any selected node
    moves them all.
  - Stereo/Mid/Side/L/R applies to all. ⌘A selects all bands, Esc clears.
- **Band groups:** "Save as group" names the selection (e.g. "Vocal clean-up"). The group shows as a tag on its band cards. Toggle it on or off as one,
  copy it to another part (bands keep their relative positions), or save it as a preset. Smart and Calibrate create groups too ("Elmhirst-style
  vocal" = HP + notch + air).
- **Toolbar row** above the display:
  - **Tools:** ↖ Edit, ✎ **Sketch** (draw a curve with the mouse and bands are created to follow it, at most 6), ⌖ **Grab** (drag a peak in the
    live spectrum to cut or boost it; a band appears there).
  - **Character:** Clean, Subtle (transformer-style), Warm (tube-style). It's light saturation, and the native engine must match it.
  - **Phase:** Zero latency (default while recording), Natural, Linear (default when mastering). The latency shows in the strip, and `LineUp`
    compensates.
  - **Scale:** Hz, **Notes** (frequency axis in note names, e.g. C4, so a songwriter can read it; the analyser shows the song's key notes as
    ticks), and ±6 dB (mastering zoom; the normal range is ±18 dB).
- **More filters:** Bell, Shelf (low/high), Cut (6–96 dB/oct, plus **fractional slopes** and **brickwall**), Notch, Band pass, Tilt, **All pass** (phase
  alignment, e.g. kick vs bass), and **Flat tilt**. Up to 24 bands.
- **Dynamic bands:** Range, Threshold, **Attack/Release** (shown on the card: "A5 R80"), and **sidechain filter**.
  - **Spectral** mode only acts on the frequencies inside the band that exceed the threshold. It's the default for de-ess and harshness
    fixes, and its card says "spectral".
- **Triggered by another part** (external sidechain): a dynamic band keyed from another part, e.g. on Chords "dip 3 dB at 1.2–4 kHz only while
  the Melody sings". Set it with "Key: Melody ▾". "Fix in Chords" from Checks creates exactly this. The panel on the Melody EQ links to it with
  Open on Chords.
- **All EQs in the song:**
  - A list of every EQ (parts, buses, returns, master), each with a mini curve and its overlaps with the open EQ tinted red.
  - Click one to jump to it, ⌘-click to overlay it on this display.
  - Copy curve / Paste to… (with a choice: replace or add bands), and Open all (a grid of all EQs).
- **Workflow:**
  - Drag or scroll on any value, and double-click to type it.
  - MIDI Learn on every knob (uses the Controller map).
  - Undo/redo inside the EQ, sample-accurate automation of every band parameter (shows up in the Automation tab), and resizable/full screen.
  - Surround is out of scope, since Semitone is stereo.
- Tests:
  - Multi-select relative moves keep spacing.
  - A group copies to another part with relative positions.
  - Sketch creates ≤ 6 bands within 1 dB of the drawn curve; Grab adds a band at the grabbed peak.
  - Spectral and dynamic bands are inactive below threshold; an external key triggers the band.
  - Linear phase latency is compensated, and the Notes scale is accurate to the cent.
  - The native engine matches the page for every filter type, character mode and phase mode.

## 2. Calibrate (Mix tab → Calibrate; only when the user presses it)
- Header pickers:
  - **Mix like** (default from `bestFor`, e.g. Tom Elmhirst-style for the Ultraviolence era).
  - **Master like** (e.g. Emily Lazar-style).
  - **For** (destination from `destinations`).
  - **How far** (gentle · balanced · bold, which scales the limits).
- **Steps bar:** gain staging → clean-up → how parts interact → tone → dynamics → space → rides → master. It shows progress while it listens
  (it renders the song offline, so it works on everything together, not part by part).
- **Tonal balance:** the whole mix before (grey) and after (brass) against the **ideal range for the style** (green corridor), with numbered
  markers where the changes are.
- **Four readouts:** low end, mud 250–500 Hz, vocal vs music, loudness · crest (before → after).
- **What changes, and why:** every change is a numbered row with a reason and a switch. Tap a row to hear just that change (solo A/B).
  Changes go to the real places: EQ bands (tagged with the engineer), compressors, sends, rides (as automation, from the emotion curve) and bus
  settings.
- **Keep N changes**, or **Then master it →** (the same flow with the master steps and the destination meter).
- Locks are respected and listed ("🔒 Locked by you: …"). Re-running only changes what moved since last time.
- Mastering also shows the destination bars (from the mixer master card), true peak, crest guard, and **Listen on** checks (phone, earbuds,
  car, laptop simulations).

## 3. Fix what I hear (Mix tab → Fix what I hear, also from ⌘K and the EQ's "I hear…")
- Type it in plain words ("it sounds a bit muddy in the chorus, not sure where") or tap a chip. The text maps to `problems` by their `words`, and
  section words narrow where to look.
- **Found it:** a time × frequency heat map of the problem across the song, with the problem zone boxed and labelled ("260–500 Hz · +5 dB over
  ideal · both choruses"), plus ◉ **Listen to just this band** and Compare with ideal.
- **Who's adding to it:** each part's share of the problem energy, with Solo.
- **What's happening:** one plain paragraph.
- **Three ways to fix it:** usually a gentle EQ fix (weighted by the chosen engineer, marked recommended), a structural fix (e.g. ownership of
  the low end), and an **arrangement fix** from the coach (e.g. thin the chords). Each has ▶ Hear it and Keep, and each is one undo step.
- "Not quite? Tell me more" refines it ("only on the second chorus", "it's the bass", "keep the pad warm").
- Detectors: per-part band energy over time, masking overlap while both sound, narrow sustained peaks (ringing), s/t events on vocals, low
  transients (plosives), crest factor, stereo correlation and mono loss, noise floor and hum harmonics, and the section loudness curve. They run
  in the page for the web version and in native code in the app (offline render).

## Tests (tests/web/mixing.test.js + synth_test)
- Every problem's words map to it.
- A synthetic muddy chorus finds the right zone and parts; a masked vocal finds the masker; sibilance is detected on a vocal take.
- Calibrate respects locks and limits and is level-matched; every change is undoable and switchable.
- Destination targets are met within ±1 LU with true peak ≤ target.
- The EQ (including dynamic and M/S bands) sounds the same in the page and the engine.
- Every engineer in `bestFor` exists; low-confidence profiles show their note.
