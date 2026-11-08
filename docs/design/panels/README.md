# Song settings, Record, and the mixer with your DAW built in (agreed with Jamie 8 Nov 2026)

Pictures: `1-Song-settings.png`, `2-Record.png`, `3-Mixer-with-your-DAW.png`. Exact values: `reference-*.dc.html` (design-canvas files: copy
values, not markup). Theme checks: `../themes-check/`.

Before: Style & settings was a long column of full-width dropdowns; Record & listen was a loose stack of buttons; DAW & MIDI was a lone "Connect
MIDI out" button with set-up text. The Sound drawer sub-tabs become **Library · Emotion · Style & settings · Record**, and **DAW & MIDI goes
away as a tab**: what it did moves into the mixer and Share.

## 1. Song settings (was Style & settings)
Two columns of cards, no page scroll at 1440×900:
- **Style:** a monogram tile, the style name in serif, its one-line description and producer, tag chips, and **Change style** (opens the tag picker).
  Project Save/Open/New move to the logo menu and ⌘S/⌘O (the title is already in the header).
- **Structure:** eight picture cards (Single idea, Verse–chorus, Pop + bridge, Ballad, AABA, Hook–verse loop, Epic, Surprise me). Each draws its
  shape as coloured blocks, and the current one is ringed in brass. The header shows sections · bars · time · length ▾ and ⟳ Shuffle.
- **Key:** a **circle of fifths** (majors outside, the relative minors near the key inside, the current key filled brass, its relative major ringed)
  with "E minor · 1 sharp" in the middle. Beside it, the **mode** list, each with its feeling (bright, sad, hopeful minor, dark, dreamy, bluesy,
  dramatic, unstable).
- **Time:** a big tempo readout with − / ＋, Tap tempo, ½×, 2×; metre as a segmented control; **tempo for this style** as a range bar with the
  current tempo marked and one line on what slower/faster feels like; and **song length** (1 · 3 · 5 · 8 · 10+ min, from song-shapes.json).
- **Feel:** four LED-ring knobs: Swing, Melody (sparse → busy), Leaps (steps → leaps), Quirks (clean → quirky). Each shows a value and its range words.
  Then **chord rhythm** as six pattern cards, each drawing its hits (Sustained, On the beat, Driving 8ths, Push 3+3+2, Stabs, Strum).
- **Performance:** six switches with one-line descriptions (voice-led chords, human drummer, vary repeats, seventh chords, sustain pedal,
  bass pattern ▾).
- **Energy & peaks:** the song's energy curve (brass, filled) with the melody's peak line (dashed) across the sections, the peak note and the
  chorus lift in semitones, plus Final chorus lift (Off / +1 / +2).
Every change is one undo step, and "changes write a fresh idea with the same seed" stays true.

## 2. Record (was Record & listen)
- **Left column:**
  - **Input:** device menu, a gain knob, a 24-segment level meter with a plain verdict ("−12 dB peak · good level"), switches for Hear myself,
    Low-cut, Mono and Pre-roll, and the latency line (auto-aligned).
  - **MIDI keyboard:** device and status, a mini keyboard that lights the keys being played, Computer keys, Snap ▾ and Controller ◎ (the controller
    map from Automation).
  - **Listen & analyse:** Listen, Analyse a file…, and a drop zone.
- **What are you recording?** Four cards: **Hum a melody** (becomes notes in the key), **Play keys** (MIDI with knob moves, see Automation →
  Record with your controller), **Voice or instrument** (audio takes), **Import a take**. The row below holds backing, count-in, Snap to the key, and
  a big **● Record (R)**. "into Verse 1 · from bar 5" says where it lands.
- **Takes:** waveform rows for the selected section and part, with ruler bars. **Swipe across takes to build one comp**: the chosen stretches are
  outlined in the part colour, and the **Comp** row shows what plays ("Take 3 · bars 5–7"). The take being recorded draws live in red. Each take has
  ▶ and ★, and **Make melody from comp** turns it into notes.
- **Options:** Punch in (bars, set by clicking the ruler), Loop record (each pass a new take), Tuning (Off, Natural, Tight; never robotic by default),
  and Timing (ms shift).
The desktop app records natively (TakeRecorder). Inside a DAW, recording stays with the DAW, and this panel says so and offers Import a take.

## 3. The mixer, with your DAW built in (replaces the DAW & MIDI tab)
- **Every strip says what it plays through**, at the top: "Semitone · Dreamy pad (built-in)", "Serum 2 (your plug-in · in Semitone)",
  "→ FL Studio · Ch 3 (MIDI out · Rhodes in FL)", or "Audio · Take 3". Clicking it changes the sound source: built-in sounds, your plug-ins,
  or **send to your DAW** on a channel.
- A strip sent to the DAW is outlined in brass. Its fader area is hatched with "mix this part in FL Studio" (Semitone isn't making that sound), and
  its inserts are empty.
- Strips have inserts (empty slots dashed), Pan and Room knobs (the same LED-ring knob), a fader with metal cap, a stereo meter, dB readout and
  M / S / ● (record arm).
- **Master card:** LED-ring master knob, LUFS, Polish, Limiter and Listen on.
- **Your DAW card** (brass):
  - The DAW and port ("FL Studio 2026 · MIDI out · port 'Semitone' (IAC)", Port ▾) with a linked status.
  - One row per part with its channel and an on/off switch, shared with the strips' "plays through".
  - Mute Semitone's sound for parts sent out, Send clock & song position, Test each channel, and **Set-up guide** (the FL/loopMIDI/IAC steps
    from the old tab, as a step-by-step sheet).
- **Send to your DAW card:** Drag a section (audio, ⌥ for MIDI), Stems & MIDI (in Share), Markers in FL. These are the old tab's drag and export
  features, now next to the routing.
- Inside the FL plug-in, "Your DAW" shows FL as the host and the routing uses the plug-in's own MIDI output, with no virtual port needed.

## Every design in every theme
- The `themes-check/` folder has the macro panel in all five themes, plus part knobs, controller recording, the mixer, Song settings and Record in
  light and warm themes.
- **Rule found while checking:** anything drawn on top of a panel must use a token, never a fixed dark colour. That covers the "mix this part in FL
  Studio" overlay (it stayed dark in Daylight until changed), unlit LED segments and slider wells (`--st-inset`/well token), and segmented-control
  selections.
- **Soft inks:** the pale accent text colours (part-colour tints like the pink take names, green "good level", red "recording") are unreadable on Riso
  and Daylight paper (`problem-soft-inks-Riso.png`). Add `inkSoft` per part and status colour to themes.json. Dark themes keep the pale tints, and
  light themes use deeper versions that reach 4.5:1.
- Unlit knob segments, chord-rhythm hits and structure blocks use alpha tints of the part colour, so they work on any background.
- Tests: `themes.test.js` renders Automation, the macro panel, the part knobs, Song settings, Record and the mixer in every theme and checks text
  contrast ≥ 4.5:1 on every visible label.
