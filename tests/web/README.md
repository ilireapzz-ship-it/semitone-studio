# Interface tests (Playwright)

Each file loads `web/semitone-studio.html` in headless Chromium and prints what it checked. Most of them pretend
to be the plugin with a fake `window.__JUCE__` bridge (`__nlog` records events the page sends, `__fire` sends one to the page).
A run is good when the last line is `[]` (no page errors) and the printed values look right.

```
npm install playwright        # once, anywhere above this folder
npx playwright install chromium
node tests/web/regression.test.js
```

| File | Checks |
|---|---|
| `regression.test.js` | every style renders, orchestral render has no NaN, phone layout doesn't overflow |
| `native-bridge.test.js` | plugin bar, timeline sent to native, tempo match, state save/restore, standalone transport |
| `header.test.js` | one header and transport: recipe sentence, undoable recipe, lenses, time display, menu, phone width |
| `timeline-session.test.js` | one timeline, lenses, drawer tabs, pins, pass drawn on the song, old screens in the drawer, rail per lens, Describe a change |
| `parts.test.js` | Add a part menu, Write it for me, linked layers, unlink, one undo step, MIDI and engine, saved songs, pass grammar, song doctor checks, channel limit |
| `borrowed.test.js` | Capture, scale lock, ghosts, mix snapshots, moving and copying sections, ⌘K saved actions |
| `loops.test.js` | loops hidden until used, tempo and key from name or audio, stretch and key fit, plays, saved, folds away, Bridge status and loop points |
| `screens.test.js` | Start, Words (syllables, stress, rhyme letters, rhymes), Versions (saved, compare, copy), Ideas shelf |
| `pitch.test.js` | range check, nearest key, pack files, real PDFs, loudness match, credits, hearing another key without changing the song |
| `length.test.js` | length suggests a shape, lands near its range at many tempos, hook timing, lyric target, pass moves, radio edit |
| `coach.test.js` | five-layer weighing, Review, voices, skeleton, keep the demo, the dial, games, degree, mastery, golden songs |
| `space.test.js` | 1440×900, 1280×800, 1024×680, 900×600: no page scroll, nothing outside the window, roll height; splitter, ⤢, ⌘J per lens, auto-rail (`SPACE_SHOTS=folder` saves pictures) |
| `ruler-sound.test.js` | the ruler at four sizes, click moves the marker, Play starts there, a click while playing jumps, Stop comes back, double-click plays; the picker on every track, search, one undo, kits; plug-ins on Drums and added parts; FL Studio note |
| `themes.test.js` | every token from themes.json, five themes with no reload, saved locally, follow the system, contrast, plug-in shares the setting (`THEME_SHOTS=folder` saves a picture per theme) |
| `song-files.test.js` | .semitone save / open / recents / title |
| `sounds-in-daw.test.js` | section layout, rendering and chunked clips sent to the plugin (slow: renders the whole song) |
| `section-drag.test.js` | Compose section blocks drag out as audio, ⌥ as MIDI, Whole song chip, click still selects |
| `orchestral-drag.test.js` | the same for Orchestral, plus its sections render |
| `style-tags.test.js` | library loads, every new style renders, search, blending, conflicts, eras, variants, orchestral tags, undo, save |
| `producer-pass.test.js` | all 8 producers, A/B returns your version exactly, Discard, Keep as labelled undo steps, muted sections leave the MIDI |
| `emotion-vibe.test.js` | emotion per section, suggested moves, lanes change the sound, one undo step, saved with the song |
| `real-sounds.test.js` | suggestions, recipes, scan results shown only from real names, MIDI + note export |
| `engine-additions.test.js` | swing, shuffle and train grooves, snare roll, silence, accordion, upright bass, Jazz and Country tags use them |
| `demo-song.test.js` | first launch of the app opens the demo, silently; Space plays the whole song; a saved song or the plugin never gets the demo |
| `song-doctor.test.js` | hook, flat, clash, key fit, style position, why it works, usual habits, describe a change, fill a gap, each one undo step |
| `recording.test.js` | desktop takes through the native bridge, takes follow sections, Use this one, Record button, plugin leaves recording to the DAW |
| `tools.test.js` | history list and jump, ⌘K palette, focus mode, section notes, taste ordering |
| `song-versions.test.js` | keep, switch, remove, exact restore, keeps playing for notes and chords, undo, saved with the song |
| `redesign.test.js` | song view, dock tabs, arrangement, side panel, Mix tab, Record and Start screens, phone width |
| `plugin-host.test.js` | find plug-ins, put one on a part, state saves, not offered inside a DAW |
| `undo.test.js` | every kind of change undoes and redoes exactly, labels, no ghost steps, playing song keeps playing, 100-step cap, plugin |
| `fl-markers.test.js` | Send structure to FL: marker list, setup panel |
- `plugin-pass.test.js`: producer moves on plug-ins find controls by name, never swap a plug-in out, and every control returns afterwards.
- `chord-ideas.test.js`: the sounding chord is lit while the song plays and while an idea previews in context.
- `beat-master.test.js`: drum rows (drop, add, delete, undo), master volume, meters, sidechain.
- `track-tools.test.js`: folding tracks into tags, colours, the × buttons, Delete key, right-click menus, editable velocity.
- `one-roll.test.js`: bass and added parts on the same piano roll as melody (move, stretch, add, delete, velocity, undo, linked layers).
- `drum-roll.test.js`: the step grid and drum piano roll edit one pattern (add, drag, velocity, delete, undo, both directions).
- `toolbar.test.js`: snap and free move, Select, Draw, Erase, Slice, Quantize, the timeline grid and the menu bar.
- `roll-feel.test.js`: hearing notes on click, entry and drag, moving a selection as one, the piano keys, plug-ins opening. Also Shift and drag to duplicate, one undo for it, Shift-click still selecting, and an empty click moving the play marker (a note click not).
- `startup.test.js`: big songs reach the page in base64, unchanged, so opening never locks up.
- `daw-tools.test.js`: Redo, Articulation mode, scrolling and zooming the roll and timeline, splitting, duplicate and empty tracks, track and mixer order.
- `tool-pool.test.js`: the piano roll tool bar starts Select, Slice, Articulation mode; drag to reorder; the Tool Pool window (add, remove, drag in and out, move, undo, search, Starter/Default/Everything), right-click and ⚙ entry points, fits at 900×600, order kept after reopening.
- `clip-copy.test.js`: right-click Copy on a timeline clip, then Paste and replace, Paste on top and Paste here (empty spot on an added track) for melody, bass and drums; other sections untouched, one undo, saving and opening, ⌘C and ⌘V on clips.
- `mix-beat.test.js`: Piano roll / Beat / Automation tabs stay visible; beat grid drum names, hint line, About box; mixer faders line up and fit; Room row gone and the old-room notice turns it off with undo; + plug-in on every strip (faked app): menu, load on the right channel, engine takes playback, open, save, remove and undo.
- `audio-tracks.test.js`: Add a part → Vocals makes an armed, centred, mono, high-passed track on channel 16 in one undo; guitar, double and backing-vocal placement; placement by name; a take crossing sections is cut per section, shares one decoded file and plays on its track's channel; punch-in keeps the old take before it; Record with a track armed records the whole song from the marker and draws the take live (faked app); mix check finds stereo/off-centre bass and mud, fixes in one undo; Mono check reaches the engine and isn't saved; tracks, takes and comps save; delete and undo; older songs' takes show as Vocals.
- `drum-knobs.test.js`: a volume knob in front of every drum row, added samples too; drag down quieter in one undo step, drag up to +6 dB, double-click back to 0 dB, all the way down is off, arrow keys; levels change drumPerf (browser, app engine, MIDI, export) and are saved.
- `mix-tidy.test.js`: Smart pan / Mix check / Mono check in the editor's top bar on Mix only; no Inserts heading; compact strip headers; the Drums strip's Sidechain popup ticks parts, changes the trigger and depth for all of them, one undo each, fed to the app engine; dragging a strip selects no text, lifts it and marks where it lands.
