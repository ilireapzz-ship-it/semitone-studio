# The merged design (approved by Jamie, 6 Nov 2026)

This replaces the older screens in `docs/design/1-*.png … 7-*.png` wherever they disagree.
Pictures: `1-Write-lens.png`, `2-Produce-lens.png`, `3-Mix-lens.png`, `4-Loops-and-Splice.png`, `5-Notes.png`.
`reference.dc.html` is the source of the pictures (a design-canvas file, not app code). Its inline styles give exact
colours, sizes and spacing, but don't copy its markup.

## Why
The app felt fragmented. Features lived on their own screens, each with its own copy of the timeline. There were two toolbars,
20+ buttons of equal weight, and an 11-tab dock. The recipe was a tiny chip, and every helper had its own card style.

## Four rules
1. **One timeline.** Producer changes, emotion, automation, sound swaps, lyrics, takes and loops all draw on the song.
   No screen keeps a second copy of the timeline.
2. **The recipe is a sentence** in the header: "Lana Del Rey · Ultraviolence produced like Jack Antonoff feels
   melancholy → euphoric like a late-night drive". Each value is a dashed-underlined word you click to change.
3. **Select, then edit.** Clicking a section, part, note, change or loop makes the one drawer and the one side panel follow it.
4. **One voice for help.** The producer, song doctor, sound finder, mix check and emotion all post to the Session panel
   in one style. Each item is numbered, and the same number pins it on the timeline.

## Layout (top to bottom, left to right)
1. **Header (54px):** logo, song title (serif), the recipe sentence, then "Search or do anything ⌘K", then the lens
   switch: **Write · Produce · Mix**.
2. **Transport (48px):** back to start, play/pause, an inset display (time, bar.beat.tick, key, tempo, metre), then
   whatever the lens adds: A/B in Produce while a pass is on, mix snapshots in Mix, the Splice Bridge status when
   active. On the right: Undo, **Capture**, Record and Share (the only filled button).
3. **Timeline:** a bar ruler, the arrangement row (sections, draggable by ⠿, with numbered pins), the Feels row
   (emotion per section), then the tracks. Track header: colour bar, name, sound, M/S, and a meter in Mix.
   Clips are tinted in the track colour, with the section name in small type. The lens decides the extra lanes:
   - Write: lyrics and a takes · comp lane under Melody, and chord names in the Chords clips.
   - Produce: changed clips outlined in brass with numbered marks, muted clips hatched ("muted by the pass"),
     automation lanes (e.g. tremolo depth, delay riser), and changed sounds in the track header.
   - Mix: meters, a volume lane, and loudness (LUFS) per section in the arrangement row.
4. **Drawer:** the selection name, then tabs Notes · Words · Sound · Mix (underline tab). Inside:
   - Notes: piano roll with a keyboard, scale lock (scale rows shaded from the recipe key), ghost notes from Chords,
     and syllables under the notes (Write) or velocity (Produce).
   - Mix: channel strips with Jamie's plug-ins as inserts (empty slots dashed), pan, fader, meters and a dB readout.
   - Sound (loops): the loop editor (see below).
   - On the right, up to two explanation cards: the selected item, plus "why it works", emotion or reference.
5. **Session panel (340px, right):** title, filter tabs (All / Producer / Sound / Emotion …), the pass header (Produce),
   numbered items with a switch (pass moves) or one small action (Fix, Lift, Try, Match…), Keep N changes /
   Another take, and the "Describe a change" box at the bottom.
6. **Popovers:** clicking a part's name opens a small card with its sound, the closest match in your library and
   Use it / Sketch sound / Other options. This replaces the separate Real sounds screen. ⌘K opens a floating actions list.

## Ideas borrowed from other DAWs (agreed 6 Nov 2026)
- **Capture** (Ableton's Capture MIDI): always keep the last ~2 minutes of MIDI input, and one button turns it into a
  take or notes at the right place. The button sits next to Record.
- **Takes lane and comping** (Pro Tools playlists): takes stack under Melody, and you swipe across them to pick bars
  (`S.takeComp` already exists).
- **Ghost notes and scale lock** (FL Studio): in the piano roll, show another part faintly and shade or snap to the recipe key.
- **⌘K as an actions list** (Reaper): every command can be searched, and the user can chain steps into their own saved
  action (stored locally).
- **Drag sections** (Studio One arranger): drag a section name to move it; ⌥-drag copies it across every part.
- **Mix snapshots** (Reaper SWS): save and recall the whole mix state, shown as chips in the transport in Mix.
Take the ideas only. Don't copy those apps' look or names.

## Loops and Splice Bridge (only when used)
Nothing about loops shows until the user drops a loop or loads Bridge. Then the Loops track, the transport status and
the Sound drawer appear, and they fold away again when the last loop or Bridge is removed.
- **Splice Bridge** is a VST3/AU instrument that links to the Splice Desktop app. It previews samples in the host's key
  and tempo, and "Copy modified sample" puts the pitched and stretched file on the clipboard.
- **Recognise it:** in `LibraryScan` / `PluginHost`, a plug-in named "Splice Bridge" (vendor Splice) is an *audition
  tool*, not a part. Loading it creates a hidden audition track: it isn't drawn as a part, isn't in the mix strips or
  exports, and is routed straight to the output.
- **Feed it:** host tempo, key and transport, plus loop points on the selected section, so it plays along with
  what you're working on. While Bridge is loaded, the transport shows "Splice Bridge · following E min, 78, looping Chorus 1 · ⌘V".
- **Getting loops in:** ⌘V pastes an audio file from the clipboard at the playhead or the selected section. Dropping
  files from Splice Desktop or Finder onto a section works too.
- **Fitting:** detect the loop's tempo and key, then stretch and transpose to the song, with switches for Fit to song tempo,
  Fit to song key, Follow the chords and Slice into Drums. Show "90 BPM · F♯ min → 78 BPM · E min".
- **Sound finder** items in Session suggest search phrases built from the recipe and section (e.g. "dream pop
  guitar E minor 78"), with a Copy button. Splice has no public API we found. A built-in Splice browser (as in
  Ableton Live 12.3) would need a partnership, so ask Jamie before going further.

## Look ("make it feel expensive")
- Near-black, slightly warm surfaces: `#0A0B0D` base, `#0E0F12` panels, `#121316` raised. Hairlines are
  `rgba(255,255,255,.06)`. Depth comes from a faint inner top highlight and soft shadows, not borders.
- Text `#EDEBE6`, secondary `#9BA0A8`, tertiary `#6B7079`. Small labels are uppercase 10px with wide letter spacing.
- **One accent: brass `#D9B37A`** for the selection, producer changes, active tabs and the playhead head.
  Other colours are muted dots: song doctor `#9DB4E8`, emotion `#B9A3E3`, mix/sound `#7FC9A2`, loops `#6CC3C3`.
- Track colours are desaturated: Melody `#D9776F`, Chords `#7C95D9`, Bass `#D5A35F`, Drums `#74B88F`, FX `#9A9A9A`.
- Type: Inter Tight for the interface, **Instrument Serif** for the song title, panel titles and card titles,
  JetBrains Mono with tabular numbers for time, tempo, dB and LUFS.
- Buttons: one filled light button per area (Share, Keep, Suggest, Use it). Everything else is a ghost button with a hairline.
  Tabs are an underline, not pills. Switches are small (26×14).
- Popovers and ⌘K: translucent dark with blur, a hairline border and a large soft shadow.
- No bright saturated blocks anywhere. A chorus is a slightly deeper tint than a verse, not a bright blue slab.

## Order to build
1. Look tokens (colours, type, buttons, tabs) across the existing UI. This is the cheapest and most visible step.
2. Header with the recipe sentence and lens switch, plus the transport. Remove the second toolbar.
3. Lenses on the one timeline. Move Producer pass, Emotion & vibe and Real sounds onto it and into the Session
   panel, the drawer and popovers. Delete their separate screens and the duplicate timelines. Replace the 11-tab dock with
   the 4-tab drawer (Lessons/Theory move into "why it works" cards and ⌘K).
4. Session panel as the single feed for every helper.
5. Capture, the takes lane, ghost notes and scale lock, ⌘K actions, section drag, mix snapshots.
6. Loops: drop, paste and fit, then Splice Bridge recognition, the hidden audition track and host sync.
Each step needs tests in `tests/web/` and a plain-words note for Jamie, and must keep working as a plain web page.
