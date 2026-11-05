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
| `undo.test.js` | every kind of change undoes and redoes exactly, labels, no ghost steps, playing song keeps playing, 100-step cap, plugin |
| `fl-markers.test.js` | Send structure to FL: marker list, setup panel |
