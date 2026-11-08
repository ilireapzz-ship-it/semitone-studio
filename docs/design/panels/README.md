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
- **Structure:** picture cards (Single idea, Verse–chorus, Verse–pre–chorus, + bridge, Refrain song, Ballad, AABA, Epic). Each draws its
  shape as coloured blocks, and the current one is ringed in brass. The header shows sections · bars · time · length ▾ and ⟳ Shuffle.
  - **Colour key** above the cards (and the same colours on the timeline's section pills): Intro / outro grey, Verse light blue, Pre-chorus
    mid blue, Chorus strong blue, Refrain brass, Bridge / break violet. They are alpha tints of theme tokens, so they work in every theme.
    Add a `sectionColors` token group to themes.json.
  - **Examples from the chosen artist or era** on every card: up to two song titles in italics ("Brooklyn Baby · West Coast +3") and a count
    ("5 of 10"). A card with none says "none in this era" and dims. Hovering or ⌥-clicking a card opens the full list, with "Use this shape"
    and "Hear its shape" (plays the song's own sections as a sketch with the current sounds, never the original recording).
  - **Where the examples come from:** today `progressions.json` lists the section types per song (verse, pre, chorus, bridge, refrain, outro), so
    classify by which types appear (verse only → Single idea; + chorus → Verse–chorus; + pre → Verse–pre–chorus; + bridge → + bridge; refrain →
    Refrain song). That's a guide, so the header says "Examples from <style>". For accuracy, add an optional `form` per song in catalog.json
    (the real section order, e.g. `"form": "I V P C V P C B C O"`, with a source). Use `form` when present. `check_style_data.py` checks the
    letters. Ballad, AABA and Epic need `form` or a length to be detected.
  - Example counts follow the selected style tags (a blend shows songs from each, marked by artist).
- **Key** (change it here as well as in the recipe; both are the same setting and one undo step):
  - A row of 12 tonic buttons (C … B) plus a **Major / Minor** switch for quick changes.
  - The **circle of fifths** is clickable too: majors outside, relative minors inside, the current key filled brass, and its relative major ringed.
  - Mode buttons in two columns, each with its feeling.
  - **Green dots** on tonic buttons and circle keys mark keys this era's songs use.
  - **"Lana Del Rey songs in E minor"**: chips of the artist's songs in the chosen key, the current era's first and highlighted ("Old Money · UV"),
    then other eras (Born to Die, Off to the Races, Nectar of the Gods), with "1 of 10 in Ultraviolence" and the era's most-used keys
    ("D minor (3) · F (2) · E (2)").
  - Hovering a chip shows the song's key, tempo and progression from progressions.json. Clicking it offers "Use this key" or "Hear its chords in
    your sound" (a sketch, never the recording).
  - Data comes from `progressions.json` `key` per song. Parse "D minor / F major" as both, and modal keys ("E mixolydian") as their tonic,
    showing the mode. Blended styles show songs from each tag's artist.
  - **Changing the key:** transposes the song (notes, chords and lyrics stay, everything else follows). It asks once whether to move the melody
    up or down (nearest by default), and warns if the melody leaves the comfortable range set in Pitch.
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

### Click track (like FL Studio's metronome, `2-Record.png` shows the menu open)
- **One button in the transport**, always visible in every lens: a metronome icon + "Click", with a ▾. Click it (or press **C**) to turn it on or off.
  When on it is lit brass and says when it plays: "Click · rec" (only while recording) or "Click" (always). **⇧C** cycles the "when" setting.
- **The ▾, or a right-click on the button,** opens the Click menu. It's small and in plain words, and the same menu is on Record's "Click" segment
  (Off / Recording / Always):
  - **When:** Off · Only while recording · Always · Count-in only.
  - **Sound:** Wood (default), Classic beep, Rim, Hi-hat, Shaker, Cowbell, Song's kit (uses the song's own rim or hat), and **Your sample…** (a dashed
    card: drop a WAV/AIFF or choose a file, optionally a second sample for the accent, trimmed to its start, kept with your settings, not the
    song). Each card shows the sound's shape and plays it on hover.
  - **Pattern:** Every beat · Accent bar 1 (default) · 8ths · Follow groove (clicks on the song's swing/shuffle feel) · Bars only (downbeats only,
    for experienced players). A row of bars under it previews the pattern for two bars of the current metre.
  - **Level:** Volume and Accent knobs (the same LED-ring knob).
  - **Switches:** Flash the beat on screen (the time display and the playhead flash on each beat, accent in brass), Click only in headphones (desktop
    app with a multi-output interface: click on outputs 3–4), Mute during count-in only.
  - Footer: "Always on the song's grid: tempo changes, metre changes and swing are followed automatically. Never in exports or stems."
- **Always follows the song:** the click is scheduled from the song's tempo map and metre per section (including tempo automation and the last-chorus
  +2 BPM kind of moves), so it never drifts. In 6/8 and 12/8 it accents the dotted-quarter pulse, and "8ths" clicks every eighth. Inside FL Studio
  the plug-in's click follows FL's tempo and stays off by default (FL has its own).
- **Count-in** stays its own setting (Off / 1 bar / 2 bars) and always uses the click sound, even when the click is off for the take.
- The click is audible only, never recorded into takes (it is excluded from the input and from export). Settings are saved in app settings,
  shared by the app and the plug-in, like the theme.
- Tests: on/off with C; the "when" modes (silent in playback when "Only while recording"); tempo changes and metre changes land on the grid; swing
  follow; your sample plays and the accent sample on beat 1; never in exports; flash on beat; theme contrast of the menu.
The desktop app records natively (TakeRecorder). Inside a DAW, recording stays with the DAW, and this panel says so and offers Import a take.

## 3. The mixer, with your DAW built in (replaces the DAW & MIDI tab)
- **Every strip says what it plays through**, at the top: "Semitone · Dreamy pad (built-in)", "Serum 2 (your plug-in · in Semitone)",
  "→ FL Studio · Ch 3 (MIDI out · Rhodes in FL)", or "Audio · Take 3". Clicking it changes the sound source: built-in sounds, your plug-ins,
  or **send to your DAW** on a channel.
- A strip sent to the DAW is outlined in brass. Its fader area is hatched with "mix this part in FL Studio" (Semitone isn't making that sound), and
  its inserts are empty.
- **Strip layout (no wasted width):**
  - Top to bottom: plays through, four insert slots (empty ones dashed), then a small **EQ curve** in the part colour (click to open the EQ).
  - At the bottom, a two-column **fader area**. The left column is one small rounded **knob panel** (slightly darker than the strip, 8 px corners)
    filled top to bottom: **Pan**, **Width**, and one **smart knob** (30 px LED-ring knobs, 4 px apart), then the **Comp** control filling the rest.
    - **Smart knob:** its label is the effect it controls with ✦ and ▾ ("✦ Drive ▾"), lilac ring for sends (Plate, Echo, Slap, Room, Hall),
      brass for inserts (Drive, Crush, Duck, Tone…). Clicking the label opens **"this knob controls"**: *Useful on <role>* (radio list, the smart
      pick first, each with a one-line reason), *Rare on <role>* (dimmed, with why, e.g. reverb on bass), **Any plug-in control…**, **◎ Learn
      controller**, and **✦ Back to smart**. A manual choice sticks for that part. The brain is `smartKnob` in `data/mix-master.json`: producer
      entry for the role (sourced move in that producer's pass, e.g. Antonoff-style: vocal → Slap, drums → Room), then a sourced style exception
      (film/orchestral may add reverb to bass, high-passed at 600 Hz), then the role pick (bass → Drive, sub → Duck from kick, lead → Echo,
      chords → Plate, pad → Hall, drums → Crush…). Smart never puts reverb or echo on bass unless one of those says so.
    - **Comp:** label, amount ("35%"), then a 16 px bar that stretches to the bottom. A metal cap marks the **amount** (drag up for more, scroll
      for fine steps, double-click = smart amount, right-click = Style Smooth/Steady/Punchy/Glue/Squash, Sidechain from…, Full view); a faint
      brass tint fills below the cap; the live **gain reduction** draws from the top in brass, with the dB under the bar. Style and amount per
      role come from `comp` in `data/mix-master.json`; Amount is an automation target and controller-learnable. Parts mixed in your DAW show "—".
      Today parts have no compressor of their own (only the master's Polish): add one per part in the page and port it to `Source/SynthFx.h`. The right column has a **dB scale** (+6, 0, −6, −12, −18, −24, −36, −60, with 0 in
    the text colour and +6 in red), the fader with a metal cap, and a full-width stereo meter.
- **The meter:**
  - Green → yellow → red gradient that reaches red only near 0 dB.
  - A **peak-hold line** that falls slowly.
  - A **clip light** above it that latches red until clicked.
  - A hatched **red zone** above 0 dB ("above this the part clips").
  - A green-outlined **ideal zone**: the target average level for this part's role, from `part-roles.json` `levelVsLead` relative to the lead, set
    so the mix sums near the master target. For example, lead −12…−6, chords −18…−12, drums −10…−4. It moves with the song's genre profile.
- **Readouts:**
  - Two readouts under the meter: fader dB and peak dB (the peak turns red above 0).
  - One plain **verdict line** under them, in a colour that matches: "In the ideal zone" (green), "A little quiet: +2 dB" (brass), "Clipping: lower
    1 dB" (red, and the strip gets a red hairline), or "Level is set in FL".
  - Clicking the verdict applies the fix as one undo step.
- M / S / ● (record arm) stay at the bottom.
- **Master:** the knob plus inserts, then two horizontal bars:
  - **Loudness** (short-term LUFS) with the green ideal zone for the "Listen on" target (streaming −14 to −9 by default; club, film and podcast change it).
  - **Peak** (true peak) with the red zone above −1 dBTP.
  - The header says "−9.4 LUFS · on target" or what to change. A key line reads "green: ideal for streaming · red: clips".
- **Master card:** LED-ring master knob, LUFS, Polish, Limiter and Listen on.
- **Your DAW card** (brass):
  - The DAW and port ("FL Studio 2026 · MIDI out · port 'Semitone' (IAC)", Port ▾) with a linked status.
  - One row per part with its channel and an on/off switch, shared with the strips' "plays through".
  - Mute Semitone's sound for parts sent out, Send clock & song position, Test each channel, and **Set-up guide** (the FL/loopMIDI/IAC steps
    from the old tab, as a step-by-step sheet).
- **Send to your DAW card:** Drag a section (audio, ⌥ for MIDI), Stems & MIDI (in Share), Markers in FL. These are the old tab's drag and export
  features, now next to the routing.
- Inside the FL plug-in, "Your DAW" shows FL as the host and the routing uses the plug-in's own MIDI output, with no virtual port needed.

## 4. Mixer channels and routing (`4-Mixer-routing.png`, values in `reference-Routing.dc.html`)
Today every part already gets its own internal channel (`master.buses`, `xFreeCh` for MIDI), but channel numbers aren't shown, there are no
group buses or separate effect returns, and nothing can be rerouted. Add:
- **Numbered channels:** each strip shows its number (01, 02…) in a small badge. Numbers follow the timeline's part order and stay stable when parts are
  added (a new part takes the next free number, a deleted part's number is reused only on "Renumber").
- **Auto-routing by role (on by default),** using `part-roles.json` families:
  - lead, harmony, double and vocal audio → **Vocals bus** (glue + de-ess);
  - chords, pad, arp, counter, bass, bass2, loops → **Music bus**;
  - drums and perc → **Drums bus** (punch);
  - everything sends to the **Plate return** at a role default and to the **Echo return** (synced to the song's tempo) only for lead and counter;
  - bass and pads duck from the kick through a **sidechain** when the style uses it (EDM, pop).
  - Buses and returns go to Master. New parts are linked the same way the moment they're added, and a style or producer can change the defaults
    (e.g. an Antonoff-style pass adds a slapback send on the lead).
- **Manual control, any time:**
  - Each strip has a **→ route button** above M/S ("→ Vocals bus") that opens the routing card.
  - **Output:** a list of buses, Master, New bus…, or "FL Studio · Ch n" (MIDI out). The auto choice is marked with its reason
    ("auto: bass is music").
  - **Sends:** Plate, Echo and any added return, each a level slider (dB) with pre/post; Sidechain with its source (Kick, Snare…) and amount.
  - Changing any of these marks the strip **manual** (brass route button) and stops auto-linking for that strip only. **Back to auto** returns it.
  - Dragging a strip onto a bus in the Buses card also routes it.
- **The drawer toolbar shows the state:** "● Auto-routing by role · 1 manual · Routing map ›".
- **Routing map:** a simple left-to-right diagram of parts → buses → master, where lines can be dragged to reroute (same data). Good for checking the
  whole mix at a glance.
- **Buses & returns card** (right column): each bus or return with its number (B1, B2…, R1, R2…), what feeds it, a level meter, and ＋ Bus. Clicking one
  opens its own strip (fader, inserts, the same meter with its ideal and red zones). Returns are tinted violet, buses brass.
- **MIDI channels for the DAW** stay automatic (`xFreeCh`) and show in the Your DAW card and in each strip's output when sent out. They can be
  changed in the same route card.
- Everything is saved in the song and undoable. The native engine and export render buses, returns and sidechain exactly like the page
  (`Source/Synth.h` mixer). Tests cover auto-routing for every role, manual override and back to auto, sends audible, sidechain ducking, buses
  in the engine and in stems, and renumbering.

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
