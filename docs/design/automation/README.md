# Automation, redesigned (agreed with Jamie 8 Nov 2026)

Pictures: 1–4, `5-Record-with-a-controller.png`, `6-Split-move-copy.png`. Before: `before.png`. One lane for one section, chosen from two dropdowns, a third for shapes, an 80px strip, a loose
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
Look (modern, clean):
- **The knob:**
  - a 41-segment LED ring (lit segments brass, brighter towards the value, unlit graphite);
  - a dotted inner arc showing the range the song's Energy line covers (0.15–1.00), with end dots;
  - a matte cap with a fine knurled rim and a soft drop shadow;
  - a brass pointer line and dot;
  - the value printed in the centre ("0.48", with "ENERGY · PRE" under it), and 0 and 1 at the ends.
- Under it, three joined tiles: Lowest / **Now** / Peak.
- One short line of help.
- A segmented preset control (Lift, Open up, Pull back, Tension).
- **Snapshots A and B** ("Verse feel 0.30", "Chorus feel 0.95"): click to jump, drag between them to morph.
- Then **Draw it from the emotion line**.
- No empty bands: the song preview on the right grows to fill the height, and "＋ Add a control" is a dashed full-width row
  ("drag one from the lanes on the left"). Each control is a soft card with an icon tile in
the part colour, its name and where it lives, a recessed range track with tick marks, metal end caps, a fill in the part colour, a glowing brass
diamond at the current value with a value tag, a small response graph showing where the knob sits now, and an invert toggle. "Across the song"
shows the Energy line (brass, filled) over section bands, with each control's resulting line (dashed if inverted) and the playhead. The header
holds an "In FL Studio as 'Macro 1'" switch, Save to ⌘K and ×.
- A thin brass "signal" spine runs from the knob down the left of the control cards, and each card is tapped with a glowing brass dot.
- Icons are drawn line icons per control type: filter curve, reverb waves, stereo arrows, faders.
- The selected card is lifted (part-colour hairline, soft shadow) and opens fine settings: Glide (ms), Bend, Limit (the plug-in's safe range,
  from plugin-maps `max`) and Steps.
- The left column ends with Lowest and Peak tiles (value and section), then **Draw it from the emotion line**.
- The song preview has faint value gridlines, uppercase section labels, and a glowing brass dot with a "0.48" tag where the playhead meets the Energy line.
- The panel opens over the lanes at about 900×650 and never scrolls at 1440×900. In smaller windows it fills the drawer.
- A macro is one knob (0–1) that moves several controls. Each has a **range** (value at 0 → value at 1, shown in units with the current value),
  a **response** (Straight, Slow start, Fast start) and **Invert**. A row of small lines shows what the macro's lane does to each control across
  the song.
- **Draw it from the emotion line:** builds the macro's lane from the section emotions and energy (rises into euphoric, dips for intimate).
- **Show in FL Studio as a plug-in control.** Gap found: the plug-in exposes no host parameters today, so FL can't automate anything in Semitone.
  Expose a fixed set of 8 host parameters ("Macro 1–8"), mapped to the song's macros by name. FL automation then drives them (the host value wins
  while FL automates; Semitone's own lane is shown greyed with "FL is automating this").
- Macros save with the song. Saved macros (a name, controls by role and ranges) also live in ⌘K, so "Energy" can be reused in another song.

## Record with your controller (`5-Record-with-a-controller.png`, values in `reference-record.dc.html`)
Gap found: `nativeMidiIn` (and the browser MIDI path) only reads notes for Capture. Mod wheel, knobs, faders, pedals and pitch bend are ignored
today. Goal: press Record, play, move the mod wheel or a knob, and the lane writes itself, with no setup for the common cases.

**Seamless by default**
- Any MIDI input works: the desktop app (CoreMIDI/WinMM through JUCE), the plug-in (MIDI routed from FL to Semitone, which already reaches
  `st_midiin`), and Chrome/Edge on the web (Web MIDI). Forward every message to the page, not just notes.
- **Defaults with nothing to set up:**
  - **Mod wheel (CC1) → the selected lane.** It follows whatever lane is selected, so pick a lane and play.
  - CC11 (expression) → the part's volume.
  - Pitch bend and sustain (CC64) stay with the notes as expression.
  - Any other moved control → offer "Link Knob 2 to Melody · Reverb send?" once.
- **Pressing Record arms the selected lane** (a red "● armed" tag in its header), and any lane with a linked control can be armed with its own
  "○ arm". Then:
  - Record captures notes and the moves on armed lanes together, and the lane draws live in red, ahead of the old line (shown faint).
  - The red head follows the playhead.
  - "Writing from the mod wheel · Touch" shows above the lane.
- **Write / Touch / Latch** (the existing ● menu) decide what happens when you let go: replace, glide back, or hold.
- Pressing **Capture** after playing also keeps the controller moves from the last 2 minutes, not only the notes.

**A live indicator in the transport** shows which controller is talking and where it's going: device · control → part · lane, a 10-step meter
and the value. It fades out after 3 seconds of no movement.

**The ◎ Controller panel (in the Automation toolbar and in Settings):**
- The device name, with a layout menu ("Semitone default", plus saved layouts and built-in starting layouts for common keyboards that send
  standard CCs).
- The map, one row per control: an icon (wheel, knob, fader, pedal, bend, pads), the name and CC, the target and why ("follows what you select",
  "learned", "Energy macro"), and a live value bar. Rows can be retargeted from a menu or by dragging a lane onto them.
- **Pads** can trigger macro **Snapshots A/B** and other one-shot actions (Drop for the last bar, Back to rest).
- Options:
  - **Soft takeover**, on by default: a knob only takes over when it reaches the line's current value, so nothing jumps.
  - **Tidy after recording**, on by default: thin the take to the points that matter, with **Keep raw** one click away, and say how many
    moves became how many points.
  - **Loop record**: each pass over a loop replaces only where you moved.
- **Learn**: move any knob, fader or pad and it links to the selected lane (or macro, or snapshot).
- The map is saved per device in app settings (shared by the app and the plug-in, like the theme), and a song can override it.

**Expression vs lanes:** the mod wheel on a part whose sound uses it (e.g. vibrato on a synth patch, or a plug-in mapped to CC1) asks once: "Record
the mod wheel into the notes (expression, follows the notes if you move them) or into a lane (shapes the whole part)?" The answer is remembered per
part. Pitch bend and sustain are always expression.

**In FL Studio:** the plug-in gets FL's MIDI when the channel's MIDI input port routes to Semitone, as for notes. The 8 host macro parameters
also mean FL's own controller links can drive Semitone macros (FL → Semitone), and recording in FL writes FL automation clips for them.

**Tests:** fake CC1 while recording writes the selected lane; CC11 → volume; soft takeover never jumps; tidy keeps the shape within 2% and
Keep raw restores every point; loop record replaces only moved regions; Touch glides back; pads fire snapshots; one undo step per take;
the indicator shows and fades; the map saves per device; the plug-in path (`st_midiin` with 0xB0) and Web MIDI both work.

## Split, move, copy and delete ranges (`6-Split-move-copy.png`, values in `reference-split.dc.html`)
Automation is edited like audio clips, not only point by point.
- **Split anywhere:** with ↖ Select, press E (or right-click → Split here) at the mouse or playhead. A brass dashed line and a "✂ Split at 23.1"
  tag appear across the lanes in scope. Splitting adds matching points on both sides so the sound doesn't change. The piece between two
  splits (or any dragged range) becomes a **range** you can work with.
- **Select a range:** drag across the time ruler or empty lane space. Scope (the first chip on the action bar): **All lanes**, **This part's
  lanes**, or **This lane**. Shift-click section names to select whole sections, and double-click a section band to select it.
- **Move:** drag the range. It snaps to bars (⌘ to snap to beats, ⇧ to move freely), and a ghost outline shows where it lands, named
  ("to Chorus 2"). Edges join the neighbours with a short glide unless the range starts or ends with a hold.
- **Copy:** ⌥-drag, ⌘C/⌘V at the playhead, or ⌘D to duplicate right after. **Copy to every Chorus** repeats it in each section of that type.
- **Delete:** ⌫ removes the moves in the range (the lane returns to its resting value there). **Cut** removes and copies.
  **Back to rest** flattens to the resting value but keeps the points' timing.
- **Action bar:** floats under the selection: scope ▾, ✂ Split (E), Cut ⌘X, Copy ⌘C, Paste ⌘V, Duplicate ⌘D, Delete ⌫, Copy to every <section>,
  Back to rest. Every action is one undo step, and it works the same on macro lanes and pass lanes (a pass lane being edited becomes "You").
- **Stretch:** dragging a range's edge with ⌥ held stretches the moves in time (a 4-bar build becomes 8 bars).
- Tests: split keeps the value at the split point; move, copy, duplicate and paste land on bars; scope respected; delete returns to rest; cut/paste
  between lanes of the same unit; stretch; one undo step each.

## Producers use all of this (`data/automation-moves.json`, embedded as `autoMoves`)
Every producer pass now draws its movement moves from this file (47 moves across the 8 producers). Each producer has:
- a philosophy, typical density and how tidy its lanes are, and things it avoids;
- a **macro template** (Antonoff-style "Lift", Max Martin-style "Payoff", Finneas-style "Closer", Mike Dean-style "Sweep", RedOne-style "Drop",
  Nigro-style "Explode", A. G. Cook-style "Flip"; Rubin-style has none on purpose) with controls, ranges and responses, made when the pass runs;
- **moves** in the `auto:<parts>:<target>:<shape>{params}@<where>` grammar (also `macro:` and `tidy:`), each with why, when it fits, what to listen
  for, the coach rules it serves (weighed like every suggestion), the emotion it suits, and **record**: the controller and write mode to perform it by
  hand ("mod wheel on the selected lane, Touch"), shown on the pass card as "Record it yourself";
- emotion use and best song length, so the same producer moves differently in a 1-minute sketch and an 8-minute epic.
Code: `passOp` learns `auto:`, `macro:` and `tidy:` ops and turns shapes into the shape objects above (they stay shapes, ghosted until Keep).
`check_style_data.py` checks every part, target, shape and rule. Tests: each producer's pass on a 3-minute and a 1-minute demo song writes only lanes
the grammar allows, makes its macro, respects the length rules, and every move appears as one switchable Session item.

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
