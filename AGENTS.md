# Semitone Studio — notes for coding agents (Codex and others)

**Read `CLAUDE.md` first.** It is the project guide shared by every agent: layout, the web ⇄ native bridge,
how to build and test, the sound engine rules, and FL Studio lessons learned the hard way. Everything there applies to you.

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
1. **Undo for every action.** Today undo covers note edits in the piano rolls only. Add a song-level history:
   every change to style, structure, sections, chords, mix, automation, sounds, lyrics, orchestral parts etc.
   can be undone and redone (⌘Z / ⇧⌘Z, plus buttons), with a short label per step ("Changed style to …").
   A good approach: snapshot `songStateJSON()` after each settled change (debounced), restore with the same
   path `nativeRestore` uses; keep the existing piano-roll undo for fine note edits. Cap the history (~100).
   Works in the web version, the app and the plugin.
2. **Open with a song ready.** First launch of the standalone app shows a finished demo song, ready to play
   (Space plays). Later launches reopen the last song (autosave already does this). Don't start sound by itself.
3. **Windows check.** The Windows build has never been run on a PC. Go through docs/INSTALL-WINDOWS.txt
   when a PC is available; fix what breaks. (FL markers need loopMIDI on Windows; nothing else should be Mac-only.)
4. **Engine gaps** (Source/Synth.h), so the plugin sounds complete with its window closed: per-part effects,
   mix automation, transitions, recorded takes, samples and SoundFonts, orchestral instruments.

### Phase 2: record the song
- Audio tracks for vocals and guitar, placed by section (recordings belong to a section, so they follow the structure).
- One Record button: it knows whether you're humming (→ melody), singing, playing keys or recording an instrument.
- Takes, comping and punch-in for each section; latency compensation.
- In the standalone app, record from the audio input natively (JUCE), not through the web view; the page shows
  waveforms and controls. Inside a DAW, leave recording to the DAW.

### Later phases (see the plan doc)
3 your VST3/AU plugins inside Semitone · 4 mix and master · 5 share everywhere (export to every DAW, iPad, share links).
