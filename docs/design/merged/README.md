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
6. **Popovers:** clicking a track's name opens one sound picker (agreed with Jamie 7 Nov 2026): search, suggestions for
   this song, your plug-ins, built-in sounds and samples, on every track including Drums. A click changes the sound and
   plays a test note; the picker stays open to try more. The Sound tab lists the same tracks with suggestions and plug-in
   controls under them, so there is no separate Real sounds screen. ⌘K opens a floating actions list.

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

## Themes (agreed 6 Nov 2026)
Five themes. Studio Black is the default (described under "Look" above). The others are Console (Pro Tools meets Ableton),
Warm studio, Daylight (modern Apple/iOS, light) and Riso zine (creative, light). Pictures: `theme-*.png` and
`6-Themes-and-collapsed-panel.png`. Every value is in **`themes.json`**.
- Build the interface on CSS variables (`--st-bg`, `--st-accent`, `--st-melody`, `--st-radius`, `--st-font-ui`, …), one per
  token in themes.json. No colour, radius or font stays hard-coded in a rule. A theme is just a `data-theme` attribute on
  `<html>` that sets those variables. Track colours, the clip fill strength (`clipAlpha`) and note colour on clips (`noteOnClip`)
  are tokens too, so Console's solid clips work.
- Some themes change shape, not just colour: Console uses square corners and bevelled buttons; Daylight uses pill buttons, larger radii
  and green switches; Riso zine uses ink outlines, hard offset shadows and a halftone background. Do this with per-theme rules
  on a few classes (`.ghost`, `.prim`, popovers, switches). Don't fork the markup.
- Picker: a ◐ button in the header opens the Theme card (a preview tile per theme, the active one ringed) with
  "Follow the system: light by day, dark at night", which switches between Studio Black and Daylight with the OS appearance.
- The choice is saved locally (app settings, not the song) and applies at once with no reload. Inside a DAW the plugin window
  uses the same setting.
- Fonts: bundle them in the app (no network at runtime). Check each theme for contrast (text on panel ≥ 4.5:1).

## Collapsible Session panel
- A ⟩ button in the Session header (and ⌥⌘S) collapses the panel to a 58px rail. The timeline and drawer take the space.
- The rail shows one icon per helper with a count badge (Producer 5, Song doctor 2, Sound 1, Emotion 1). Clicking an icon
  opens the panel filtered to that helper. ✦ at the bottom opens "Describe a change". ⟨ reopens the panel.
- The numbered pins stay on the timeline while the panel is collapsed. Clicking a pin opens the panel at that item.
- The open/closed state is saved locally, per lens (e.g. open in Produce, closed in Mix).

## Add a part, layers and duplicates (agreed 6 Nov 2026)
Pictures: `7-Add-a-part.png` (the menu) and `8-Parts-added.png` (after). Today Compose has fixed parts (Melody,
Chords, Bass, Drums); extra sounds only exist as per-section chord layers (`S.secLayer`). This makes parts open-ended.
- **＋ Add a part** sits under the last track header (and in ⌘K). It opens a two-column menu:
  - **Write it for me** (fits the song's key, chords and sections, suggest-and-accept as usual):
    - Counter melody: answers the melody in the gaps where it rests or holds, mostly a 3rd or 6th away, and avoids clashing
      with long melody notes.
    - Second bass: sub, octave or synth bass under Bass.
    - Harmony / backing vocal: a 3rd or 6th with the melody, using the lyrics.
    - Arpeggio from the chords.
    - Pad or strings bed.
    - Extra percussion.
  - **From the selected part:**
    - *Layer it (linked)*: same notes, a new sound.
    - *Duplicate it*: an independent copy.
    - *Move it an octave*: a linked copy up or down.
  - **Empty:** *Pick an instrument…* (built-in sounds or your plug-ins), *Audio part* (record).
- **Linked layers** sit indented under their parent, with a LINKED tag and the parent's notes drawn in the layer's colour.
  A layer has its own sound, octave shift, level, pan, effects and plug-in, but no notes of its own: edit the parent and
  the layer follows. "Unlink" turns it into a normal copy. A layer can be switched on per section (e.g. strings only in choruses).
- **New generated parts** get a NEW tag and brass outlines until the user keeps them. Regenerate gives another take,
  and each part is one undo step.
- When a generated part is selected, the drawer shows it with Melody and Chords as ghost notes, plus a short "why" card.
- Parts are a list in the song (`S.parts` of {id, name, role, sound, linkTo?, octave?, sections?}). The four originals
  become ordinary entries. Producer passes, emotion, mix, MIDI, stems, the native engine (`nativeSoundInfo`) and the
  plug-in host must all handle any number of parts, each on its own MIDI channel. Old songs load unchanged.
- Helpers can suggest parts too (Session: "Strings layer only in the choruses", "Bass 2 doubles the root: thin it in verses").

## The songwriter's screens (agreed 6 Nov 2026)
Pictures: `9-Start.png`, `10-Words.png`, `11-Versions-and-Ideas.png`, `12-Pitch-and-share.png`.

**Start (new song).** "What are we writing?" followed by the recipe as one big editable sentence: *A song like [style · era]
produced like [producer] that feels [emotion arc] like a [vibe] in [key] at [tempo] for [purpose]*. Each value is a
dashed-underline word that opens the existing tag picker. "Or start from" chips: a hummed melody, a chord loop, a lyric, a
reference song, a loop. Then **Start the song** and **Surprise me**. Below that: recent songs (with a mini arrangement bar and their
recipe), the Ideas shelf, and Keep learning (a lesson built in the last song's key, plus a tip).

**Words (drawer tab).** A section list on the left. Each lyric line is in the serif face with:
- **Syllables vs the melody's notes** for that line (green when they match, brass when they don't).
- **Stress vs beat** dots: big for stressed syllables. A blue ringed dot marks a stressed syllable on a weak beat, which is
  the song doctor's "lands on a weak beat", shown in place and highlighted in the line.
- **Rhyme letter** (A/B/C, coloured by group), plus a rhyme-scheme card ("A B C A … try A B B A").
- **Rhymes** for the selected word: true rhymes first, then near rhymes marked "near". Use a local rhyme dictionary
  (bundled, e.g. CMUdict-based), not a web service.

**Versions.** A "v5 ▾" chip next to the song title. A version is saved automatically on every big change (a kept pass, an
added part, a key change, a rewrite of a section), with a plain name ("Antonoff-style pass kept"). The user can name and star
one too (⌘S). Actions: **Compare vN with now**, which A/Bs the same bar in both and switches instantly, and **Open as a copy**.
Versions live in the song file (a list of `projectData()` snapshots, deduplicated). This is separate from undo.

**Ideas shelf (drawer tab "Ideas", also on Start).** Everything not in a song yet: Captures, phone voice memos, loops,
chord ideas, lyric lines, beats. Each card shows a mini waveform or notes, a title and where it came from. Drag a card onto a
section to use it (it fits the key and tempo), or "Start from this" on the Start screen. Stored locally in Documents/Semitone
Studio/Ideas. Phone capture needs hosting, so ask Jamie first. Until then, memos can be dropped in from Finder.

**Pitch & share (from Share).**
- **Pitching to [artist]** with a **range check**: a keyboard showing the artist's usual range (from data in
  `style-library.json`: add `range` per artist/era with a source note; mark it as a guide), the melody's range, and the
  highest and lowest notes that fall outside. Suggest the nearest key that fits ("D minor (−2)") with **Hear it in D minor** and
  **Add D minor demo**. The check reads only the user's notes.
- **The pack** (checklist): demo mix (WAV + MP3, loudness matched), alt-key demo, lyric sheet PDF with chords above the words,
  chord chart PDF, stems (including plug-ins), MIDI, and song notes (key, tempo, structure, credits). **Make the pitch pack**
  writes a folder. Share links need hosting, so ask Jamie first.
- **Lyric sheet preview** on paper: title, key, tempo, length, "Words & music: …", "Demo: guide vocal", and sections with chords.
- **Credits & splits** saved with the song (writer names and percentages), printed on the sheet and in the notes.
- The demo uses the built-in neutral guide vocal or Jamie's own recording, never an imitation of the artist's voice.

## Song length, the coach, the Review and the degree (agreed 6 Nov 2026)
Pictures: `13-Start-length.png`, `14-Review.png`, `15-Degree.png`.

**Song length** is part of the recipe ("… about [3 minutes] for …"). Clicking it opens five cards (~1, ~3, ~5, ~8 and
10+ minutes), each with what it's for, its shape and a mini structure. It's a guide, not a limit. Data: **`data/song-shapes.json`**
(embedded as `songShapes`). Each length has structures, `hookBy` (seconds), intro length, chorus repeats, bridge, how fast new parts
arrive, lyric line targets, energy shape, outro, and what the pitch pack does. It steers "Open with a song ready",
Surprise me, producer moves on intros, outros and repeats, emotion arcs, lyric prompts, the Review and the radio-edit offer. Saved in the song.

**The coach: `data/coach.json`** (embedded as `coach`). Every suggestion is weighed in five layers: theory, style, producer, you
and this song. Strength = base × style strictness × producer strictness, nudged by what Jamie keeps or dismisses (stored locally), shown at ≥ 0.35.
Every card names the layer that led ("Theory", "Lana · Ultraviolence", "Antonoff-style", "You usually…", "For a 3-minute single").
- `producers`: strictness, density, risk, priorities, tone, and which rules each loosens or tightens. This is the chameleon: the
  producer chosen in the recipe changes how strict and what kind the advice is.
- `rules`: 23 rules across lyric, form, melody, harmony, rhythm, arrangement, production and mix. Each has a reason, per-style `loose`/`tight`
  overrides, and `breakIt` notes for when a style breaks it on purpose (e.g. parallel 5ths in rock).
- `jury`: the **Review**. One button scores eight items (hook, prosody, contrast & form, harmony, melody, lyric, arrangement, mix)
  1–5 against the style and length, with **one top priority** (Try it, Show me, Lesson) and the layer behind each item.
  Voices: Teacher, Co-writer, Critic, or the selected producer. The Review also shows "How this review was weighed" (layer weights) and
  "Change the producer, change the advice" (the same song read by three producers).
- `adapt`: mastery per concept (local). The coach explains below 0.3 and only names the concept above 0.8. It's honest, with no inflated praise.
- `curriculum`: **Your degree**, ten subjects (harmony, counterpoint, melody, lyric craft, form, arranging, rhythm, production,
  ear training, music psychology), each with lessons built from Jamie's songs and in their keys, plus ear training on his own bars and the next lesson
  picked from the Review's top priority.
- `golden`: a test in `tests/web/`. Run the rules over every catalog song in its own style. A rule that flags more than 25% of a style's songs
  is too strict there, so fix its `loose` value. Catalog songs are the style's norm, not mistakes.

**Contrast, constraint games and the skeleton test (from A. G. Cook's published workshop outline and interviews, 6 Nov 2026).**
New rules in coach.json: `song-survives`, `element-role`, `avoid-middle`, `paradox`, `sound-hook`, `concise`, `keep-the-demo`. All seven are tightened by the A. G. Cook-style producer, and Rubin-style also checks `song-survives`.
- **Skeleton A/B:** a transport toggle that plays only the melody and chords on a plain piano, so you can hear whether the song survives
  without production.
- **Keep the demo:** the Review compares the first version with now and flags anything exciting that was lost (a sound, a chord, a moment).
- **Minimal ←→ maximal dial** on the recipe (`coach.dials`). It sets density, how many parts get suggested, and space vs build. Each producer sets the default.
- **Play a game** (`coach.games`, on Start, in ⌘K, and offered when a song is stuck): seven sounds, one synth, roll the dice, three chords,
  twenty minutes, opposites, write as your alias (a saved persona with its own recipe and rules), remix yourself. The app enforces
  the constraints and the Review respects them.

## Quality checks for every step
- **Design check:** after each build step, screenshot the real app at 1440×900 in every theme and compare it with these boards.
  Fix any drift before moving on (spacing, type, colours, one filled button per area, no bright saturated blocks).
- **Contrast:** text on panels must be at least 4.5:1 in every theme. Track colours also get a small part icon, so they don't rely on colour
  alone (colour-blind users).
- **Big songs:** test with 24 parts and a 6-minute song. Scrolling, playback and the drawer must stay smooth (under 16 ms per frame
  when idle, no dropped audio).
- **Keyboard:** every action reachable by keyboard, with visible focus rings in the accent colour, and a ? overlay listing the shortcuts.

## Order to build
1. Look tokens as CSS variables across the existing UI, Studio Black first, then the theme picker and the other
   four themes from `themes.json`. This is the cheapest and most visible step.
2. Header with the recipe sentence and lens switch, plus the transport. Remove the second toolbar.
3. Lenses on the one timeline. Move Producer pass, Emotion & vibe and Real sounds onto it and into the Session
   panel, the drawer and popovers. Delete their separate screens and the duplicate timelines. Replace the 11-tab dock with
   the 4-tab drawer (Lessons/Theory move into "why it works" cards and ⌘K).
4. Session panel as the single feed for every helper.
5. Open-ended parts: Add a part, linked layers, duplicates, counter melody and second bass.
6. Capture, the takes lane, ghost notes and scale lock, ⌘K actions, section drag, mix snapshots.
7. Loops: drop, paste and fit, then Splice Bridge recognition, the hidden audition track and host sync.
8. Start screen with the recipe sentence, then the Words tab, then Versions and the Ideas shelf.
9. Pitch & share: range check (add artist ranges to the data), the pitch pack folder, the lyric sheet and credits.
10. Song length in the recipe (song-shapes.json) wired into structure, passes and prompts.
11. The coach: layered weighing (coach.json) behind every existing suggestion, then the Review, the golden-songs test, mastery, and Your degree.
Each step needs tests in `tests/web/` and a plain-words note for Jamie, and must keep working as a plain web page.
