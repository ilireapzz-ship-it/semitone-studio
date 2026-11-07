# Automation, redesigned (agreed with Jamie 8 Nov 2026)

Before: `before.png`. One lane for one section, chosen from two dropdowns, a third for shapes, an 80px strip, a loose
"Smart mix" box, and empty space. After: `1-Whole-song.png`, `2-Add-a-lane.png`, `3-Editing-details.png`, `4-Macro.png`.
Exact values: `reference.dc.html` (a design-canvas file: copy its values, not its markup). Same look tokens as the rest (themes.json).

## Where it lives
- **Automation becomes its own drawer tab** (Notes · Words · Sound · Mix · Automation). Ideas moves into ⌘K and Session (ask Jamie
  if he'd rather keep five tabs and put Automation under Notes; drawn as its own tab).
- It uses the space rules (docs/design/space): the editor takes the room, and the timeline becomes the 22px-per-part overview with an
  "A n" count per part.
- Automation lanes under parts in the Produce and Mix lenses are the same data. Clicking one opens this tab on that lane.

## Layout
1. **Toolbar (one row):**
   - the selected-lane menu ("Chords · Brightness ▾");
   - the tabs;
   - Whole song / Section;
   - tools: ↖ select, ✎ draw, ╱ line, ⌒ curve;
   - Snap ▾ (bar, beat, 1/8, 1/16, triplets, off);
   - ● Write ▾ (Write / Touch / Latch);
   - Off / Moves on (hear without automation, an instant A/B that doesn't change the song);
   - ⤢.
2. **Above the lanes:** section names with bar numbers, plus a 16px **emotion and energy line** across the song, coloured by each section's
   emotion, so moves are shaped against the feeling. Section bands run down behind every lane (choruses slightly tinted).
3. **Lane headers (230px), grouped by part in song order** (Melody, Chords, added parts, Bass, Drums, Song, Master):
   - part (small caps, part colour), a plain name ("Brightness", not "Filter 1 Freq");
   - a knob showing the value under the playhead, in real units (dB, %, Hz/kHz, L/R, BPM);
   - **who made it**: You, Emotion, <Producer>-style, Smart mix, Coach. A pass's lanes stay hatched and ghosted until Keep;
   - B (bypass this lane) and S (hear only this lane's part).
   - The **selected lane** is tall (≈196px). It shows where the control lives ("Serum 2 · Filter 1 Freq · switched on for Pre and choruses"),
     value guide lines in units on a sensible scale (log for frequency, dB for volume), points with value labels at turning points, and
     bend handles. Other lanes are 46px (macros 58px), and all lanes are visible without scrolling at 1440×900 up to about 8.
     Parts can collapse their lanes, and lanes can be reordered by dragging.
4. **Shapes bar (64px):** Build, Swell, Breathe, Pump, Sweep, Drop, Wobble, Steps, Fade.
   - Each has a picture and a one-line tooltip.
   - **Over:** 1 bar, this section, to next chorus, the selection.
   - **Copy to:** every <this section type> (every Pre, every Chorus).
   - Breathe dips while the vocal sings (it reads the Melody notes) and opens in the gaps.
   - Pump is on every beat.
   - Drop cuts the last bar.
5. **Session panel:**
   - Pinned at the top: "Selected lane · What it does", a plain explanation of what the control does to the sound and why
     producers move it, with chips for who made it and which emotion it serves.
   - Then automation suggestions, each with Try, weighed by the coach like every other suggestion (e.g. swell the reverb on one word,
     drop the drums for the last bar of the Pre, duck the chords under the vocal, last chorus +2 BPM, sync tremolo to 1/8).

## Add a lane (`2-Add-a-lane.png`)
- **"What should change over time?"** with a search box in plain words: "brighter" → Brightness, "bigger" → Reverb, Width or the Energy macro,
  "faster" → Tempo.
- Four columns with one-line meanings:
  - Level & space: volume, pan, width, reverb, echo
  - Tone: brightness, warmth, drive, low cut
  - Movement: tremolo, chorus, vibrato, filter wobble
  - Song & macros: tempo, Energy, master brightness, silence
- The **for [part ▾]** picker also offers Master, Song, and every added part and layer.
- If the part has a plug-in, its mapped controls show with friendly labels (`plugin-maps.json`), plus **Learn a control…** (the
  existing learn flow).
- A **New macro** row at the bottom.
- Only controls that can actually be heard are offered. A plug-in control that's switched off in the patch says so ("needs Filter 1 on:
  turn it on for these sections?").

## Editing details (`3-Editing-details.png`)
- **Points:**
  - Click on the line to add a point; drag to move it.
  - ⇧-drag for fine moves; ⌥-click to delete.
  - Double-click a point to type an exact value in units, "6.8 kHz" ↵.
  - Hover shows the bar.beat and value.
- **Selection:** drag a box to select points, then ⌘C/⌘V, ⌥-drag to duplicate, ← → to nudge by the snap, and Delete. Select a time range
  across all lanes to copy every move in that range.
- **Bends:** right-click a segment for Straight, Smooth, Build (slow then fast), Ease off (fast then slow), Hold then jump, plus a Bend amount.
  Also Thin points, Flatten to one value, Back to rest, Turn into a shape.
- **Shapes stay shapes:** Wobble and other shapes are objects (outlined, with a label like "Wobble · 1/8 · 40%"). Click to change Speed (1/4, 1/8,
  1/16, triplets, free), Shape (Smooth, Triangle, Square, Random), Depth, and Starts (at once, fades in, on the downbeat). They stretch and copy as
  objects, and "Turn into points" makes them hand-editable.
- **Write modes:**
  - Write replaces the lane while playing.
  - Touch writes only while you hold the knob, then glides back.
  - Latch keeps your last value.
  - Writing happens with the lane's knob or a learned **MIDI controller knob** (Learn), and each pass is one undo step.
- **Values stay safe:** plug-in limits (`max` in plugin-maps) and level limits apply, and volume moves above 0 dB warn.

## Macros (`4-Macro.png`, exact values in `reference-macro.dc.html`)
Look: a machined knob (tick ring 0 → .5 → 1, brass arc that glows, dark domed cap with a soft shadow, brass pointer), the value in an
inset display ("0.48 · at bar 14 · Pre"), and preset chips (Lift, Open up, Pull back, Tension). Each control is a soft card with an icon tile in
the part colour, its name and where it lives, a recessed range track with tick marks, metal end caps, a fill in the part colour, a glowing brass
diamond at the current value with a value tag, a small response graph showing where the knob sits now, and an invert toggle. "Across the song"
shows the Energy line (brass, filled) over section bands, with each control's resulting line (dashed if inverted) and the playhead. The header
holds an "In FL Studio as 'Macro 1'" switch, Save to ⌘K and ×.
- A macro is one knob (0–1) that moves several controls. Each has a **range** (value at 0 → value at 1, shown in units with the current value),
  a **response** (Straight, Slow start, Fast start) and **Invert**. A row of small lines shows what the macro's lane does to each control across
  the song.
- **Draw it from the emotion line:** builds the macro's lane from the section emotions and energy (rises into euphoric, dips for intimate).
- **Show in FL Studio as a plug-in control.** Gap found: the plug-in exposes no host parameters today, so FL can't automate anything in Semitone.
  Expose a fixed set of 8 host parameters ("Macro 1–8"), mapped to the song's macros by name. FL automation then drives them (the host value wins
  while FL automates; Semitone's own lane is shown greyed with "FL is automating this").
- Macros save with the song. Saved macros (a name, controls by role and ranges) also live in ⌘K, so "Energy" can be reused in another song.

## Gaps this design closes (found checking the old tab against pro DAWs and Semitone itself)
1. **Whole-song view:** automation is about arcs across sections; the old tab showed one section only.
2. **No overview of what's automated:** lane headers, "A n" counts in the overview, and lane badges in the Mix strips (already there) now agree.
3. **No reasons:** every lane says who made it; the selected lane explains what it does; suggestions carry a coach rule.
4. **Moving sections:** lanes are stored per section (`S.auto[si]`), so they move and copy with sections (`secMove`). The whole-song view
   stitches them. The edge between sections stays continuous: when a section moves, its first point is joined to the new neighbour with a
   short glide unless it starts with a hold.
5. **Producer passes:** a pass's lanes are previews (ghosted) until Keep, and the A side hides them, like the rest of the pass.
6. **Added parts, layers, loops, plug-in effects and Master** all get lanes. The part picker lists `songParts()` and `S.hostFx` slots.
7. **Mixer faders vs volume lanes:** when a part has a volume lane, its fader shows the moving value and becomes a **trim** (an offset in dB
   on top of the lane), so mixing never fights the automation. Same for pan.
8. **Units and scales:** values in real units, a log scale for frequency, dB for level, never 0–1 or raw parameter numbers.
9. **Smooth sound:** values glide between points with no zipper noise (ramp per block in the engine). Plug-in parameters already ramp through
   `hostPlugLanes`.
10. **Native engine and export:** every lane type plays in the native engine, the plug-in and audio export, not only the page. Tested with
    `tests/synth_test.cpp` for volume, pan and the effect sends.
11. **MIDI export:** plug-in lanes linked by MIDI CC export as CC in the MIDI file, and Volume/Pan as CC7/CC10, so moves survive the trip into FL.
12. **Empty state:** a song with no lanes shows three suggested starts from the song's emotion arc (e.g. "Open the chords into the chorus",
    "Breathe the pad around the vocal", "Energy from the emotion line") plus ＋ Add a lane, instead of a blank strip.
13. **Coach:** new rules in `coach.json`: `movement` (a song where nothing moves across sections, Theory, base 0.4), `nervous` (too many moves
    in a short time, or wobble on everything, base 0.4), `vocal-space` (moves on other parts that mask the vocal, base 0.5). The A. G. Cook-style
    producer tightens `movement`, and Rick Rubin-style loosens it.
14. **Small windows:** below 1280 the lane headers shrink to 160px (name and value only) and the shapes bar becomes one "Shapes ▾" menu. Below 800 tall,
    lanes other than the selected one become 28px.
15. **Keyboard and accessibility:**
    - Tab moves between points; arrows move a point by the snap and by small value steps.
    - Every point has an aria label ("Brightness 6.8 kHz at bar 16").
    - Lane colours come from the part tokens, plus the part name, so they never rely on colour alone.

## Tests (tests/web/automation.test.js)
- The whole-song view stitches sections, and a moved section keeps its lanes.
- Every lane type draws and plays, and the value under the playhead matches the knob.
- Shapes over 1 bar, a section, to the next chorus and a selection; copy to every chorus.
- Bends, exact values, multi-select copy/paste, and Wobble stays an object.
- Write/Touch/Latch with a fake MIDI CC, one undo step each.
- Macro ranges, response and invert; draw from emotion.
- Fader trim with a volume lane; Off/Moves on doesn't change the saved song.
- Pass lanes ghosted until Keep.
- Sizes at 1440×900, 1280×800, 1024×680 and 900×600 with no page scroll.
- The native engine plays a volume and an echo-send lane the same as the page (synth_test).
