# Semitone Studio — desktop app and DAW plugin

Milestone 1: the full Semitone interface running as a **VST3 / AU plugin** and a **standalone app**.

- Follows the host's tempo and play button, looping the whole song or the selected section.
- Sends each part (or orchestral instrument) on its own MIDI channel, so any instrument in your DAW
  (Omnisphere, Kontakt, Serum…) can play it.
- Saves the whole Semitone song inside the DAW project.
- Drag parts as MIDI files straight onto the DAW timeline.
- Every export from the web version works; files are saved with a normal Save dialog.

## How it's built

| Piece | Where |
|---|---|
| Interface | `web/semitone-studio.html` — the same single-file app as the web version, compiled into the plugin |
| Bridge (web side) | inside the HTML: `NATIVE` / `nativeInit()` (only active when running in the plugin) |
| Plugin / app | `Source/PluginProcessor.*`, `Source/PluginEditor.*` (JUCE 8, WebView2 on Windows, WKWebView on Mac) |
| Sequencer | `Source/Sequencer.h` — host-synced, looped, JUCE-free; tested in `tests/` |

Events between the interface and the plugin: `st_ready`, `st_init`, `st_timeline` (the song as notes),
`st_state` (project saved in the DAW), `st_host` (tempo/position, 30×/s), `st_transport` (standalone play),
`st_save`, `st_saved`, `st_drag`.

## Builds

Every push builds Windows and macOS versions with GitHub Actions (`.github/workflows/build.yml`).
Download them from the run's **Artifacts**. Local build:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Windows also needs the WebView2 SDK (`nuget install Microsoft.Web.WebView2 -Version 1.0.1901.177`
and `-DJUCE_WEBVIEW2_PACKAGE_LOCATION=<folder>`).

## Updating the interface

Design happens in the web version. To ship it here, replace `web/semitone-studio.html` with the latest
web file and push; the build wraps and embeds it.

## Next milestones

2. Native sound engine (Semitone's own instruments as plugin audio, not just MIDI).
3. Host VST3/AU instruments inside Semitone, one per piano roll.
4. iPad (AUv3).
