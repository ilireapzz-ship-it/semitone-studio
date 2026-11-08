# Semitone Studio — notes for coding agents (Codex and others)

**Read `CLAUDE.md` first.** It is the project guide shared by every agent: layout, the web ⇄ native bridge,
how to build and test, the sound engine rules, and FL Studio lessons learned the hard way. Everything there applies to you.

## BUILD QUEUE (6 Nov 2026): start here
Jamie approved a full redesign plus new "brain" data. Build it in this order. Each step ships on its own: small commits,
tests in `tests/web/`, the page still works as a plain web page, the Mac + Windows CI run passes, and a plain-words note for
Jamie in `docs/design/merged/PROGRESS.md`. Tick the step there when it's done, then go to the next one without waiting.
Spec and pictures: **`docs/design/merged/README.md`** (it wins over the older `docs/design/N-*.png` and the notes further down).
Exact look: `docs/design/merged/reference.dc.html` (a design-canvas file: copy its values, not its markup) and `themes.json`.

0. Fix the 3 ISSUES from `python3 tools/check_style_data.py` (empty `src` on `plug-sweep`, `plug-dark-verse`, `plug-climb`).
1. **Look:** CSS variables for every token in `themes.json`. Studio Black first, then the ◐ theme picker and the other four themes,
   including "follow the system". Saved locally, no reload needed.
2. **One header and transport:** the recipe sentence (style, producer, emotion, vibe, key, tempo, **length**, purpose), the ⌘K box,
   Write / Produce / Mix lenses, the inset time display, Capture / Record / Share. Remove the second toolbar.
3. **One timeline with lenses:** move Producer pass, Emotion & vibe and Real sounds onto the timeline, into the Session panel, the drawer and
   popovers. Delete their separate screens and duplicate timelines. Replace the 11-tab dock with the drawer (Notes, Words, Sound, Mix, Ideas).
4. **Session panel:** the single feed for every helper. It collapses to a rail (⟩ / ⌥⌘S), with open/closed remembered per lens.
5. **Open-ended parts:** Add a part, linked layers, duplicates. Write it for me: counter melody, harmony, second bass, arp, pad, perc.
   All of it driven by `data/part-roles.json` (see below), with every feature picking parts by role.
6. **Borrowed DAW ideas:** Capture, the takes lane and comping, ghost notes and scale lock, ⌘K actions (saved chains), section drag, mix snapshots.
7. **Loops and Splice Bridge:** drop, paste and fit, the hidden audition track, host sync. Nothing appears until it's used.
8. **Songwriter screens:** Start (the recipe sentence and the length cards), Words (syllables, stress, rhyme), Versions (v-chip, compare, open as a copy),
   the Ideas shelf.
9. **Pitch & share:** range check (add `range` per artist/era with sources), the pitch pack folder, the lyric sheet PDF, credits and splits.
10. **Song length:** `data/song-shapes.json` wired into structure, passes, emotion arcs, lyric prompts and the radio-edit offer.
11. **The coach:** `data/coach.json` five-layer weighing behind every existing suggestion (the card names the layer that led), producer strictness,
    the golden-songs test, then the Review, the skeleton A/B, keep-the-demo, the minimal ←→ maximal dial, Play a game and aliases,
    mastery, and Your degree.
12. **Using the space** (Jamie, after trying the build): `docs/design/space/README.md`. The editor and timeline trade room
    (splitter, ⤢ overview, ⌘J), one toolbar row, a chord strip on the roll, a full-width roll, help moves to Session, no empty rows,
    track heights that fill the space, and small windows that never stack (auto-rail, the 1024×680 plugin window). Do this one next.
13. **Automation, redesigned** (Jamie, 8 Nov): `docs/design/automation/README.md`. A whole-song view of every lane with reasons, shapes
    that mean something, Add a lane in plain words, editing details (bends, exact values, Wobble as an object, Write/Touch/Latch, MIDI
    controller), macros, fader trim, 8 host parameters so FL can automate Semitone, **recording automation from any MIDI controller
    (mod wheel follows the selected lane, CC11, knobs, pads → snapshots, soft takeover, tidy after recording)**, and the 15 gaps listed there.
14. **Automation range editing and producer training** (8 Nov): split anywhere, move/copy/duplicate/delete ranges with scope, stretch
    (`docs/design/automation/README.md`, "Split, move, copy"), and producer passes that use `data/automation-moves.json` (auto:/macro:/tidy: ops,
    macro templates, "Record it yourself").
15. **A volume knob on every part** in the timeline headers (`docs/design/space/README.md`, end).
16. **Song settings, Record, and the mixer with your DAW built in** (`docs/design/panels/README.md`): the DAW & MIDI tab goes away, every strip
    says what it plays through, a Your DAW card, theme rules (no fixed dark overlays, `inkSoft` tokens) checked in every theme.
17. **Smart EQ, Calibrate, Fix what I hear** (8 Nov): `docs/design/mixing/README.md` and `data/mix-master.json` (12 sourced mix and mastering
    engineer profiles, best per style, spectrum by role, interactions, 22 problems, destinations). Also: numbered channels, auto-routing to buses
    and returns, and level zones on strips (`docs/design/panels/README.md` sections 3–4).
18. **Smart knob and real per-part compressor** (8 Nov): each strip has Pan, Width and one smart knob whose target follows `smartKnob` in
    `data/mix-master.json` (producer > style exception > role; changeable, Back to smart), and the Comp bar becomes a real one-amount compressor
    (`comp` in the same file, port to `Source/SynthFx.h`). Spec: `docs/design/panels/README.md`, strip layout. Test: bass never gets a reverb pick
    unless a sourced exception applies.
Rules that always hold: suggest and accept (never surprise changes), one undo step per change, learning about Jamie stays on his
computer, no copied melodies, lyrics or recordings, no imitating or cloning a real singer's voice. Producers and artists are described
as "X-style, based on how they're known to work". Ask Jamie before anything that needs hosting, accounts or money.

### The brain for open-ended parts: `data/part-roles.json` (6 Nov 2026)
With any number of parts, every smart feature has to reason by **role**, not by the four fixed parts. The file
(embedded as `partRoles`, checked by `check_style_data.py`) holds:
- `roles`: what each kind of part does (lead, counter, harmony, double, chords, pad, arp, bass, bass2, drums, perc, texture,
  loop, audio), its register, density, relation to the lead, sections, and mix defaults (level vs lead, pan, EQ).
- `generate`: the rules "Write it for me" follows for counter melody, harmony, second bass, arp, pad, perc and layers. A counter
  melody moves in the lead's gaps, stays a 3rd or 6th away, uses contrary motion, keeps its own register, and echoes the hook.
- `doctor`: new song-doctor checks between parts (clash, mask, busy, parallel, mud, crowd, noroom).
- `emotion`: which parts each emotion adds or removes (all 20 emotions in sound-library).
- `partMoves`: the grammar producer passes and emotion moves use to add, layer, drop or thin parts:
  `add:counter:answer@verse2,chorus`, `layer:lead:i:strings@chorus`, `drop:bass2@verse`.
- `styleParts` (all 26 profiles) and `producerParts` (all 8 producers): the extra parts each style and producer tends to use,
  with moves. These are habits, based on how they're known to work, never copied lines.
What to change in code when open-ended parts land (design step 5):
- Parts carry a `role`. `PASS_PART`, `passOp`, `FEEL_DO`, the song doctor, the mix, `nativeSoundInfo`, MIDI/stems export and
  `PartHost` work over every part and pick parts by role.
- `passOp` understands `add:` / `layer:` / `drop:` / `thin:`. Each added part is a suggested move with its own switch and its own undo step.
- Profiles' free-text `arrangement.extras` stay for the cards; `styleParts` is what code reads.
### Song length and the coach (6 Nov 2026)
- `data/song-shapes.json` (`songShapes`): the recipe asks "about how long?" (~1/3/5/8/10+ min). Each length sets structure, hook timing,
  build rate, lyric amount, energy, outro and pitch-pack behaviour.
- `data/coach.json` (`coach`): five-layer weighing (theory, style, producer, you, song) for every suggestion, per-producer strictness,
  23 rules with per-style overrides, the jury Review, mastery and the ten-subject curriculum, plus the golden-songs test.
  See `docs/design/merged/README.md` (steps 10–11).
Also fix: `check_style_data.py` reports 3 ISSUES from the plug-in producer moves (`plug-sweep`, `plug-dark-verse`,
`plug-climb` have empty `src`). Give each a source from that producer's existing sources, or mark it as a stand-in the
way other guesses are.

## Start here (handover, 5 Nov 2026)
You can't open Jamie's claude.ai design canvas or plan doc, so copies are in the repo:
- **Design:** `docs/design/1-Start.png` … `7-Sounds.png` are the target screens (Start, Song view, Record, Mix,
  Style tags, Producer pass, Real sounds). Where this file says "design canvas board N", it means `docs/design/N-*.png`.
- **Plan:** the product decisions and ideas below are the complete plan; nothing else is needed from the plan doc.
- **Data:** `data/style-library.json` (styles, producers, passes, eras, variants), `data/progressions.json` (chords per
  song), `data/catalog.json` (artists → albums → songs with tags), `data/sound-library.json` (real instruments,
  effects, emotion and vibe rules). After any data change run `python3 tools/build_style_matrices.py` and
  `python3 tools/check_style_data.py` (ISSUES must be 0), then `python3 tools/embed_data.py`, which copies `data/` into the HTML (the app only sees what is embedded).

**Suggested order** (each step ships on its own, with tests and a plain-words note for Jamie):
1. Phase 1 task 1, **undo for every action**: done (`H` / `histCheck` in the HTML, `tests/web/undo.test.js`). A new setting is undoable once it is in `projectData()`.
2. Done for Style library steps 1–3, 6, the Lana eras and genre variants and the song-tag filter (`tagSongFilter`, `tagMatrix`, same counting as the script). **Style tags** replace the style picker (Style library steps 1–2): load the JSON, chips, search, blend rules, the
   "what your tags do" card. Then the new profiles, artist eras and genre variants (steps 3, 7, 29), applying
   `existingStyles` to the original 8 (step 6).
3. **Producer pass** (25): first version done (`PASS`, `passOp`). `S.secLayer` layers up to 2 extra sounds on a section's chords (page audio only, not MIDI or the native engine). A move runs when `passOp` understands its op; the rest show "coming soon". Parts mute per section through `S.secMute`. Still to do: the other ops, `fxStyle`, the emotion rules (28).
4. **Emotion and vibe** (28): first version done (`FEEL`, `FEEL_DO`): 10 of the 20 moves draw real lanes; effect moves take a free effect slot (`feelSlot`, marked `emo`), the rest show "coming soon".
5. **Real sounds**: first version done (`SND`, `Source/LibraryScan.h`): suggestions, recipes, opt-in scan, part + recipe export. Sample packs are not scanned yet.
6. Engine additions the styles ask for. Done: swing, shuffle and train grooves, snare roll and silence transitions, accordion, upright bass. Still to do: real ride cymbal (swing uses the hat), drones, flanger, stutter, tempo-drop, other instruments.
7. The rest of Phase 1, then the "Smarter" ideas (1–11) and Phase 2.
Still being researched in a Claude session, don't duplicate: album-by-album catalogs (with tags and eras where needed)
for the other artists and composers, added to `data/catalog.json` and progressions.json the same way as Lana Del Rey.

## Working rules
- Several agents work on this repo (Codex, Claude Code on Jamie's Mac, a cloud Claude session). Run `git pull`
  before you start and before you push; keep commits small and self-contained; push to `main`.
  Every push builds Mac + Windows in GitHub Actions: check that the run passes.
- Build and install on the Mac with `tools/build-mac.sh`. Run the C++ tests (see CLAUDE.md) and the interface
  tests in `tests/web/` (see its README) before pushing. Add a test for anything new.
- The interface is one file, `web/semitone-studio.html`. It must keep working as a plain web page
  (no `window.__JUCE__`), in the desktop app and inside a DAW.
- Jamie is a songwriter, not a programmer: report what changed in plain words, and what he should try.
- Never delete Jamie's files; move them to a `_to_delete/` folder and say so.
- Don't change the product decisions below without asking Jamie.

## Product decisions (from the DAW plan)
Standalone DAW first, plugin as a companion · Mac and Windows together · recording for demos first, then
release-quality mixing and mastering · AI vocals: a neutral built-in guide vocal plus export to licensed tools,
**no cloning of real artists' voices** · for everyone, Classroom is the way in for beginners · smart features
suggest and you accept, never surprise changes.

## What's left: the plan, in order

### Finish Phase 1 (the song-first app)
1. **Undo for every action.** Done: one song-level history for Compose and Orchestral (⌘Z / ⇧⌘Z, plus buttons), labelled, last 100 steps.
   Notes, chords and lyrics restore in place so playback continues; other changes stop playback.
2. **Open with a song ready.** Done (`demoSong`).
3. **Windows check.** The Windows build has never been run on a PC. Go through docs/INSTALL-WINDOWS.txt
   when a PC is available; fix what breaks. (FL markers need loopMIDI on Windows; nothing else should be Mac-only.)
4. **Engine gaps** (deferred 5 Nov 2026: risky without a listen, and rendered sections already cover these) (Source/Synth.h), so the plugin sounds complete with its window closed: per-part effects,
   mix automation, transitions, recorded takes, samples and SoundFonts, orchestral instruments.

### Phase 2: record the song
First version done: native takes in the desktop app (`Source/TakeRecorder.h`, `nativeTakeStart`), takes follow their section (`takeKey`, `takeSi`), Use this one, a Record button, punch-in from the playhead, a timing offset. Desktop takes are listed in the song and reload from their WAVs; `S.takeComp` picks a take per bar; Make melody turns a take into notes (`takePitch`). Record still can't tell humming from singing by itself.
- Audio tracks for vocals and guitar, placed by section (recordings belong to a section, so they follow the structure).
- One Record button: it knows whether you're humming (→ melody), singing, playing keys or recording an instrument.
- Takes, comping and punch-in for each section; latency compensation.
- In the standalone app, record from the audio input natively (JUCE), not through the web view; the page shows
  waveforms and controls. Inside a DAW, leave recording to the DAW.

### Later phases (see the plan doc)
3 your VST3/AU plugins inside Semitone (first slice done: one instrument per part in the desktop app, `Source/PluginHost.h`, with lanes, producer moves, maps of main controls, Learn a control, Link by MIDI, presets, saved sounds, style starts and your effects; effects on built-in parts, still to do: effects on built-in parts) · 4 mix and master (first versions done: strips with mute and solo, Listen on, Mix check, loudness, reference with energy copy) · 5 share everywhere (MIDI, Reaper, DAWproject and the pitch pack with an alternative key exist; stems don't include plug-ins yet. iPad, share links, listen-and-comment links and phone capture need hosting: ask Jamie first).

## Ideas to build (agreed with Jamie, 5 Nov 2026)
Fit these into the phases as marked; the **design canvas** "Semitone Studio — song-first DAW redesign" shows the
target look (Start, Song view, Record, Mix). Rules for every one of them:
- **Suggest, explain, accept.** A smart feature shows a card with what it would change, why, Try it / Not now.
  Nothing changes silently; everything is one undo step (needs Phase 1 task 1 first).
- "In the style of" means musical features (chords, structure, tempo, sound). Never copy melodies or lyrics,
  never imitate or clone a real singer's voice.
- Learning about Jamie stays on his computer (local storage / project files), never sent anywhere.
- Each idea ships with a test in `tests/web/` and a short plain-words note for Jamie.

### Smarter: Semitone understands the song (Phase 1–2, interface only; start here)
First versions of 1–6 and 8–11 are done, mostly in the Song doctor (`doc*`, `taste*`). 11 moves the song with `songTranspose` (audio takes stay put); 8 suggests a tempo only; 9 is a list, not click-any-chord. 7 is next, once suggestions have been used.
1. **Prosody check.** Flag stressed syllables on weak beats or short notes (needs syllable stress: a small
   English stress dictionary plus rules), suggest moving the note or the word. Shown under the piano roll's lyrics.
2. **Hook strength, explained.** Score the chorus on repetition, contour, rhythm and range lift over the verse;
   always say why in words ("the chorus top note is the same as the verse's"). Part of the A&R check.
3. **"Why does this section feel flat?"** Diagnose from harmony, note density, register and dynamics against
   the energy plan; offer 2–3 concrete fixes as cards.
4. **Arrangement clash detection.** Parts crowding the same range at the same time (e.g. pad and vocal in the
   chorus) → musical fixes: move the pad an octave, thin the chords under the hook.
5. **Fill this gap.** Write a new section (pre-chorus, bridge, intro) from its neighbours: start from the
   previous section's last chord and lead into the next one's first; melody continues the song's motifs.
6. **Change it by describing it.** A text box: "make chorus 2 bigger", "verse 2 different but familiar",
   "darker chords in the bridge". First version is rule-based: map phrases onto existing controls (lift,
   density, voicing, energy, melody variation) and show the result as an undoable card listing what changed.
7. **Taste learning.** Remember which suggestions Jamie tries or skips and rank future ones by that.
8. **Smart defaults.** New songs start from his usual tempo range, keys, sounds and structures.
9. **"Why does this work?" everywhere.** Click any chord, leap or section change for a short, Classroom-style
   explanation using his own song, with a link into the matching Classroom lesson.
10. **Style DNA cards.** Per style: typical chords, tempo range, length, structure; mark where this song sits.
11. **Key and range fit.** Enter a singer's range (or pick a saved singer: just a name and two notes), suggest
    the best key, show where the melody would strain. Notes and ranges only, never voices.

### Writing faster (Phase 1–2)
Done (first versions): 14 command palette (`CMDS`), 15 history list, 16 focus mode, 17 section notes (one short note, no to-dos). 7 ranks palette actions and doctor fixes by use. 12 song versions: done (`S.alts`, `altSwitch`); a switch stays seamless only when notes, chords or lyrics differ.
12. **Song versions side by side.** Whole-song alternatives (Chorus A / Chorus B, ballad / uptempo), switch
    with one click while it plays. Builds on today's verse versions.
13. **Rhyme and syllable help** in the lyrics lane: rhymes, near rhymes, syllable count per line vs the notes,
    a warning when a line has more syllables than notes.
14. **Command palette (⌘K).** Type an action ("lift chorus", "export stems", "record guitar").
15. **Undo history you can see.** A list of every change with labels; click to jump back (on top of task 1).
16. **Focus mode.** Only the lyrics and the playing song, for writing sessions.
17. **Section notes.** Sticky notes and to-dos on a section ("re-sing line 3"), shown on the timeline.

### Recording (Phase 2)
18. **Auto-comp.** (first version: `compAuto`, bar by bar, scores in-tune and on-time against semitones and the written melody; tone not scored yet) After several takes, pick the best take per line by pitch, timing and tone; Jamie swaps phrases.
    Flag pitchy or late spots.
19. **Practice coach.** (first version: `renderCoach`, trends and weak spots from Play along and recorded takes; exercises are a slow loop or another take, not yet Classroom lessons) Track sing/play-along accuracy over sessions; turn weak spots into Classroom exercises
    built from his own songs.

### Mixing (Phase 4)
20. **Reference matching.** Read a reference track's structure, energy curve, tempo, loudness and tonal balance
    (the song analyser does part of this); line up the energy plan and mix targets; show the differences.

### Pitching and sharing (Phase 5, some can come earlier)
21. **Pitch mode.** One screen that builds a pitch package: demo mix, lyric sheet, chord chart, key (plus an
    alternative key for the artist), a short note; one share link. Grows out of today's pitch pack.
22. **Listen-and-comment links.** Co-writers and A&R open a link, listen, comment at a moment in the song.
23. **Idea capture from the phone.** A voice memo that arrives as a new song sketch with key, tempo and melody detected.

### Producer pass (agreed with Jamie, 5 Nov 2026; build right after Phase 1 task 1, undo)
25. **Ask a producer about your finished song.** Flow: pick a style → find a song you like → pick a producer → Semitone
    shows that producer's likely moves for this song as **session notes** (design canvas board "6 · Producer pass").
    - Moves come from `producers[].pass` in style-library.json (rules in `producerPass`): one line each, with the reason,
      a number that also marks the spot on the timeline, and an on/off switch. Muted parts show hatched, changed ones outlined.
    - **A/B** switches between your version and the pass while the song plays. Keep N changes / Try another take / Discard.
    - **How far:** sounds & mix · + arrangement · + writing. Only "+ writing" moves may change melody or chords, and the card says so.
    - The pass is one undo step, and each kept move is its own step, so any of them can be undone later.
    - Wording: "Rick Rubin-style pass, based on how he's known to work". Never quotes or lines in the producer's voice.
    - Producer tags picked *before* generating still set the production layer (blend rules); the pass is the after-the-fact version.
    - Moves the engine can't do yet (stutter, found sounds, tempo curve per section) stay listed, greyed, "coming soon".
    - Each pass also has **firstListen** (shown first, with a button to hear it, e.g. Rubin's bare version) and **asks**
      (two questions for the user). Moves carry `c` and `src`; `onByDefault: false` moves are offered but start off.
    - The card shows where each move comes from in a few words ("from his live-band Adele sessions"), and low-confidence
      moves say so.

### Real sounds, emotion and vibe (agreed with Jamie, 5 Nov 2026)
26. **Point to real instruments instead of competing with Omnisphere or Serum.** Semitone's own synths are for sketching.
    `data/sound-library.json` lists 31 production instruments and 15 effects: who is known to use them and how (sourced),
    software that recreates them, a patch **recipe** (what to set), and Semitone's stand-in. Sound suggestions (producer
    pass, Sounds tab, style info) name the instrument, the recipe and the software. **Never invent preset names.**
27. **Scan my plug-ins** (desktop app, opt-in): read plug-in names and preset file names from the folders in
    `libraryMatch` (names only, kept on the computer) and match them to recipes by keyword, so a suggestion can say
    "in your library: Omnisphere → …". "Use in my DAW" exports the part as MIDI plus a note with the recipe.
28. **Emotion and vibe drive effects and automation.** A song (and each section: an emotional arc) carries emotion
    tags; the song carries vibe tags (catalog `tagVocab.emotion` / `.vibe`; sound-library `vibes`). `emotionRules` give
    the baseline automation moves per emotion (use / avoid), the producer's `fxStyle.byEmotion` says how that producer
    handles it, and a vibe suggests tempo, sounds and effects. Show automation as lanes with a one-line "why"
    (design canvas board "7 · Real sounds, emotion and vibe"). Emotion→move rules are Semitone's own (`c: low`): tune them.

### More genres and Jamie's own library (5 Nov 2026)
29. **New genres:** Cinematic, Country, EDM, Psychedelic (`profiles[]` kind genre, chord data from 8–13 songs each in
    progressions.json). Each has `variants` (Cinematic: emotional / epic trailer; Country: classic / modern pop-country;
    EDM: progressive / big room / future bass; Psychedelic: 60s / modern psych-pop) shown as chips under the genre tag,
    `sounds` (sound-library ids), `fxStyle` (effects and how emotion changes them) and `typicalEmotions`.
    Engine additions they ask for: train-beat and shuffle grooves (country), snare-roll and silence transitions (EDM),
    a drone part and phaser/flanger effects (psych), trailer percussion ostinato and sub hits (cinematic).
30. **Library scan:** `tools/scan_library.py` is the reference (names only, writes to the app-data folder, never
    uploaded or committed). The desktop app's "Scan my plug-ins" does the same natively; suggestions then show
    "you own it" with the plug-in and, when a preset name matches the recipe, the preset. Never commit a user's scan.

### Needs Jamie's decision first: don't start without asking
24. **Built-in assistant.** A chat panel that sees the whole song and answers "why does the bridge feel weak?"
    or "three alternate chorus melodies in my range". Needs an AI model through an API (internet, running cost).

## Style library and tags (agreed with Jamie, 5 Nov 2026)
`data/style-library.json` holds new style profiles and a tag system, researched with sources. Build it in this order:
1. **Tags replace the style picker.** Kinds: genre, artist, soundtrack & media, composer (base tags), producer
   (a production layer), mood, era, instrument (modifiers), and "your reference" (a song or album the user owns).
   The existing 8 styles and the orchestral templates become tags too (`existingTags`). Search box plus chips, like
   Suno/Udio style tags; the design canvas has the target screen ("5 · Style tags").
2. **Blend rules** (`blendRules` in the JSON): the first base tag sets the core; more base tags blend in at 0.35;
   a producer replaces only the production layer (kit, groove, instruments, sound, articulation, mix moves);
   modifiers apply deltas; conflicts are asked, never silently resolved; a card lists what each tag changed, with Undo.
3. **New profiles:** artists Chappell Roan, Lady Gaga, Fleetwood Mac, Red Hot Chili Peppers, Charli XCX; composer Yann Tiersen
   (Compose, 3/4 waltz); soundtrack Stranger Things; genre Jazz (swing, with ballad / bossa / waltz variants);
   producers Jack Antonoff, Max Martin, Finneas, Mike Dean, RedOne, Rick Rubin (adds a 'Reduce' suggestion that strips each
   section back), Dan Nigro (adds a 'Build it up' suggestion: piano → band → double-time last chorus), A. G. Cook (hyperpop); orchestral composer Ramin Djawadi
   (a new OT template). Each `profiles[]` entry carries every table a style needs (STYLES, STYLE_BASS,
   STYLE_GROOVE, STYLE_KIT, STYLE_INSTR, STYLE_QUIRKS, STYLE_FEEL, SUGSTYLE, TRANS_STYLE, ARTSTYLE, STYLE_DNA);
   write a STYLE_MIX entry for each from its `mixHint` and producer `mixMoves`. Load the JSON at build time
   (tools/wrap_web.py can inline it) or paste it into the HTML; keep one source of truth.
4. **Engine additions they need:** a swing groove (ride pattern, hi-hat on 2 and 4), an upright-bass and an
   accordion instrument, AABA and 12-bar blues forms, a stutter-edit effect (post-chorus hooks), a slap-bass
   articulation, a small found-sound percussion set (Finneas), a synth-solo outro option (Mike Dean). Until one
   exists, use the stand-in named in the JSON and say so in the style's info.
5. **Soundtrack library** (`soundtrackLibrary`): media tags map to a profile or a blend of tags; blends are
   labelled "approximate". **Your reference** (`referenceTag`): the song analyser reads the user's own files
   locally and keeps only numbers, never audio, melodies, lyrics or MIDI.
6. **Chord variety from many songs.** Each profile's chord habits come from several songs
   (`data/progressions.json`, 4–9 songs each, sourced) built by `tools/build_style_matrices.py` into `mMajor`,
   `mMinor` and `modeShare`. When a new song is made in a profile, pick major or minor by `modeShare` (or the
   user's choice), set the mode (the style's own mode if it is in that family, else ionian / aeolian) and use
   that family's matrix. `bpmRange` gives the tempo range to draw from. To add songs: edit progressions.json,
   re-run the script, never hand-edit `m`.
   The **original 8 styles** get the same treatment: `existingStyles` in style-library.json holds their rebuilt
   `m` / `mMajor` / `mMinor` / `modeShare` from 8–20 songs each (both lane artists); apply them to STYLES by id.
7. **Artists by album, songs by tag** (`data/catalog.json`, agreed with Jamie 5 Nov 2026). Every artist and composer is
   listed album by album; each song carries descriptive tags from `tagVocab` (mood, tempo feel, instruments, production).
   An artist whose albums sound very different gets **one profile per era** (`artistEras` in style-library.json; first one:
   Lana Del Rey, 5 eras, 64 songs with chords). In the tag picker, the artist chip opens its era chips (design canvas,
   "5 · Style tags"). No era picked → the artist's default profile; one era → only that era's chords, tempo, sounds and mix;
   two or more → normal blend rules. Never mix eras silently. A song-tag filter ("only songs tagged piano") rebuilds the
   matrix from the matching songs of the chosen era(s) at run time (same counting as build_style_matrices.py), only if at
   least 4 match; otherwise say so and keep the full era. Covers and interludes are listed, never analysed.
   `chords: false` songs still count for tags and the album sound. `tools/build_style_matrices.py` checks the catalog
   (unknown tags, songs marked as having chords but missing) and fails if anything is off.
   New instruments the eras ask for: reverb/tremolo electric guitar, pedal steel, tenor saxophone, brass section, mellotron;
   and a per-section tempo change (Ultraviolence-style chorus that slows down). Use the stand-ins named in each profile until then.
- Rules: profiles describe habits across many songs, never one song; no copied melodies, lyrics or recordings;
  no voice imitation. Points marked `"c":"low"` are starting guesses: tune them with the song analyser and say so.
