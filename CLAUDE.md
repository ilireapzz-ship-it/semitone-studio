# Semitone Studio — notes for Claude Code

Jamie's music-theory and songwriting app, growing into a standalone DAW (plan: "Semitone Studio: DAW Plan"
Claude Doc). Desktop app + VST3/AU plugin built with JUCE 8.0.4. Mac (FL Studio 2026 is the main host) and Windows.
Jamie is a songwriter, not a programmer: explain results in plain words, not code.

## Start of every session
- `git pull` first: a cloud Claude session also pushes to `main`.
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
| Mac drag of MIDI files | `Source/MacDrag.mm` |
| Tests | `tests/sequencer_test.cpp` (sequencer + clip player): `clang++ -std=c++17 -ISource tests/sequencer_test.cpp -o t && ./t`; `tests/synth_test.cpp` (sound engine), same way with `-O2`. `build-mac.sh` and CI run both |

## Bridge events (web ⇄ native, JSON)
Web → native: `st_ready`, `st_timeline` (notes `[start,dur,ch,key,vel,…]`, `ev` pitch bends, `length`, `offset`,
`sound` = every part's patch settings from `nativeSoundInfo()`, saved in the DAW project so the engine works with the window closed),
`st_state`, `st_transport` (standalone), `st_save`, `st_drag`, `st_song_save`, `st_song_open`, `st_title`,
`st_soundmode` (0 sounds+MIDI, 1 sounds only, 2 MIDI only, 3 built-in engine only, for comparing), `st_audio_layout` (`[{id,at}]` per section),
`st_audio_clip` (chunked int16 PCM + tempo marks).
Native → web: `st_init`, `st_host` (playing/ppq/bpm ~30×/s), `st_midiin`, `st_saved`, `st_dragdone`,
`st_song_saved`, `st_song_opened`, `st_audio_need`, `st_audio_got`.
In the HTML, the bridge is the `NATIVE` / `nativeInit()` / `NSND` code (only active inside the app/plugin).

## Editing the HTML
- It is one big file: patch with exact-string edits; check the last `<script>` with `node --check`.
- New JS modules go before `/* ---------- routing + boot ---------- */`; CSS before `</style>`.
- The page must keep working as a plain web page (no `window.__JUCE__`).

## The native engine (Source/Synth.h)
- It copies the web recipes on purpose: `voice`, `drumHit`, `fmEP`, `anaSynth`, `anaChorus`, `renderPianoNote`, `renderPluck`, `newMaster`, `buildPolish`. Change a patch in the page's tables and the engine follows, because the page sends the settings. Change a recipe and port it to `Synth.h` too.
- Web Audio details it must match: lowpass/highpass Q is in dB; saw starts at 0 and rises; saw, square and pulse are scaled by 1/1.179; compressors add make-up gain; a mono bus is 3 dB down in each ear through a centred panner.
- Check sound changes against the web version, not by ear alone: render the same notes in both and compare level, brightness and level over time, with the mixer effects off (the engine has none yet).
- The room is a feedback network: a loop gain above 1 at any frequency makes it howl by itself and the limiter hides it. `tests/synth_test.cpp` guards this.

## Things learned the hard way (FL Studio on Mac)
- Drag-and-drop MIDI: FL accepts it on the **Channel Rack** (Import MIDI dialog), not the Playlist.
- MIDI routing: Semitone gear → MIDI → Output port N; the instrument's Input port N. Use SONG mode (PAT loops the pattern).
- Omnisphere STACK MODE ignores part channels. Single-part MIDI exports use channel 1. Pitch-bend range is 12.
- Clicking inside the plugin's web view by computer use needs full-screen control.
- Never delete Jamie's files: move them to a `_to_delete/` folder instead and tell him.

## Roadmap (Phase 1 of the DAW plan)
1. Songs as `.semitone` files + autosave + title — done.
2. Semitone's own sounds inside the DAW (ClipPlayer) — done for Compose and Orchestral (sections rendered by the page).
3. Native synth engine so the plugin renders sound without the web page — first version done: all patches, instruments and kits, room, polish. Plays what no rendered section covers.
   Still only in rendered sections: per-part effects, mix automation, transitions, recorded takes, samples and SoundFonts. The engine has no orchestral instruments yet (Orchestral plays from rendered sections).
- Plugin drag-out: song-structure blocks (Compose and Orchestral) drag onto the DAW timeline as WAV cut from the section clips (`st_drag_audio`, `SemitoneEditor::dragAudio`); ⌥ drags that section as MIDI (`MIDI_SEC` / `OMIDI_SEC`).
