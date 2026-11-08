# Using the space (agreed with Jamie 6 Nov 2026, after he tried the build)

Before: `before-1440.png`, `before-1280.png`. After: `1-Editing.png`, `2-Arranging.png`, `3-Small-window.png`.
Exact values: `reference.dc.html` (design-canvas file; copy values, not markup).

## What was wrong
- The piano roll started about 2/3 of the way down at 1440×900, and was nearly gone at 1280×800. Three rows of controls
  (sub-tabs, Edit melody/chords + snap + loop + context, a big chord row) sat above it.
- The roll used only the left ~650px. The right half of the drawer held one small "Why it works" card.
- Empty rows: Feels (dots), an empty lyrics row, and blank space under ＋ Add a part.
- The timeline and drawer had a fixed split.
- Below ~1100px wide, panels stacked into a long scrolling page (the drawer was 1283px tall at 1024×700).

## The rules
1. **The editor and timeline trade room.** A drag splitter (8px, grip in the middle) sits between them. Double-click a clip, or press ⤢,
   to give the editor the room. The timeline then becomes an **overview**: 26px per part, names only, the edited bars ringed
   in brass and the rest dimmed. ⌘J (or ▾) folds the editor to its 34px tab line, and the parts then grow to fill the height
   (arranging). The split is remembered per lens.
2. **One toolbar row in the editor (34px):** selection menu ("Verse 1 · Melody ▾"), tabs (Notes Words Sound Mix Ideas), then the
   tools of the open tab inline (Notes: Melody/Chords, Snap, Loop, In context, Scale lock, Ghosts, − bars +, ⋯ More), then ▾ ⤢.
   No second row. Piano roll / Beat / Automation become entries in that row or in ⋯.
3. **Chords live in a 22px strip on top of the roll,** aligned to the bars (name and roman numeral). Click to change, drag an edge to
   resize. This replaces the big chord blocks. "Edit chords" uses the same strip.
4. **The roll gets the full width.** The keyboard is 40px, rows are 15px (13px in small windows), and the roll auto-fits the notes' range
   vertically on open. A 46px lane underneath shows syllables (Words) and velocity. Help cards leave the drawer.
5. **"Why it works" and every other help card goes to the Session panel,** pinned at the top as "Selected · Verse 1". The Session
   panel is 300px (not 340) and its list scrolls.
6. **No empty rows.** The emotion word and its colour go inside each section pill (a 2px underline in the emotion colour) and the Feels row
   is removed. Lyrics show inside the Melody clips when parts are tall, and in the editor's Words lane otherwise. ＋ Add a part
   shows under the parts only when there's room. Otherwise it goes in the selection menu and ⌘K.
7. **Track height fits the space:** in arranging, parts share the available height (min 40px, max 110px). In editing they are 26px. Every
   row height fills the window, with no blank bands.
8. **Small windows never stack or scroll the page:**
   - Below 1280px: Session folds to its 46px rail automatically (badges show counts), and the recipe becomes one chip
     ("Lana · UV + Antonoff ▾").
   - Below 1100px: ⌘K becomes an icon, the time display hides key/tempo, and the parts' header column is 128px.
   - Below 800px tall: the editor opens over the timeline as a sheet (the overview stays visible above it).
   - The plugin window inside FL Studio is the target case: 1024×680 must look like `3-Small-window.png`.
9. Header 46px and transport 38px (was 54 + 48).

## Tests
- At 1440×900, 1280×800, 1024×680 and 900×600: no page scroll, no element outside the window, and the roll is at least 45% of the window height
  when editing.
- The splitter, ⤢ and ⌘J states, remembered per lens.
- Auto-rail below 1280px. Reopening it by hand is respected until the window is resized again.

## A volume knob on every part (8 Nov 2026, `2-Arranging.png`, `1-Editing.png`, values in `reference-arrange.dc.html`)
Every part's header on the timeline (Melody, Chords, added parts, layers, Bass, Drums, loops, audio parts) gets the same LED-ring knob as
Automation, coloured in the part colour:
- 34px in arranging (with the dB value under it and a stereo meter beside M/S), 22px in editing, and 20px in small windows (value on hover).
- Drag up/down to change, ⇧ for fine steps, double-click for 0 dB, and type a value on ⌘-click. The value is the mixer fader's value (same number, both
  places).
- A brass dot on the knob means the part has a volume lane. The knob then shows the value under the playhead and moves as it plays, and turning it
  sets the trim (as the mixer fader does), so it never fights the automation.
- With a controller, faders/knobs can be linked to part volumes (the default layout links four faders to the first four parts).
