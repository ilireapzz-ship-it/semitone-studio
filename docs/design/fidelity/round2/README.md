# Design fidelity check, round 2 (9 Nov 2026, after `bde7e2d`)

Jamie: "it still isn't quite there." Same method as round 1: `compare-*.png` = design board (left) vs the built app at 1440×900 in
Chromium (right). Round 1's causes are fixed (whole-window screens, Session rail, slim song strip, Record rebuilt, Knows your song).
What's left is below, worst first. Work through it in order, and for each item save `after-*.png` next to its design here.

## 1. The graphs are empty when the song is stopped (biggest gap)
- **Compressor** (`built-Comp-after-8s.png`): still "Measuring this part…" after 8 s, so there is no level graph, no gold "taken off"
  caps, no red gain-reduction line, no lyric lane, no section amount bars, no "home −9.1 dB" / "♥ kept big" labels. The window is mostly
  empty. On open, render the part offline (as Calibrate does) and draw the 4 bars around the play marker (or the selection) within ~1 s;
  while playing, scroll live. "0.0 dB REDUCING" should show the measured value.
- **Smart EQ** (`built-EQ-after-suggest.png`): still "Measuring this part…": no before/after spectrum, no ideal-range corridor, no masking
  hatch from the compared part. Same fix: analyse the part's render on open.
- If headless rendering is the only problem (tests pass, app works), prove it with a screenshot from the real app.

## 2. Smart EQ bands
- **Suggest bands** draws dashed ghost bands on the curve, but they stay a list with Keep buttons on the right. In the design the bands are
  **numbered nodes on the curve** (HP, 1, 2, 3…), each with a **card in the row along the bottom** (number, type, values, who set it:
  Smart / You / profile) and the **band panel** beside the selected node (type tabs, Freq / Gain / Q / Range knobs, threshold for dynamic,
  Stereo/Mid/Side/L/R, Listen, B, lock). Keep the suggestions as ghosts with ✓ on the node itself; Keep all turns them into real nodes +
  cards. The bottom row is currently only "＋ Band".
- The EQ's header pill and legend match. The right column matches apart from the suggestion list (move it onto the graph).

## 3. Compressor finish
- **More** (`built-Comp-More.png`) only brings back the knee, In/Out lines and the I hear chips. It should open the pro panels from
  `mixing/README.md` 4b: the chain cards (two in a row), the chosen type's classic controls (e.g. Fast: 4 · 8 · 12 · 20 · All, Input,
  µs attack), Direction, Colour + Drive + Pre/Post, Range, Lookahead, Stereo, Clip peaks, Quality, Listen to (Trigger + 6-band sidechain
  curve + shapes), Split into bands. Values in `data/mix-master.json` → `comp.pro`.
- Amount knob: design is 176 px with a thin champagne amount arc, a **red inner ring for live reduction**, and the ✦ smart marker on the
  ring; built knob is smaller with no inner ring.
- "2 notes" link (opens checks) is missing from the smart line. Presets is missing from the header.
- "Makes room" says "not built yet": build it (spec 4d) or hide it until it works. Don't ship a switch that does nothing.

## 4. Mixer strips (`strip-design-vs-built.png`)
- **EQ thumbnail**: needs its **power icon** at the top right (bypass), and should show the real curve; with no bands it's a flat line.
- **Verdict line**: built shows "Play to check" whenever stopped. Keep the last measured verdict ("In the ideal zone", "Clipping: lower
  1 dB", "A little quiet: +2 dB") after playback stops, with the last peak/dB numbers, so the strip isn't blank.
- Smart knob value reads "0%" by default (design: shows the smart amount, e.g. "Echo 18%"). If it must start at 0, show the smart amount
  as a faint ✦ tick on the ring.
- Header row: the design has one quiet line "All parts ▾ · Notes Words Sound Mix Automation · Mixer Snapshots". The built line has 12
  items (Mixer, Automation, Calibrate, Fix what I hear, Auto-routing by role, Routing map ›, Smart pan, Mix check, Mono check…). Keep
  Mixer · Snapshots there; move Calibrate, Fix what I hear, Smart pan, Mix check, Mono check into one "Tools ▾" menu (or the Mix check pill
  that opens a list), and routing into the routing card. Automation stays its own tab.
- Right column: matches in content (Master, Your DAW, Send to your DAW, Buses & returns). Send to your DAW should be the three rows from
  the design (Drag a section · Stems & MIDI · Markers in FL) rather than the chip grid.

## 5. Record (`compare-P-Record.png`, `built-Click-popout.png`)
- Very close. Click pop-out exists and matches; give it the 78 BPM · 4/4 · follows the song chip and the Pattern bars as beat blocks
  (accent taller/brighter) like the design.
- "Tuning: Not built yet" → build the gentle pitch help or hide the card until it exists.
- Takes empty state is fine; check with three takes that waveforms, ★, the Comp lane and Make melody from comp look like the board.

## 6. Automation (`compare-A-Lanes.png`)
- Close. The selected lane in the design is tall with **value labels on points** ("420 Hz", "6.8 kHz", "1.1 kHz", "9.4 kHz") and its
  real unit scale on the left; check those show on any tall lane. Session "Movement" matches.

## 7. Style & settings
- Matches (checked in round 1).

## Done means
Every item above either matches its board in an `after-*.png` here or has a short reason in PROGRESS.md row 20. Graphs must have content
in the screenshots (load the demo song if needed).
