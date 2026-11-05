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
