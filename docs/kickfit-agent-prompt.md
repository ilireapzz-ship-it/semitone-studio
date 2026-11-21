You're taking over a new project inside the Semitone Studio repository: **Kick Fit**, a separate VST3/AU plug-in that helps Jamie find the sweet spot for the kick in a mix. You are running on Jamie's Windows PC (computer name DESKTOP-5ABHAEE), so you can read his sound libraries directly.

## Who you're working with
Jamie is a songwriter, not a programmer. Explain results in plain words, not code. Never delete Jamie's files. If something has to go, move it to a `_to_delete/` folder and tell him.

## Before anything else
1. `git pull`. A cloud Claude session and Codex also push to `main`.
2. Read `CLAUDE.md` and `AGENTS.md`. They cover the build, the CI and the house rules. The CI (`.github/workflows/build.yml`) builds Mac + Windows on every push to `main`. Its Windows job shows how to configure with CMake and WebView2.
3. Look at `Source/Synth.h` and `Source/SynthFx.h` for the existing JUCE-free DSP style and the `tests/synth_test.cpp` style of tests. `tools/scan_library.py` already knows some library paths, but it is Mac-flavoured and names-only.

## What Jamie wants (decisions already made)
- **A separate plug-in**, not part of Semitone's main plug-in. Jamie puts it on the kick channel in FL Studio (his main host), on Windows and Mac. Use JUCE 8.0.4 like the rest of the repo. Add it as its own `juce_add_plugin` target, for example in `kickfit/` with its own CMakeLists, pulled in by the root CMakeLists. Add it to the CI build and packaging for both platforms.
- **It works on every kind of kick**: Splice samples (most of his kicks), FL Cloud Sounds, 808s that play notes, acoustic kicks, and Semitone's own kicks.
- **A mode for every main genre.** Proposed list, which Jamie hasn't confirmed yet (ask him once, early): trap/hip-hop, pop, R&B, house/EDM, drum & bass, rock, afrobeats/amapiano, lo-fi. Each mode has its own target ranges, taken from measurements (below), never invented.

## What the plug-in does
1. **Sidechain input** from the bass or the whole beat, so it judges the kick in context. FL Studio supports sidechain into plug-ins. Write Jamie short plain steps for routing it.
2. **One picture**: the kick and the bass on top of each other, in pitch and in time, with a shaded sweet-spot zone for the chosen genre.
3. **Tune to key**: detect the kick's fundamental (its boom note) and offer to shift it to a note that fits the song's key. The key comes from a key setting or from detecting it on the sidechain.
4. **Make room**: duck only the low range of the bass when the kick hits (a frequency-limited duck), not the whole bass.
5. **A few big controls**: Thump, Punch, Click (audible on phone speakers), Tail, Room for bass.
6. **"Sit it" button**: measures the kick's loudness against the sidechain mix and moves it into the genre's target range. Jamie's ears do the final bit.

## Step 1: measure real kicks and songs (do this first; the targets depend on it)
Jamie's folders on this PC: **Music** (iTunes library, his downloaded songs), **FL CLOUD Sounds**, **packs** (probably Splice). Find his Splice download folder as well (Splice app → Preferences → Folder location) and confirm the paths with him if unsure.

- Write the analyser as `tools/kick_analyse.py` (Python is fine; it's a tool, not part of the plug-in).
- **Kick samples**: find kicks by file and folder names (kick, kck, bd, 808 …) and confirm each one by its sound (a strong low-frequency hit near the start). For each, measure: fundamental pitch and whether it glides, attack/click energy and where it sits, decay time, the balance between low and high energy, peak and loudness, and DC/phase oddities. This tells us the range of kicks the plug-in must handle.
- **Songs**: use iTunes metadata (genre tag) to group them. Separate drums and bass with a local stem-separation tool (for example Demucs; install it into a project venv and tell Jamie what you installed). Then measure per song: kick loudness against the full mix, kick fundamental against the song's key, the kick/bass overlap in frequency and in time, low end mono-ness, and how much the bass ducks under the kick. Record genres with too few songs as "not enough data", not a guess.
- **Privacy**: audio, song titles and file paths stay on the PC. Commit only aggregate numbers per genre, for example `data/kickfit-targets.json` with medians, ranges and song counts. Note in the file how each number was measured.
- Show Jamie a plain-words summary per genre, e.g. "in your trap songs the kick sits about X dB under the mix and its boom is usually near F". Ask whether it matches what he hears.

## Step 2: build the plug-in
- Keep the DSP JUCE-free in headers (like `Synth.h`) so it can be tested with plain `clang++`/`cl`.
- Choose low or no latency. If any part needs lookahead, report it to the host.
- Write the GUI in the same plain style Semitone uses, sized for a laptop screen.

## Step 3: prove it's good (no claim of "good" without these)
- **Automatic tests** (`tests/kickfit_test.cpp`, run in CI next to the others), built from synthetic kicks and bass plus a few short committed test signals you generate yourself (never Jamie's samples):
  - Do no harm: a kick already inside the genre target is changed very little.
  - No clipping, no clicks at block edges, no denormals, and stable at 44.1/48/96 kHz and buffer sizes from 32 to 2048.
  - Pitch detection is within a few cents on known test tones and 808 glides.
  - The ducking only touches the low band, and makes no change when the sidechain is silent.
- **Test bench**: a script that runs a large set of Jamie's kicks through each genre mode against matching bass stems. It reports how many moved toward the target, and any that got worse. It runs locally on the PC, and only the summary is committed.
- **Small-speaker check**: simulate phone/earbud speakers (a high-pass around 150–200 Hz) and confirm the kick is still audible through its click and harmonics.
- **Blind A/B for Jamie**: a simple level-matched, randomised listening test on his own songs. He picks A or B without knowing which is processed. Keep score. If he doesn't prefer Kick Fit's version clearly more often, keep working.
- Test the Windows VST3 in FL Studio on this PC, and report what you checked.

## Working style
Small commits, with Mac + Windows CI passing on each. Add a plain-words progress note for Jamie in `docs/kickfit/PROGRESS.md`. Add a short Kick Fit section to `CLAUDE.md` (layout table row and test command) once the target exists. If something can't be measured, say so. Never fill a gap with a guessed number.
