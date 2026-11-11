# Semitone Studio — notes for Claude Code

Jamie's music-theory and songwriting app, growing into a standalone DAW (plan: "Semitone Studio: DAW Plan"
Claude Doc). Desktop app + VST3/AU plugin built with JUCE 8.0.4. Mac (FL Studio 2026 is the main host) and Windows.
Jamie is a songwriter, not a programmer: explain results in plain words, not code.

## Start of every session
- `git pull` first: a cloud Claude session and Codex (see AGENTS.md, which also holds the task list) push to `main` too.
- Build and install on this Mac: `tools/build-mac.sh` (`--run` opens the app, `--no-install` builds only).
  Restart FL Studio to load a new plugin build. Every push to `main` also builds Mac + Windows in GitHub Actions.

## Layout
| What | Where |
|---|---|
| The whole interface | `web/semitone-studio.html` — one self-contained HTML/JS file (also the web version) |
| HTML → plugin | `tools/wrap_web.py` (inlines VexFlow at build time), compiled in as BinaryData |
| Plugin / app | `Source/PluginProcessor.*` (audio thread, state), `Source/PluginEditor.*` (web view + bridge) |
| Host-synced MIDI | `Source/Sequencer.h` (JUCE-free) |
| Semitone's own sounds in the DAW | `Source/ClipPlayer.h` (JUCE-free): sections rendered by the page, played locked to PPQ |
| Native sound engine | `Source/Synth.h` (JUCE-free): live voices driven by the sequencer's events; plays any beat no rendered section covers, with no web page |
| Song files, autosave, recents | `Source/SongFiles.h` |
| Scan my plug-ins (names only, kept locally) | `Source/LibraryScan.h` |
| Recording takes (desktop app, audio input to WAV, tested) | `Source/TakeRecorder.h`; files in Documents/Semitone Studio/Songs/Takes |
| Your plug-ins on parts (desktop app) | `Source/PluginHost.h`; events `st_host_*` |
| Mac drag of MIDI files | `Source/MacDrag.mm` |
| Interface tests | `tests/web/*.test.js` (Playwright, see tests/web/README.md) |
| Tests | `tests/sequencer_test.cpp` (sequencer + clip player): `clang++ -std=c++17 -ISource tests/sequencer_test.cpp -o t && ./t`; `tests/synth_test.cpp` (sound engine), same way with `-O2`. `build-mac.sh` and CI run both |

## Bridge events (web ⇄ native, JSON)
Web → native: `st_ready`, `st_timeline` (notes `[start,dur,ch,key,vel,…]`, `ev` pitch bends, `length`, `offset`,
`sound` = every part's patch settings from `nativeSoundInfo()`, saved in the DAW project so the engine works with the window closed),
`st_state`, `st_scan`, `st_scan_load`, `st_take_start`, `st_take_stop`, `st_transport` (standalone), `st_save`, `st_drag`, `st_song_save`, `st_song_open`, `st_title`,
`st_soundmode` (0 sounds+MIDI, 1 sounds only, 2 MIDI only, 3 built-in engine only, for comparing), `st_audio_layout` (`[{id,at}]` per section),
`st_audio_clip` (chunked int16 PCM + tempo marks).
Native → web: `st_init`, `st_host` (playing/ppq/bpm ~30×/s), `st_midiin`, `st_saved`, `st_dragdone`,
`st_song_saved`, `st_song_opened`, `st_audio_need`, `st_audio_got`, `st_scan_done`, `st_take_started`, `st_take_done`.
In the HTML, the bridge is the `NATIVE` / `nativeInit()` / `NSND` code (only active inside the app/plugin).

## Editing the HTML
- It is one big file: patch with exact-string edits; check the last `<script>` with `node --check`.
- New JS modules go before `/* ---------- routing + boot ---------- */`; CSS before `</style>`.
- The page must keep working as a plain web page (no `window.__JUCE__`).
- Layout: `initTidy` moves the Compose tools into the tabbed dock (`ss_dock`), then `initShell` builds the song view: `#arr` arrangement, dock, `#songSide`. New screens are overlays: `#startView`, `#recView`. The HTML order is not the page order.
- Plug-in automation: lanes use the target `plug.<parameter index>` on a part (`hostPlugLanes`), values 0–1, sent as `plugLanes`. Indexes are per plug-in version. Desktop app only.
- A pass being auditioned is a preview: `savedProject` keeps autosave and song state on your version (`PASS.A`) until Keep. Some moves compound if run twice (`thin`, reverb, bass level).
- Learn a control: `st_host_learn` makes `PartHost` watch every parameter; `learnPoll` ignores ones that drift by themselves, then reports the biggest mover (`st_host_learned`). Learned controls save per part in `S.hostLearn` with the plug-in id, join `plugMap`, and reach `paramsVar` through `also`. Omnisphere and Reaktor ignore values on unassigned slots.
- Link by MIDI: for plug-ins that hide their controls, a control index from 100000 (`HOST_CC`, `ccBase`) is a MIDI controller. Lanes run through the same path as plug-in controls, and `st_host_cc` sweeps one for the plug-in's MIDI Learn. Semitone can't read these back, so the song stores a resting value.
- Plug-in sounds: `hostSoundsHTML` lists saved sounds (Application Support/Semitone Studio/Plug-in sounds, tagged with the song's style), and presets the plug-in names usefully. `hostStylePlan` reads style words from tag text and sets mapped controls. Undo can't reach inside a plug-in, so changes offer Put it back (`hostKeepBack`).
- Your effects: `PartHost` holds up to four per plug-in part and on the whole song (target 16), saved in `S.hostFx` with their state. `runFx` applies them; `processMaster` runs last. A whole-song effect sets `engineOwns`. Built-in parts and the Transitions strip take them through `processChannelFx` (targets 0–15; Transitions use `transCh()`); buses can't yet. `PartHost::process` delays everything to match the slowest plug-in chain (`LineUp`), and takes allow for it (`HOSTS.latency`).
- Plug-ins in the mix: with `engineAll`, `PartHost::process` runs first and `Synth::setExternal` takes each plug-in into its part's strip (`hostMix`: fader, pan, lanes, effects, room, master, polish). The engine's own notes wait for the slowest plug-in. Export audio then uses `SemitoneProcessor::bounce`, the same mix faster than real time, into Documents/Semitone Studio/Exports.
- One clock in the app: with a plug-in on a part (`engineOwns`), the timeline carries `engineAll`, the native engine plays every part without a plug-in, and the page stays silent and only draws the playhead. The page and native audio can't share a clock otherwise. Samples and SoundFonts live in the page, so `nSampleParts` renders them to clips (`RENDER_ONLY='samples'`) for `ClipPlayer`.
- Plug-in maps: `data/plugin-maps.json` lists, per plug-in, the controls that matter with plain labels, the switch each needs, and safe limits, all measured. `plugFind` tries the map first, then names. A move may turn a mapped switch on for its own sections (`on`, `idle`, `set`).
- Producer moves on plug-ins: an op `plug:parts:intent shape@where` in `producers[].pass.moves` (data/style-library.json). `passPlug` finds the control by name (`PLUG_INTENT`, `plugFind`), skips one that is switched off in the patch (`plugIsOff`), and writes normal plug-in lanes. A pass never replaces a plug-in (`passSwaps`). `hostPlugLanes` keeps a flat lane for any control a lane has moved, so it returns to rest when the lane goes; on stop `PartHost::process` returns lanes to their first point. Nothing follows plug-in output, so keep drive moves small.
- Themes: `tools/embed_data.py` turns `docs/design/merged/themes.json` into `--st-*` variables (`#stThemes`), and `data/fonts/` into `#stFonts`, so edit those, not the blocks. The older `--paper`, `--ink`, `--accent` and so on point at the tokens. In markup use `var(--st-melody)` (`partVar`); on a canvas read the value (`partColor`, `css()`). Button shapes per theme are the `--st-btn-*` and `--st-prim-*` rules in the look block. The setting lives in localStorage, and in `settings.json` in the app data folder (`st_pref`, `prefs` in `st_init`) so the app and the plug-in share it.
- One screen: `arrRender` draws the whole timeline, `HEAD.lens` is Write, Produce or Mix. `drawerInit` groups the old dock panes under Notes, Words, Sound, Mix, Ideas (`DRAWER`); `shellDock(id)` still opens any pane. Session (`sessGather`, `sessRender`) is the one feed; pins share its numbers. A pass draws through `PASS.marks` and `tlSpot`.
- Added parts: the four originals stay in `S.parts`; `S.extra` holds the rest with a role from `data/part-roles.json` and its own channel (`xFreeCh`: at most 11, fewer with section layers). `songParts()` is the list to loop over, not a fixed array. Notes are keyed by `secKey`, `x.sections` by index, so `secMove` rewrites both. Generators are `xGenSection`.
- Loops: `S.loops` holds settings, the audio stays outside the song (`st_loop_put`/`st_loop_get` in the app, IndexedDB on the web). `loopRender` fits them in the page and `nSampleParts` sends them to the engine as a clip. Splice Bridge is `PartHost::audition`, straight to the output and never exported. VST3 has no way to send a key, so the page only tells you to set it. Hosted plug-ins now get the song's tempo and position through `SongHead`.
- Words uses `data/rhymes.json` (`tools/build_rhymes.py`, CMUdict common words): `dictWord`, `rhymesFor`. Versions are `S.vers`, in the song but outside undo (`histCapture` drops them). Compare, Skeleton and Hear it all borrow the pass trick: `PASS.A` holds your song so nothing saves the preview, and `verDone` puts it back.
- Pitch: `pitchFit` reads only your notes; `range` in `artistEras` must have a source and is a guide. `pdfMake` writes plain PDFs with base fonts. `st_pack_file` writes the folder in Exports. Length: `lenList` builds a structure from song-shapes, and only `lenApply` changes the song.
- Coach: `coachStrength` is the five layers, `coachMeasure` is one detector per rule (null means not measured, never a guess), `coachItems` scores the jury. New suggestions should carry a rule id so they're weighed.
- Space: `spGet`/`spPut` keep each lens's split (`ss_space`), `spApply` sets `sp-big` (editor gets the room, parts 26px), `sp-fold` (editor 34px), `sp-sheet` (under 800px tall), and `spFit` shares the height between parts. The app status bar now lives behind the logo. Help cards go to Session through `spHelp`. Page sizes are tested in `space.test.js`.
- `data/*.json` are copied into the HTML by `tools/embed_data.py` (run it after any data change; the build fails if you forget). Style tags are `TL` / `tagResolve`: a picked mix becomes one generated style saved as `tags` + `tagChoices` in the song.
- Drums: grooves have a feel (`GCLS`) and each style a set (`GPAL`, `gPal`, `groovePick`); fills are `FILLS` chosen by `fillPlan` (sized by the energy change, cached per song version in `FILLC`); toms (`TOMS`, GM 50/47/43) are fill-only, not grid rows, and are ported to `Synth.h startDrum`. Transitions are picked by `transCore`/`transOf` from the energy change and the style's `TRANS_STYLE` set; `kickOut` acts in `drumPerf`.
- Your own bars: `SC_BARS` (tools, transport, head, mix) each get a `.sc-slot` of shortcuts from `SCB` (localStorage `ss_bars`, `st_pref` key `bars`). A shortcut is `rt:<rail tool>` or `el:<selector>` that clicks the real control (`scRun`); `initBars` wraps `railTarget`/`railDrop` so rail and Tool Pool drags land on bars. Customise this bar saves `SCB.order[bar]` by block key (`data-ck`). Give a new ⋯ control an id so it can become a shortcut.
- Smooth playback in the app: the page resends `st_timeline` for any change, so the engine must not reset on a resend. `Sequencer::sameNotes` keeps held notes (a real edit re-sounds them by chase); `PluginProcessor` reuses the running `FxBank` and sends `fxTune` so effects `retune` in place (`fxRetunable` decides); `Polish::set` ignores identical settings. Don't put playhead-dependent values in `nativeSoundInfo()` (`pcNative` builds with `PCX.si=null`).
- Mixer chain: `st.compAt` = insert slots before the compressor (page `getBus` splits `fxChain` there; engine `FxBank::compAt`, desc `at`). `chainApply` reorders and renames lanes `fx<i>.*`. A strip compressor is off until its amount is above 0 (`pcOf`). New EQs get `EQ_FLAT` bands (`who:'Start'`, ignored by suggestions until moved).
- Song position: `S.songAt` (`songPos()`, `songPlay()`), separate from the selected section. The ruler (`rulSeek`), the strip above the roll (`setPlayFrom`) and ⏮ move it; Space and ▶ play from it and Stop returns there. ⇧Space plays the selected section.
- Recording across sections: unless Loop is on, a take plays the song on from the selected section (`LIVE.recAcross`); each note keeps its section (`si`), the commit writes each section in turn under `H.hold` and makes one undo step, and `rollRecLane` marks the timeline (`rec-got`, `rec-now`). With the step grid open, Record records drums there (`beatPaneOpen`).
- Recording notes: `recPress` with the piano roll open calls `rollRecStart` (records into `rollRecPart()`, live red notes on `#rollRecCv`, pill `#rollRec`); elsewhere it opens the dock Record tab (`shellDock('record')`), the agreed design for audio takes and comping. The old `#recView` page is only reached by `recShow()`.
- Playing parts in: Record › Keys (`REC.mode==='keys'`) records into `keysInto()` (melody keeps `commitMelodyRec`; chords `writeChordPads`; drums the section type's `S.beats`; bass `S.bassOwn`, added parts `x.notes`) via `keysCommit`. `liveOn` plays the target's sound (`keysSound`); the key handlers run in capture and stop the tool shortcuts while `keysOn()`. A take replaces only the stretch played. `keyAdvice`/`keyRelabel` offer the key the notes are in.
- Transitions in the timeline: `trLaneRow`/`trLaneLayout` draw a block per join (`transOf(i)`, build-up moves `TR_PRE`, landing `TR_POST`). Lengths are `trLen(i)` = `S.transLen[i]` or `trLenAuto` (bars before/after; size from `transSizeOf`); `scheduleTransitions`, `nativeTrans` and `drumPerf`'s Kick out all read them. Section indices key `S.trans`, `S.transOwn` and `S.transLen`.
- Undo is one song history (`H`, `histCheck`): it compares `projectData()` + `orchData()` after each settled change, so anything those save is undoable automatically. `pushUndo` / `oPushUndo` only mark a fine note edit as its own step. Give a new setting a label in `histLabel`.

## The native engine (Source/Synth.h)
- It copies the web recipes on purpose: `voice`, `drumHit`, `fmEP`, `anaSynth`, `anaChorus`, `renderPianoNote`, `renderPluck`, `newMaster`, `buildPolish`. Change a patch in the page's tables and the engine follows, because the page sends the settings. Change a recipe and port it to `Synth.h` too.
- Web Audio details it must match: lowpass/highpass Q is in dB; saw starts at 0 and rises; saw, square and pulse are scaled by 1/1.179; compressors add make-up gain; a mono bus is 3 dB down in each ear through a centred panner.
- Check sound changes against the web version, not by ear alone: render the same notes in both and compare level, brightness and level over time, with the mixer effects off (the engine has none yet).
- The room is a feedback network: a loop gain above 1 at any frequency makes it howl by itself and the limiter hides it. `tests/synth_test.cpp` guards this.

## Installing on Jamie's Windows PC
- No compiler on that PC: push, let GitHub Actions build, download the "Semitone-Studio-Windows" artifact (signed in, through the browser pane) into a new folder.
- Copy `Semitone Studio.exe` over `%LOCALAPPDATA%\Programs\Semitone Studio\` (the desktop and Start menu shortcuts point there, so he always opens the newest) and the `.vst3` folder into `C:\Program Files\Common Files\VST3\` (admin prompt). He must reopen the app and restart FL Studio to get it.
- Never call `setTitleBarButtonsRequired`, `setResizable` or other title-bar/style changes on the standalone window after the page loads: JUCE rebuilds the native window and the WebView2 page is lost (a white window). The editor maximises it once with `ShowWindow(SW_MAXIMIZE)` from the timer instead.
- Look inside the real app: start it with `$env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS="--remote-debugging-port=9333"` (PowerShell), then Playwright `chromium.connectOverCDP('http://127.0.0.1:9333')` gives the live page (state, DOM, errors). It found the double click listeners and the empty pitch pack. Never press Save on Jamie's songs while testing.
- The page sends standard base64; JUCE's `MemoryBlock::fromBase64Encoding` only reads JUCE's own format. Decode with `juce::Base64::convertFromBase64` (`SemitoneEditor::decode`).
- Computer use's "open by name" can launch an old copy; start the app from `%LOCALAPPDATA%\Programs\Semitone Studio` and check the running path. Old downloads go in `C:\Users\Public\Downloads\_to_delete`.

## Things learned the hard way (FL Studio on Mac)
- Drag-and-drop MIDI: FL accepts it on the **Channel Rack** (Import MIDI dialog), not the Playlist.
- MIDI routing: Semitone gear → MIDI → Output port N; the instrument's Input port N. Use SONG mode (PAT loops the pattern).
- Omnisphere STACK MODE ignores part channels. Single-part MIDI exports use channel 1. Pitch-bend range is 12.
- Clicking inside the plugin's web view by computer use needs full-screen control.
- Playlist markers: `Source/FLMarkers.h` installs `device_SemitoneStudio.py` in Documents/Image-Line/FL Studio/Settings/Hardware and sends SysEx on the virtual port "Semitone Studio markers". FL only lists a new script after a restart. Tested working in FL 2026 (Mac).
- Rendered sections and takes are audio, so another tempo would change their pitch. When the DAW's tempo isn't the song's, `processBlock` skips them and the built-in engine plays instead.
- Never delete Jamie's files: move them to a `_to_delete/` folder instead and tell him.

## Roadmap (Phase 1 of the DAW plan)
1. Songs as `.semitone` files + autosave + title — done.
2. Semitone's own sounds inside the DAW (ClipPlayer) — done for Compose and Orchestral (sections rendered by the page).
3. Native synth engine so the plugin renders sound without the web page — first version done: all patches, instruments and kits, room, polish. Plays what no rendered section covers.
   Volume and pan lanes and section layers now reach the engine. Part effects are in `Source/SynthFx.h`: filter, sat, warm, EQ, echo, chorus, tremolo, and a plate that is a feedback network rather than the page's convolver. Change one on the page and port it. Transitions and desktop takes are in `Source/SynthTrans.h`. All 13 mixer effects are ported (wide's pitch shift is approximate). Orchestral instruments play through `nativeOrchInfo`. Still only in rendered sections: samples and SoundFonts. Your own samples and SoundFonts live in the page, so only rendered sections play them.
- Plugin drag-out: song-structure blocks (Compose and Orchestral) drag onto the DAW timeline as WAV cut from the section clips (`st_drag_audio`, `SemitoneEditor::dragAudio`); ⌥ drags that section as MIDI (`MIDI_SEC` / `OMIDI_SEC`).
