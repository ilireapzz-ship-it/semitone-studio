# Semitone Studio next to FL Studio, Ableton and Pro Tools: gap report (12 Nov 2026)

## How this was done

- **Used it for real.** I opened the installed app on this PC on a separate test data folder (your songs, autosave and settings were never touched), started blank, and wrote a song by hand with the mouse: a melody, chords and a drum beat for a verse and a chorus, then an arp, new sounds, a Rick Rubin-style pass, and a mix (fader, pan, compressor, Smart EQ, a bus). I rendered the result inside the app to measure it, because I can't hear it.
- **Read the code.** The interface file, the sound engine and the plug-in host.
- Each point below is marked **(used)** when I saw it happen, **(measured)** when I have numbers, or **(code)** when it comes from reading and has not been heard or run.

## The short answer

Semitone's smart side is already ahead of the big three: nothing in FL, Ableton or Pro Tools explains a chord, reviews a song, or tells you why a melody is masked. It feels less professional for five reasons, in this order:

1. **Things change without being asked.** A pro DAW never touches your notes unless you do. Semitone did, several times in one session (list below). This costs more trust than any missing feature.
2. **Editing is boxed in.** Chords are one per bar, sections of the same kind share their notes, and many everyday edits (stretch a chord, duplicate a section, ramp velocities) are missing.
3. **Levels are not staged.** The built-in sounds differ by up to 20 dB on the same notes, so changing a sound wrecks the balance, and the mixer's advice was built on a shaky reading.
4. **The engine has unguarded edges.** One effect could run away into a howl, and several protections a pro engine has are absent.
5. **Missing studio furniture.** Audio device settings, a CPU meter, freeze, clip fades, export formats.

## Fixed today (each has a test; nothing pushed)

| What was wrong | How I found it | Now |
|---|---|---|
| **Home → "Back to (song)" rewrote the whole song** (new sections, chords, melody, no message) whenever its length no longer matched the recipe's "about 5 minutes" | It replaced my own test song (used) | Back to only closes Home |
| **EQ picture**: the measured shape ran off the top as a flat block, masking hatching covered the whole height and the labels, the ideal band sat far from the sound | You pointed at it; then (used) | Shape sits inside the graph, hatching only where the two parts overlap, numbers drawn on top |
| **Piano roll jumped after every note you drew**, so the next click landed on the wrong pitch (eight clicks gave C7, C7, C7, C7) | (used) | Rows stay put while you write; Fit the notes still fits |
| **Deleted Bass still showed as ghost notes** in the roll | (used) | Gone |
| **Producer pass card showed code** ("drop:counter@all") | (used) | "Leave out the counter melody for the whole song" |
| **Mixer said "A little quiet: +22 dB" on every strip** | (used) | It reads the part while it sounds, not the last second; over 6 dB says "Quiet", not "a little" |
| **Echo ran away**: above 80% feedback the Digital and Ping-pong echoes grew louder for ever; above 55% the Tape and Analog tails never ended | (measured: +150 dB after 40 s) | Repeats always die away. Settings that already worked sound the same. Also changed in the engine, which needs a build to take effect |
| A **G dorian song was called "G major"** on Home, in Share and in the loop search words; the first bus you add was **"Bus 4"** | (used) | "G minor"; "Bus 1" |

## 1. Workflow and feel

| A pro DAW | Semitone today | What to build | Priority |
|---|---|---|---|
| Never changes your work by itself | Besides the bug above: a bass line starts playing as soon as chords exist; Verse 2 silently copies Verse 1; a kept producer move muted the drums in every section (used) | A rule with a test behind it: every automatic change shows a small card saying what changed, with Undo. "Linked to Verse 1" shown on the section | 1 |
| The editor stays on what you are editing | After playing the song, the editor had moved to another section and stayed there. I wrote into the wrong section without noticing (used) | A Follow on/off button | 1 |
| Blank means blank | Start blank gives 10 sections, 80 bars, six transitions, a style and a producer (used) | A true empty start: one section, nothing else | 2 |
| Scales to the screen | On your 2560×1440 screen the text and knobs are tiny and the Mix screen is half empty (used) | A size setting (100 / 125 / 150%) | 2 |
| Audio settings: output device, buffer size, sample rate | Input device only (code). In the app the sound comes from the web page: 44.1 kHz, about 64 ms behind (measured), which makes played keys feel late | Make the native engine the only sound path in the app, with a device and buffer setting. This is the biggest single step toward "feels like a DAW" | 1 |
| Section menu: duplicate, insert, rename, colour | Play from here, Split, Delete only (used) | Add the missing entries | 2 |
| CPU meter, freeze / bounce in place, track folders, your own templates, named markers, time-signature changes | None (code). Only 4/4, 3/4, 6/8, 12/8 | In that order | 3 |
| Shortcuts you can change | Fixed (code) | Later | 3 |

## 2. Editing

| A pro DAW | Semitone today | What to build | Priority |
|---|---|---|---|
| Chords are just notes: any length, any rhythm | One chord per bar; dragging its edge does nothing; an empty bar is silence; Draw and Erase are greyed out with no hint that double-click adds a note (used) | Let a chord stretch over bars and split inside a bar; show a hint in an empty chord lane | 1 |
| Scale mode can snap notes to the key | "Scale lock" only shades the rows; I placed a C♯ in G dorian with it on (used). The design note says "shade or snap" | Your call: make it Off / Shade / Snap | 2 |
| Paste lands where you clicked | In the chord lane it lands at a play marker you can't move there (used) | Paste at the last click | 2 |
| Undo names what happened | Every note edit is "Edited the notes in Verse" (used) | "Added G4", "Moved 3 notes" | 3 |
| Arp per clip | One arp per part for the whole song (used) | Arp on/off per section | 2 |
| Quantize strength, humanize, velocity ramp, glue, mute a note, chord stamp, mod-wheel / sustain / bend lanes, colour by velocity | Missing (code) | Quantize strength and velocity ramp first | 2 |
| Audio clips: trim, fades, crossfades, clip gain, reverse, pitch correction, punch range | Takes sit in sections; none of these (code) | Trim and fades first | 2 |

Editing that worked well: drawing, erasing and stretching notes, the step grid, deleting tracks and sections, 27 undos in a row, chord naming (it called my chords Gm, C, F correctly).

## 3. Mixing

| A pro DAW | Semitone today | What to build | Priority |
|---|---|---|---|
| Presets are roughly equally loud | Same melody, different sound: Bright pop peaks at −3.7 dB, Studio piano −10.9, Prophet pad −16.2. Changing two sounds made my whole song 7 dB quieter (measured) | Level-match the sounds, or offer "Match the level" when you swap one | 1 |
| A mute is visible everywhere | The pass's section mutes dim the timeline but the mixer strip shows no mute light (used) | Show it on the strip | 2 |
| Counts are true | "Keep 10 changes" kept 4 (used) | Make the button count what it will keep | 3 |
| A starting mix has width | Everything starts dead centre: my song was effectively mono (measured) | Offer Smart pan the first time Mix opens | 2 |
| Export: 24-bit, MP3, a chosen range, a loudness target | The page exports 16-bit WAV inside a ZIP, no dither, no choice (code). The engine's export is 24-bit | One export dialog: format, range, "streaming loud" | 1 |
| Gate, de-esser, multiband, transient shaper, sidechain from any track, more than 3 inserts, phase meter, true-peak number | Missing or partial (code) | Sidechain from any part and a gate first | 3 |
| Lists stay true | "Your DAW" still listed the deleted Bass; section loudness chips showed stale numbers (used) | Small fixes | 3 |

The Smart EQ, the "I hear…" chips and the checks beside it are better than what the big three ship. The ideal band is still one generic slope for every instrument; it should follow the instrument's real range.

## 4. Engine and sound quality

All from reading the code unless marked. None of these could be compiled or heard on this PC.

| Problem | Why it matters | Fix needs |
|---|---|---|
| **Two engines.** The page and the native engine are kept alike by hand, and the app uses the page unless a plug-in is loaded | Every recipe change must be made twice; differences creep in | The plan above: one engine in the app |
| **Nothing guards the very end.** The master fader and your master plug-ins come after the limiter | Overs go straight to the file | A build |
| The limiter is not look-ahead or true-peak; after it, a hard clip | Harsh on loud mixes | A build and a listen |
| Saturation is 4× oversampled in the page, not in the engine | The same song sounds different in the two | A build and a listen |
| Automation moves in steps of 32 samples, no smoothing | Zipper noise on fast moves | A build |
| When all 96 voices are busy, the oldest is cut dead | A click | A build |
| One bad number from a hosted plug-in can silence the mix until restart | Rare, but total | A build (small) |
| Tempo-synced effects (Trance gate) free-run in the engine | Off the grid, and different on every export | A build and a listen |
| Loading a plug-in preset while playing silences plug-in parts for a moment; during an export the gap lands in the file | Audible | A build |
| A plug-in effect's delay on a built-in part is not lined up | That part plays late | A build and a listen |
| A mono instrument plug-in plays in the left ear only | Rare | A build (small) |
| Rendered sections are 16-bit and start and stop with no fade | Possible clicks | A build |
| The page's player runs on a timer (read) | May drop out when the window is minimised. Not tested | Goes away with one engine |

## 5. Smart features that would put Semitone ahead

1. **Match the level.** Semitone knows how loud each sound is. Swap a sound and it offers to keep the balance.
2. **Lift check.** My chorus was not louder than my verse (−16.8 against −16.3 dB, measured). Semitone knows which section is the chorus, so it can say so and offer the lift. No other DAW knows what a chorus is.
3. **A receipt for every automatic change.** One card, what changed, why, Undo. It turns today's biggest weakness into a strength.
4. **Finish checker before export.** Silence at the start (my blank song opened with 29 seconds of nothing), clipping, mono, tails cut off, loudness for where it is going.
5. **Scale snap that knows the chord.** Snap to the chord of the bar, not only the key, and allow borrowed notes where the chord has them.
6. **Quantize that knows the style.** Strength and swing from the style's own groove.
7. **Whole-song mix proposal.** Listen once offline, propose balance and pan as one A/B card.
8. **Ideal EQ band per instrument**, from the role data that already exists.

## Suggested order

1. A "nothing changes silently" sweep with tests (auto bass, linked sections, pass mutes, Follow).
2. Level-matched sounds.
3. Chords that stretch and split.
4. One export dialog.
5. One engine in the app with audio settings. The largest job; it removes most of section 4.
6. The engine guards (end-of-chain safety, bad-number guard, smoothing), in the same build.

## Built since this report (same day, each with a test)

| From the report | What it does now |
|---|---|
| The editor stays on what you are editing | When the song stops, the editor comes back to the section you were working on, with its play marker. A **Follow** button beside Play keeps the old behaviour. A section you pick while it plays always wins |
| A mute is visible everywhere | A part taken out of sections shows on its mixer strip's **M** (hatched on that section, underlined elsewhere) and says where and how to put it back |
| Counts are true | The pass's **Keep** button counts exactly the changes it will make |
| Never changes your work by itself | In a blank song the **bass waits to be asked** (Bass follows the chords, or Fill in). Sections that share notes carry a **↔** on their name in the timeline (hover says which ones), and one click on it gives that section its own notes |
| Presets are roughly equally loud | **Match the level**: after you swap a sound, Semitone measures the old and the new one on your own notes and offers to keep the loudness. The fader does not move; the strip shows "level +12 dB"; the engine gets the same level |
| Scale mode can snap notes to the key | A **Snap** switch beside Scale lock. Off by default, so drawing works as before |
| Section menu | **Duplicate** (a copy right after, with its own name) and **Rename** (typed in place) |

Still open from the lists above: chords that stretch and split, the export dialog, a true empty start, the size setting, audio settings with one engine, and the engine guards (those need a build).

## Where things are

- Branch `claude/heuristic-blackwell-7adfaa`, 8 commits, not pushed, not merged.
- New tests in `tests/web/`: `home-back`, `eq-picture`, `roll-steady`, `pass-words`, `level-hint`, `echo-tail`, `small-words`.
- **The engine part of the echo fix (`Source/SynthFx.h`, `tests/synth_test.cpp`) has not been compiled:** there is no compiler on this PC. The first GitHub build will check it.
- **The app installed on this PC still has the old page**, so "Back to (song)" can still rewrite a song there until this branch is merged and installed. If a song looks different after coming back from Home, press Undo.
