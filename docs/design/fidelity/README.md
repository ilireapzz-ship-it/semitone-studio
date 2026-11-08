# Design fidelity check, 8 Nov 2026 (after steps 13–18)

Jamie: "the code agent hasn't managed to integrate our designs properly." Each `compare-*.png` puts the design (left) next to the
built app at 1440×900 (right, `web/semitone-studio.html`, Chromium, after `a46f1cf`). The features mostly exist; the **screens do not
match**, because new parts were added on top of the old panels inside the bottom drawer instead of rebuilding each screen from its
reference file. Fix the causes first, then each screen. **Rebuild each screen's markup to match its `reference-*.dc.html` (copy values:
sizes, spacing, colours, order). Remove the old panel it replaces; don't keep it underneath. Keep the working logic and tests.**

## Causes (fix these first)
1. **Screens get the whole window.** In every Mix / Automation / Sound board, the timeline folds to the slim one-row song strip
   (`SONG  Intro · Verse 1 · Pre · Chorus 1 …`) and Session folds to the thin right rail (icons with counts). The build keeps the full
   4-track timeline (~150 px) and the 300 px Session column open, so each screen is squeezed into the bottom third. Opening Mixer, Smart
   EQ, Compressor, Automation or Record applies the `sp-big` space mode and folds Session to its rail (`docs/design/space/README.md`),
   and gives it back on leaving.
2. **No old panels left behind.** Record still shows the old "Play & record" keyboard, "Listen & analyse" and the duplicate Record /
   Record melody (keys) / Record voice buttons under the new row. The mixer strips are the old strips with additions. Remove what the
   design replaced; keep any feature that has no new home by moving it to where the spec says.
3. **Each board's own header line.** Boards put the part picker, the sub-tabs (Mixer · Snapshots, etc.) and ⤢ on one 32 px line; the
   build repeats the drawer tabs (Song Notes Sound Mix Ideas) plus a second tab row.

## Screens
### Mixer (`compare-P-Mixer.png`; `docs/design/panels/README.md` §3–4, `reference-Mixer.dc.html`)
- Strip width so names never wrap ("Melod/y" now). Header: colour bar, number, name; "plays through" card; inserts with power icons.
- **Knob column beside the fader** (Pan, Width, ✦ smart knob in one dark rounded panel), then the **Comp bar between the green power
  rail and the gold open rail** with "Bypass · B" / "Open Compressor" tips, then the **tall fader with the two big meters**: ideal-zone
  box, red clip zone, peak hold, dB scale. Under it the dB values and the **verdict line** ("In the ideal zone", "Clipping: lower 1 dB",
  "A little quiet: +2 dB"), then M / S / ●.
- Remove from the strip: the Pan slider + Mono button row, "Chain preset…", "Play to check", the bus and sidechain selects (routing lives
  in the routing card and the strip's ⋯ menu, `panels/README.md` §4).
- Right column: Master card (as built, fine), **Your DAW** card (linked, channel list with Ch numbers and switches, Test each channel,
  Set-up guide) and **Send to your DAW** (drag a section, stems & MIDI, markers). Buses & returns stays under the routing view.
- Transitions and Master bus strips: keep, same strip design.

### Record (`compare-P-Record.png`; `panels/README.md` Record + Click track, `reference` P-Record board)
- Left column: **Input** card (device, level meter with "good level", Hear myself, Low-cut, Mono, Pre-roll, latency), **MIDI keyboard**
  card (small keyboard, Computer keys, Snap, Controller), **Listen & analyse** card (drop zone, Listen, Analyse a file).
- Main: What are you recording (4 cards, as built), Backing segmented, **Takes** for the section as waveform lanes with ▶ ★, a **Comp**
  lane (what plays), and the four cards: Punch in, Loop record, Tuning, Timing.
- The **Click pop-out** (When, Sound tiles incl. Your sample…, Pattern bars, Level knobs, switches) opens from the Click button and the
  ▾; it must exist, not only the inline Off/Recording/Always.
- Delete the old keyboard block and duplicate buttons.

### Smart EQ (`compare-E-EQ.png`; `mixing/README.md` §2–3, `reference-EQ.dc.html`, `reference-EQ2.dc.html`)
- Full workspace, not a pop-up over the mixer. Header: **On/Bypassed pill**, number, "Melody · EQ", legend, A/B, Level-matched,
  Presets, ×.
- The graph always shows something: when stopped, show the last analysed spectrum (or analyse the part's render on open) with the
  ideal range and masking hatching; never an empty grid saying "Play to see the spectrum".
- **The EQ curve with numbered band nodes** on the graph, the **band panel** (type tabs, Freq/Gain/Q/Range knobs, threshold for dynamic,
  Stereo/Mid/Side/L/R, Listen, solo, lock) beside the selected node, and the **band cards row** along the bottom (number, type, values,
  who set it: Smart / You / profile, ＋ Band).
- Right column: Smart EQ card (sentence + Suggest bands + Match reference), I hear…, Checks for this part. "Suggest bands" adds nodes
  to the curve (they appear on the graph), not only a list with Keep buttons.

### Compressor (`compare-E-Comp6.png`; `mixing/README.md` §4c–4d, §5, `reference-Comp6.dc.html`, `reference-Comp7.dc.html`)
- Close. Add the **Knows your song** card (Breathe with the drums, Rides each word, Follows the song, Keeps the big moments, Makes room)
  and the **lyric lane** + section amounts under the graph; show the last analysed level graph when stopped; **More** opens the pro
  panels (4b). Knob: thin amount arc, red inner ring for live reduction, ✦ smart marker.

### Automation (`compare-A-Lanes.png`; `automation/README.md`)
- Close in content. With cause 1 fixed it gets the full height. Add the **Session "Movement" list** for the selected lane (what it does,
  why, who made it, suggestions with Try), and the selected lane's big view with value labels on the points ("6.8 kHz").

### Style & settings (`compare-P-Style.png`)
- Close. Check spacing against the reference once cause 1 is fixed.

## Done means
For each screen, a Playwright screenshot at 1440×900 next to the design board, both in this folder (`after-*.png`), with no visible
differences in layout, order or sizes beyond real data. Update PROGRESS.md row 19 in plain words for Jamie.
