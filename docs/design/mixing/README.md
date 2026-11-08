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

## 4. Smart compressor (double-click a strip's Comp bar; `5-Smart-compressor.png`, values in `reference-Comp.dc.html`)
Different compressors do different jobs, so the window offers five characters, and Smart picks one for the part. The brain is `comp` in
`data/mix-master.json` (characters with sources, role picks, producer picks, the "I hear…" fixes and the checks).
- **How it opens:** double-click the Comp bar on any strip (or Full view in its right-click menu). The strip shows it two ways: a small
  open-window icon next to the "COMP" label, and the hover tip "◦◦ Double-click to open". ⌥/Alt-click the bar now resets to the smart amount
  (it used to be double-click).
- **Header:** "06 Vocal · Compressor", legend (in = grey outline, out = part colour, squeezed = brass from the top), A/B, Level-matched,
  **Hear what's removed** (only what the compressor takes away), Presets, close.
- **Five characters** (cards in a row): **Clean** (digital: transparent), **Punch** (VCA: tight, keeps hits; drums and buses), **Fast** (FET:
  grabs peaks, adds bite; vocals, bass, drums; never on the master), **Smooth** (opto: slow, warm, evens a performance; vocals, bass), **Glue**
  (vari-mu: gentle, ratio rises as it works; buses and master). Each card has a one-line sound, a speed meter (1–5) and what it's for. The smart
  pick has a filled "✦ smart pick" badge. How each must sound is in `characters[].engine` (detection, knee, release behaviour, saturation).
- **Display:** 4 bars of the part scrolling: input outline, output fill, gain reduction drawn from the top, a draggable dashed **threshold** line
  with its value, section marks, and callouts in plain words on the biggest jump ("“home” jumps 8 dB over the line") and on breaths.
  Beside it: the **transfer curve** (knee and the current point) and **In / GR / Out** meters.
- **Amount:** the big knob, the same value as the strip's bar (moves threshold and ratio together).
- **Fine control:** Threshold, Ratio, Attack, Release (ms or a note value, "auto"), Knee, Mix (parallel), Output ("auto" make-up), Detect
  Peak/RMS/Auto, **Listen to** (sidechain: itself or another part, with a low cut, default 120 Hz so the bass doesn't pump it), **Squeeze only**
  (whole range, or lows/mids/highs: a band compressor), **＋ Second compressor** (two in a row, e.g. a Nigro-style vocal: Fast catches peaks,
  then Smooth evens it). Values Smart set show ✦; touching one makes it yours, and Listen & set never overwrites yours unless asked.
- **Smart panel:** what it picked and why, in plain words, from the part's measured loud-to-quiet range and the producer or engineer profile
  ("Light and close, like his dry vocals… Smooth (opto) at about 3 dB"). **Listen & set** plays 8 bars (or the selection) and sets the threshold
  for the target reduction. **Match reference…**
- **I hear…:** Uneven, Squashed, Pumping, Lost its punch, Breaths too loud, Crunchy, Buried, Too flat. Each maps to one change (`comp.hear`).
- **Checks for this part:** even across sections, single jumps too fast for the character, still alive (loud-to-quiet kept), breaths raised by
  make-up, release fits the tempo (no pumping), bass distortion from a too-fast release (`comp.checks`), each with Fix / Listen / Why.
- **Today:** parts have no compressor of their own (only the master's Polish). Build it in the page and port it to `Source/SynthFx.h`.

### 4b. Compared with the best, and what was added (`6-Smart-compressor-pro.png`, values in `reference-Comp2.dc.html`)
Checked 8 Nov 2026; details and sources in `comp.benchmark` and `comp.pro` (`data/mix-master.json`).

| Our type | Best in class | What they have | Added to match |
|---|---|---|---|
| Clean | FabFilter Pro-C 3 | upward + multiband up/down styles, Tube/Diode/Bright saturation, Auto Threshold, MIDI and tempo triggers, 6-band sidechain EQ with mid/side, lookahead, 32x oversampling | Direction, Colour, keep-the-same-squeeze, Trigger, Listen to (6-band), Lookahead, Quality, Smooth automation |
| Punch (VCA) | SSL G bus (Cytomic The Glue) | ratio 2/4/10, stepped attack/release + Auto, Range, sidechain HPF to 2 kHz, peak clipper, dry/wet | classic stepped controls, Range, Clip peaks, low cut to 2 kHz |
| Fast (FET) | 1176 | input drives it, 4/8/12/20 + All-buttons, attack 20–800 µs, program-dependent release | ratio buttons incl. All, Input-style amount, µs attack, program release |
| Smooth (opto) | LA-2A | Peak Reduction + Gain, no attack, Compress/Limit, Emphasis | those controls when Smooth is chosen; Emphasis = Ignore lows |
| Glue (vari-mu) | Fairchild 670 | six time constants (5–6 auto), Lat/Vert, ratio rises with level | Time 1–6, Stereo Linked/Mid/Side |

- **Pro view layout:** the chain at the top (each compressor a card with its reduction; → between; ＋ Another; a Fix can add "Fast before");
  the display with transfer curve and meters; **the selected type's classic controls** (e.g. Fast: ratio buttons 4 · 8 · 12 · 20 · All, Input,
  Attack in µs, Release, Output, Mix, "Release follows the singing"); a strip listing each type's classic controls; on the right, **More**
  (Direction, Colour + Drive + Pre/Post, Range, Lookahead, Stereo, Clip peaks, Smooth automation, Quality), **Listen to** (Trigger: Itself /
  Another part / Beat 1/8 / MIDI notes; a 6-band sidechain curve; shapes Ignore lows, Focus on esses, Follow the kick, Ignore the hats, Mid only;
  ◉ Hear it), and **Split into bands** (Off / 2 / 3; band cards with range and reduction). New items carry a small green NEW tag in the design
  only.
- **Where Semitone goes further:** the smart pick by role, producer and style; checks and "I hear…" in plain words; Listen & set; tempo-aware
  release; one Amount shared with the strip; level-matched A/B and Hear what's removed.
- **Not covered:** external audio sidechain from outside Semitone, surround. Limiting stays on the master, de-essing in the EQ (or "Focus on
  esses" + a high band here).

### 4c. The finish: how the window looks by default (`7-Compressor.png`, values in `reference-Comp5.dc.html`)
Sections 4 and 4b say *what* is in the window; this says how it looks. Jamie found the first luxe try (brown waveform, cream VU meter, small
knobs) still ugly; this refined look replaces it. Principles: restraint, one accent colour, real data drawn well, no boxes.
- **Panel:** 16 px corners, graphite gradient (#141518 → #0F1012), 1 px hairline border rgba(255,255,255,.07), faint top highlight, soft
  shadow. Sections split by hairlines only. Inks: #EEECE8 text, #8D8F95 secondary, #5D6066 labels; one accent, champagne #E3C58E; the part
  colour (here #E9A59B) only for the audio; red only for gain reduction.
- **Header:** part name in the serif (28 px) with "COMPRESSOR" in spaced caps beside it. The five types are plain text tabs on one hairline;
  the chosen one is in the text colour with a 1.5 px champagne underline. Right: A / B, Hear removed, Presets, × as quiet text.
- **Smart line:** a small ✦ in a thin champagne circle, one sentence ("Smooth, about 3 dB — evens out the loud words…"), "2 notes", and the
  only filled button, **Listen & set** (champagne pill).
- **Display: calm, not flashy (Jamie found the glowing, striped, three-colour version cheap, 8 Nov).** Rule: one background, one thin fill,
  one crisp line, colour only where it means something. No glows, no grain, no hatching, no boxes inside the panel, no colour key needed.
  - The graph sits straight on the panel (same background, no inner box), full width. Five faint grid lines (0, −12, −24, −36, −48 dB) at
    3.5% white, labels on the right in grey mono; bar numbers and section names under it in plain text ("Bar 33 … Chorus 2 … 36").
  - **What you hear:** a single 1.1 px pale-mint line (#BFF0D6) with a very light mint fill under it (30% at the top → 8% → nothing).
  - **What was turned down:** only where the compressor worked, the gap between the original level and what you hear is filled in solid
    champagne (#E9CF9C, 90% → 45%). Nothing else is drawn for the original, so gold means "compressed here" and nothing else.
  - **How much:** a 1 px soft-red line (#F07272) hanging from 0 dB with a 16% red fill fading down, same dB scale.
  - **Threshold:** a fine dotted champagne line with a small gold "−18" where the scale is.
  - The biggest moment gets one quiet label in the clear space beside it: “home” in the text colour and −9.1 dB in gold, with a hairline
    to the peak.
  - The **knee** moves out of the graph to the right column: a 108 px outlined square with the curve and a gold dot, labelled "Knee", next to
    the IN / OUT meters.
- **Hero column (320 px):** one **Amount knob** (220 px): matte graphite body, champagne pointer, the value in the serif inside ("45") with
  "AMOUNT" under it; an outer thin champagne arc for the amount, a small cream ✦ sparkle on the ring for the smart amount (hover label "Smart: 35"; click it to jump there), and an inner part-colour arc showing the live
  gain reduction. Under it two readouts split by a hairline: "−3.4 dB REDUCING" and "35 ✦ SMART"; then thin IN and OUT meters with a peak tick.
- **Parameter row:** no knobs. Each parameter is a column: spaced-caps label (✦ if Smart set it), the value large and light (17 px, tabular),
  and a 2 px line underneath showing its position (champagne if Smart set it). Drag the value up or down, or click to type. Right: "Listen to ·
  Itself", "Bands · Off" and an outlined **More** (opens the 4b panels).
- **Motion:** the waveform scrolls with playback, the gain reduction line draws live, the inner ring follows it with gentle ballistics; nothing
  else moves.
- **Themes:** device panel, dark in every theme, own inks inside (rule in `docs/design/panels/README.md`). Checked in Daylight.

## Tests (tests/web/mixing.test.js + synth_test)
- Every problem's words map to it.
- A synthetic muddy chorus finds the right zone and parts; a masked vocal finds the masker; sibilance is detected on a vocal take.
- Calibrate respects locks and limits and is level-matched; every change is undoable and switchable.
- Destination targets are met within ±1 LU with true peak ≤ target.
- The EQ (including dynamic and M/S bands) sounds the same in the page and the engine.
- Every engineer in `bestFor` exists; low-confidence profiles show their note.
- Smart compressor: each character's curve and timing matches between page and engine on the same notes; double-click the Comp bar opens it;
  ⌥-click resets the amount; the window's Amount and the strip's bar stay in sync; Listen & set lands within 1 dB of the target reduction.
- Compressor pro: upward mode raises quiet passages without raising peaks; Range caps reduction; lookahead latency is reported and lined up;
  Beat and MIDI triggers duck on the grid; sidechain shapes change what triggers but not what you hear; bands sum flat when off.
