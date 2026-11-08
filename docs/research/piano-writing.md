# Writing two-hand piano parts that sound human: research brief

Date: 2026-11-09
Purpose: generation rules for Semitone's right-hand and left-hand MIDI piano tracks.

## How to read this

- This brief describes techniques in general terms only. It does not reproduce melodies, riffs, lyrics or full chord charts. Short chord-function labels (for example "i–VI loop") are used where they help.
- **Source quality varies.** Good public analysis of pop piano parts is rare. Most search results were sheet-music shop listings, tutorial blurbs or low-quality auto-generated pages, and I ignored the auto-generated ones. Keys and tempos marked "(listing)" come from publisher or Hooktheory data and can differ from the recording. Where a point comes from general piano-arranging practice rather than a cited source, it is marked **[practice]**. Check it by ear before relying on it.
- MIDI note numbers use middle C = C4 = 60. So C2 = 36, C3 = 48, C5 = 72 and C6 = 84.

---

## Part 1: Song and artist studies

### 1.1 Adele, "Someone Like You"

- **Key, tempo and meter:** A major in the original published key (listing). Roughly 66–70 BPM in 4/4, felt as steady eighth notes. One automated analysis reported 135 BPM, which is a double-time detection error.
- **Right hand:** This is the defining feature. The whole song is one broken-chord cell repeated on every chord. Each chord is a three-note triad, broken in the order bottom–middle–top–middle in even eighths (Pianote). That gives four eighths per half bar, two cells per bar. Simply Music describes the accompaniment the same way: a few chords, broken up the same way all the way through.
- **Voicing and inversions:** The triads are chosen for minimal hand movement. Root position on I, second inversion on iii, a root-fifth-root open shape on vi, and second inversion on IV (Pianote). So the right hand barely moves while the harmony changes. This is the key rule for "ballad broken chord": **pick inversions so the top note moves by step or stays put.**
- **Harmony:** A four-chord loop of the I–iii–vi–IV family. Harmonic rhythm is one chord per bar in the verse. Colour chords appear in later sections: add9 or sus2 on V, a minor 7th on vi, and maj7 on IV (Pianote).
- **Left hand:** Low roots, either as octaves or root–fifth, held or re-struck on beat 1 and sometimes beat 3 (Pianote suggests octaves or fifths). For slash chords, play the named bass note.
- **Build [practice, consistent with the recording]:** The verse is right hand alone or with a light left-hand root. In the pre-chorus the left hand moves to octaves. In the chorus the left hand plays octaves on beats 1 and 3, the right-hand pattern spreads up an octave or adds an octave on top, and velocity rises. In the bridge the texture thins, then the last chorus comes back fullest. The pattern never changes, only the register, the doubling and the dynamics.
- **Human feel:** Slight rubato at phrase ends, a sustain pedal changed on each chord change, and the top note of each cell slightly louder.

### 1.2 Adele, "Hometown Glory"

- **Key, tempo and meter:** B-flat minor per Hooktheory, listed at 124 BPM. That reads as a half-time feel of about 62 BPM. A Musicnotes listing says A minor, and a user review on that listing disputes it. Treat the key as a minor key around A to B-flat.
- **Harmony:** Hooktheory's vocabulary is i, iv, VI or VI7, and III, often as a second inversion (III64). III is the most-used chord in every section. That means a minor song whose centre of gravity keeps leaning to the relative major, with inversions keeping the bass smooth.
- **Texture [practice, from listening descriptions]:** A continuous right-hand arpeggio or broken-chord riff that is the hook, played in eighths in the upper-middle register. Under it, the left hand plays sustained low notes, and the bass line moves by step through inversions rather than jumping between roots. Using the III64 inversion is a strong cue: **let the bass walk by step through inverted chords instead of always playing roots.**
- **Build:** The piano riff runs through every section unchanged. Intensity comes from the left hand moving into octaves and from louder dynamics, and in the recording from added strings rather than a new piano pattern.

### 1.3 Yann Tiersen, "Comptine d'un autre été: l'après-midi" (and other Amélie pieces)

- **Key, tempo and meter:** E minor (Pianote), a moderate tempo with steady eighths. Other Amélie pieces such as "La Valse d'Amélie" are 3/4 waltzes with an oom-pah-pah left hand. That is the "Tiersen waltz" style the generator should also cover.
- **Left hand:** A broken-chord ostinato that stays the same for the whole piece (Pianote calls it Alberti-like). It alternates a low chord tone with higher chord tones in even eighths, so the left hand provides both the bass and the pulse. The harmony is a short minor-key loop of four chords, about one chord per bar, cycling endlessly.
- **Right hand, in four stages (Pianote):**
  1. A simple stepwise melody over the ostinato, with held notes and ties.
  2. A harmonised repeat of the melody in parallel sixths.
  3. A "fancy" section where the right hand plays running sixteenths over the left-hand eighths. That is two right-hand notes per left-hand note, in the same chord shape.
  4. The opening theme returns an octave higher.
- **Build rule:** Keep the left hand fixed and escalate the right hand. Go from a single melody line, to the melody in sixths, to sixteenth-note figuration, to the melody an octave up. This is a clean, implementable four-stage build.
- **Human feel:** Pianote recommends rubato and varied dynamics. The ostinato should be quieter than the melody and slightly uneven in velocity.

### 1.4 Philip Glass (the user wrote "Robert Glass"): "Opening" from Glassworks, Metamorphosis, The Hours

- **"Opening" (Glassworks, 1982):**
  - Three rhythmic layers in 4/4 (Glassworks analysis): triplet eighths against duple eighths against whole notes.
  - The right hand runs triplet arpeggios and the left hand runs duple eighths, giving a constant 3-against-2 cross-rhythm.
  - The bass moves only once per bar, on held whole notes.
  - Contrary-motion arpeggios make one piano sound like several (Brown/Yale analysis).
  - High sustained notes appear over the arpeggios.
  - The pulse never wavers, but some notes "drag" slightly. That means human micro-timing over a mechanical grid.
- **Harmony:** Short loops of two to four chords, each lasting one bar or more, repeated many times. "Loop breakers" (the term in Wu's UNT thesis) insert a single new chord to refresh the loop. Glass often uses oscillating minor thirds and sixths.
- **Metamorphosis (1988):**
  - The left hand plays a two-note alternating ostinato and the right hand plays cascading arpeggios (Georgia Straight review).
  - Some movements open with dramatic block chords before the ostinato starts.
  - A surprise new chord arrives near the end (Pianist Musings).
  - The repetition stays alive through shifting dynamics: "subtle shading".
- **The Hours (2002), "Morning Passages":**
  - A continual flow of eighths, with the harmonic rhythm speeding up as the cue progresses (classicalexburns).
  - The texture builds by additive process and micro-variation over repeating harmonic loops (MDPI 2024 abstract).
- **Build rules:**
  - The texture is fixed, and change comes from register, dynamics and harmonic rhythm.
  - Sections repeat in multiples of 2 or 4 bars.
  - The harmonic rhythm can double near the climax.
  - Add an octave in the bass for the louder passes.
- **Pedal:** Change it with every harmony, not per note. This gives a clean but resonant sound.

### 1.5 Carly Comando, "Everyday" (2006 video, released on One Take)

- **Key, tempo and meter:** A minor (listing). Listings disagree on tempo: about 70 BPM (HDpiano) or a quarter note at 140 (another edition). That is the same pulse counted differently, felt as a slow 4 with running eighths.
- **Character:** "Cascading" and "moody" piano balladry, recorded in a single take (Deep Elm / label descriptions).
- **Texture [practice, from the recording]:**
  - A continuous right-hand broken-chord figure in eighths that repeats per chord, with the melody emerging from its top notes.
  - The left hand starts with single low roots and grows into octaves.
  - A minor-key four-chord loop with one chord per bar.
  - It is a slow crescendo piece: each pass through the loop adds weight. The right hand moves up an octave and doubles its top line, the left-hand octaves get louder, and the ending thins back out.
- **Build rule:** This is the textbook "single crescendo arc". Start at velocity 40–50 and reach 95–110 around two-thirds through, then fall away.

### 1.6 "Mad World": Michael Andrews (piano) and Gary Jules, Donnie Darko, 2001

- **Production facts:**
  - Recorded at Andrews' home in under two hours, in about two takes.
  - Andrews moved the chords a half-step from what Jules called out (Slashfilm and others).
  - It turned the busy synth original into "feather-soft notes of piano" (Clash), slowed to a crawl.
  - Jules described it as so intimate that you can hear the fingers on the keys.
- **Key, tempo and meter:** A minor key. Arrangements are published in C minor and A-flat, so the recording's key is not confirmed. Slow, about 80–90 BPM or slower, in 4/4.
- **Harmony:** The song's well-known minor two-chord oscillation in the verse (i to the relative-major III area), plus a VII/IV-type turn in the chorus. One chord per bar or slower.
- **Texture:**
  - **Sparseness is the whole point.** The left hand plays a single low root or a root plus fifth, struck once and held.
  - The right hand plays soft, close-position triads or dyads struck once per beat or less, in the middle register.
  - No arpeggio runs and no octaves. Very little is added across the song. The "build" is maybe one more note in a chord, a slightly higher register, and slightly more velocity.
  - Easy published arrangements use root-position chords in the left hand and the melody in the right hand (Sharona Clarke arrangement listing).
- **Human feel:** Low velocity (30–60), audible release and hammer noise if the sample set allows, and loose timing: the hands often not quite together, with noticeable rubato.

### 1.7 Clint Mansell, "Lux Aeterna" (Requiem for a Dream) and its piano arrangements

- **Key, tempo and meter:** G minor, 70 BPM, 4/4 (Hooktheory). Most piano arrangements are also in G minor.
- **Original:**
  - Kronos Quartet strings with "ghostly piano" (Fandor).
  - It loops and layers, growing more intense. The trailer version "Requiem for a Tower" adds orchestra and choir.
  - The core vocabulary is i, iv, VI and a minor v. The loop is built mostly around i and VI.
- **Piano arrangements:**
  - Tutorials teach the piece as a short repeating right-hand motif of four notes over a short repeating left-hand figure of three notes, joined with sustain pedal (Udemy course description).
  - The common arrangement build **[practice]**:
    1. Single-line right-hand motif with sparse left-hand roots.
    2. The right hand gains octaves.
    3. The left hand gains driving octaves in eighths.
    4. The climax moves to full right-hand chords (octave plus inner chord tone), the left hand plays low octaves, and the texture is often marked by fast repeated notes or tremolo.
  - Then a sudden drop back to the bare motif.
- **Build rule:** This is the "cinematic ostinato" template. Keep the motif, and step up rhythmic density and doubling every 4 or 8 bars, with a hard cut-off at the end.

### 1.8 The Rocky Horror Picture Show (Richard O'Brien, arranged by Richard Hartley, 1973 to 1975)

- **"Science Fiction/Double Feature":**
  - A major per Wikipedia. The end reprise is in B-flat. Hooktheory's transcription is in E-flat, and chord charts give A major.
  - About 91–98 BPM in 4/4, glam-rock styled.
  - Hooktheory gives the intro and verse as I–bVII–IV, a borrowed-chord rock move. The core vocabulary is I, IV and V. Chord sites also list a vi (F#m in A) with an optional substitution.
  - The opening piano is soft and ballad-like. Commentary calls the score basic I–IV–V rock-and-roll "dressed in sequins".
  - **[practice]** 1950s-ballad piano conventions apply: right-hand repeated chords in steady eighths or triplets, a left-hand root–fifth or octave bass, and light glissando or fill flourishes at section ends.
- **"I'm Going Home":**
  - A slow, gospel-tinged power ballad, with backing vocals described as swelling "like a gospel choir". These are weak sources, but they agree with each other.
  - It is the clearest example in this list of the **gospel/rock-ballad 12/8** style. Right-hand block chords are re-struck in triplets, the left hand plays octaves on the main beats with walk-ups between chords, and the build ends in a big final chorus with fuller chords and higher register.
- **"Super Heroes":**
  - G minor, with the vocal range roughly D4–D5 (listing).
  - A slow, melancholy closing number. **[practice]** Treat it as a sparse minor ballad close to the Mad World style, with a slow build.
- **Note:** No published analysis of Hartley's piano parts was found. These style labels come from genre conventions plus listing data.

### 1.9 General pop-ballad and rock piano practice

- **Coldplay, "Clocks":**
  - A right-hand broken-chord riff in eighths grouped 3+3+2 inside 4/4 (the tresillo rhythm) (PianoWithJonny and Playground Sessions).
  - It is syncopation rather than true polyrhythm. Each group starts on a new chord tone, so accents land on eighths 1, 4 and 7.
  - The harmony mixes a major tonic with a minor v (Mixolydian colour).
  - Teachers stress feeling it against a steady four-beat pulse.
- **Coldplay, "The Scientist":**
  - Seventh and sus2 chords with close inner-voice clusters (a "whole-tone cluster" warmth).
  - Right-hand inversions keep the voice-leading smooth, with a small melodic figure on top (PianoWithJonny).
  - The left hand plays a simple low root.
- **Elton John and Billy Joel ballad and gospel-rock style:**
  - The left hand plays driving octaves that alternate with the fifth, or walks between chord changes. That comes from New Orleans and gospel styles (rock-piano analysis).
  - A root–fifth–root left-hand pattern at the start of each bar, with the fifth sometimes anticipated (PianoWithJonny).
  - A 3+3+2 feel in the left hand.
  - The right hand plays block chords with an "Elton" rhythm pattern, and alternates between broken arpeggios, syncopated lines and gospel fills.
- **12/8 gospel ballad left hand:**
  - Count all 12 subdivisions.
  - Left-hand options include simple octaves, octaves moving up and down, octave hops, and triplet walking bass (course outlines).
  - Gospel-blues walk-ups move the left hand up in octaves on the triplet subdivision (playpiano.com).

---

## Part 2: Cross-cutting findings on human feel (from performance research)

- **Melody lead.** The melody note sounds slightly before the accompaniment.
  - A 2026 TISMIR study (Hu et al., over 600 hours of MIDI across six datasets) measured a mean lead of **about 17 ms (SD about 18 ms)**. The melody leads about 70% of the time against all accompaniment, and about 80% against right-hand accompaniment.
  - Classic studies (Goebl 2001, Repp 1996) report 20–30 ms.
  - Much of the lead comes from the melody being played louder. Lead grows with the velocity difference.
- **Bass timing.** The bass lags about 60% of the time, by about 15 ms on average. When it leads, it leads by about 22 ms, and by most on downbeats (about 29 ms). Pianists often anticipate the bass on strong beats.
- **Chord spread.** Spread within a chord is larger on beats than on offbeats. Within-hand spread is smaller than between-hand spread (Goebl et al. 2010).
- **Effect of tempo and texture** on these numbers is negligible (Hu et al.), so fixed ranges are fine.
- **Melody is louder than accompaniment** in about 84–98% of events, and is softer in only 2–16%.
- **Glass:** an unwavering grid with notes that drag slightly. Even "mechanical" minimalism is played with micro-timing.

---

## Part 3: Synthesis, a rule set for the generator

### 3.0 Global conventions

- Grid unit: 1 tick = 1/480 of a quarter note (any PPQ works; ranges below are in musical units or ms).
- **Register defaults:**
  - Left-hand bass zone: 28–55 (E1–G3).
  - Left-hand comfort zone: 36–60 (C2–C4).
  - Right-hand zone: 57–88 (A3–E6).
  - Melody zone: 64–84 (E4–C6).
- **Mud rule:**
  - Below 48 (C3), only octaves, single notes or open fifths.
  - Below 40 (E2), only single notes or octaves.
  - Never put a third below 45 (A2), and never put two notes less than 3 semitones apart below 52.
- **Voicing defaults:**
  - The right hand prefers close voicing within an octave, with add9 or sus2 allowed.
  - The left hand prefers root, root plus fifth, root plus octave, or a tenth (root plus third an octave up, only if the span is allowed).

### 3.1 Named piano styles

Each style is a template: left-hand rules, right-hand rules, registers, rhythm grid, voicing, and section build. "Sections" are intro, verse, pre-chorus, chorus, bridge, final chorus and outro. If the app has fewer section types, map them over.

#### Style A: "Ballad Broken Chord" (Someone Like You, Hometown Glory, Everyday)
- **Meter and tempo:** 4/4 at 60–80 BPM, or 6/8 or 12/8 at 50–70 BPM.
- **Right hand:**
  - A triad broken in eighths with the cell bottom–middle–top–middle, repeated twice per bar.
  - Variants: bottom–top–middle–top, or a 6-note cell up and back for 6/8.
  - Close-position triads within one octave. Choose the inversion for each chord so that **the top note moves by 2 semitones or less from the previous chord**, using a minimal-movement search.
  - Range 57–79.
- **Left hand:**
  - Verse: whole-note root at 40–52, or root–fifth.
  - Chorus: octave roots (root at 33–45 plus the octave above) on beats 1 and 3, or a held octave plus a re-strike on beat 3.
  - Allow passing bass notes (inversions, so the bass moves by step) on beat 4 or between chords, in about 30% of changes.
- **Harmony:** One chord per bar, with a four-chord loop. In the chorus, optionally two chords per bar in the last bar of the phrase.
- **Build:**
  - Intro: right hand alone, velocity 45–55.
  - Verse: plus left-hand single roots.
  - Pre-chorus: left hand to root–fifth or octave, velocity +8.
  - Chorus: left-hand octaves, the right-hand cell moved up an octave or with its top note doubled an octave higher, velocity +15–20.
  - Bridge: drop back to the verse texture.
  - Final chorus: the chorus texture plus right-hand block-chord re-strikes on beat 1, at peak velocity.
  - Outro: right hand alone with a ritardando.
- **Pedal:** Change on every chord change (lift 10–30 ms after the new chord's onset, press again 30–80 ms after).

#### Style B: "Tiersen Ostinato" (Comptine, Amélie). Two sub-modes, straight and waltz.
- **Straight:** 4/4 or 2/4 at 90–110 BPM.
  - The left hand plays a fixed eighth-note broken-chord ostinato that alternates a low note (root, 40–52) with higher chord tones (52–64), e.g. low–high–mid–high. The same shape is used for every chord, shifted to the nearest inversion.
  - Minor-key four-chord loop, one chord per bar.
- **Waltz:** 3/4 at 120–170 BPM.
  - Left hand: oom-pah-pah, with a bass note on beat 1 (36–50, sometimes alternating root and fifth on each bar) and a close two- or three-note chord on beats 2 and 3 (52–64).
- **Right hand:**
  - A simple stepwise melody (range 64–81), mostly quarters and eighths, with held notes.
  - The four-stage escalation over repeats:
    1. A single line.
    2. The melody in parallel sixths below (diatonic sixths, 8–9 semitones).
    3. Sixteenth-note broken chords in the same shape (two right-hand notes per left-hand note).
    4. The melody an octave higher (+12).
- **Build:** The left hand never changes, only its velocity (35 rising to 70). The right hand steps through stages 1 to 4, one stage per 8 or 16 bars.
- **Pedal:** Change once per bar. In waltz mode, change on beat 1.

#### Style C: "Glass Minimalist Arpeggio" (Opening, Metamorphosis, The Hours)
- **Meter and tempo:** 4/4 at 60–110 BPM.
- **Right hand:**
  - Continuous arpeggio. Option 1: triplet eighths, 3 per beat, broken-chord up–down. Option 2: duple eighths in rising and falling 4-note cells.
  - Contrary motion against the left hand where possible.
  - Range 60–84.
  - Occasionally hold a top note over the arpeggio, for 1 bar every 4 bars.
- **Left hand:**
  - Option 1: duple eighths against right-hand triplets (the 3-against-2 rule), as a broken chord or a two-note alternation (oscillating a minor third or sixth).
  - Option 2: a two-note alternating ostinato.
  - Plus a held whole-note bass (36–48), struck on beat 1.
- **Harmony:**
  - Loops of 2–4 chords, each 1–2 bars, repeated 2–4 times.
  - Insert one "loop breaker" (a new chord) every 8–16 bars.
  - Near the climax, halve the harmonic duration.
  - Favour minor keys, the i–VI–III–VII family, and chords that share common tones.
- **Build:**
  - Fixed patterns. Intensity comes from adding the bass octave, moving the right hand up an octave, and dynamic swells (crescendo over 4 bars, decrescendo over 4).
  - Repeat-count variety, not pattern variety.
  - Possible block-chord statement before the ostinato enters (the Metamorphosis opening).
- **Humanise lightly:** timing SD 6–10 ms. Velocity follows the arpeggio contour, with the top note of each cell +5.
- **Pedal:** Change on every harmony change, never mid-harmony.

#### Style D: "Gospel / Rock Ballad 12/8" (I'm Going Home, Elton John, Billy Joel)
- **Meter and tempo:** 12/8 at 50–72 dotted-quarter BPM, or 4/4 with a triplet feel.
- **Right hand:**
  - Block chords of 3–4 notes (close position, top note equals the melody or a chord tone; range 60–84), re-struck on triplet subdivisions. The default pattern is to strike on 1, rest, and re-strike on the 2nd and 3rd triplets of each beat.
  - Add sus4-to-3 resolutions (the "amen" or IV-to-I colour) at cadences.
  - Short fills (a 3–5 note scalar or arpeggio run) in the last two beats of phrases, about 25% of phrase ends.
- **Left hand:**
  - Octave roots on beats 1 and 3, with fifths alternating.
  - Walk-ups and walk-downs (scale steps in octaves, on triplet subdivisions) into chord changes, about 40% of changes.
  - Range 28–55.
- **Harmony:** I, IV, V, vi, plus bVII, a secondary dominant, and the IV–I plagal "amen". One or two chords per bar.
- **Build:**
  - Verse: left-hand single roots and right-hand chords on beats only.
  - Chorus: left-hand octaves and right-hand triplet re-strikes.
  - Final chorus: the right hand adds the top-note octave doubling (4–5 notes, span 12 or less), with a key change up 1–2 semitones optional, and a big rolled final chord.
- **Swing:** Triplet grid. Optionally delay the 3rd triplet by 5–10 ms.

#### Style E: "Cinematic Ostinato" (Lux Aeterna arrangements, Clocks)
- **Meter and tempo:** 4/4 at 70–130 BPM.
- **Right hand:**
  - A short motif of 3–5 notes repeated every bar (or a broken chord grouped 3+3+2 in eighths, Clocks-style, accented on eighths 1, 4 and 7).
  - Over the build, the motif becomes a single line, then octaves (+12), then octave plus an inner chord tone.
- **Left hand:** Moves through the following stages:
  1. Whole-note roots.
  2. Half notes.
  3. Repeated eighth-note octaves.
  4. At the climax, sixteenth-note octave pulsing or a tremolo (fast alternation of root and octave or fifth).
- **Harmony:** Minor loop (around i–VI with iv and v), one chord per bar.
- **Build:**
  - Step density every 4 or 8 bars.
  - Velocity +8–12 per stage.
  - Register: the right hand climbs an octave by the climax.
  - End with a hard stop or a sudden drop to the bare motif (velocity −30).

#### Style F: "Sparse Mad-World"
- **Meter and tempo:** 4/4 at 70–90 BPM, or slower.
- **Left hand:** One note or root–fifth per chord, struck once and held for the chord's whole duration (40–55).
- **Right hand:**
  - Close triads or dyads (60–76) struck on beat 1 and optionally beat 3, or once per beat at very low velocity.
  - No arpeggios.
  - Optionally a single sustained melody note on top.
- **Harmony:** A two-chord minor oscillation in the verse and a four-chord turn in the chorus. One chord per bar or two bars.
- **Build:** Almost none. The chorus adds one chord tone, moves up 0–5 semitones of register, and adds +5–10 velocity.
- **Humanise heavily:** see the "loose" profile in 3.2.

**Style selection hint:**
- Slow minor and sad: F or B.
- Slow major and emotional: A.
- Hypnotic or ambient: C.
- Triplet or soul feel: D.
- Driving or dramatic: E.

### 3.2 Humanisation rules (numeric)

Apply these after generating quantised notes. There are three profiles: **tight** (Glass, Clocks), **normal** (default) and **loose** (Mad World, solo rubato).

**Velocity (MIDI 1–127)**

| Element | tight | normal | loose |
|---|---|---|---|
| RH melody or top voice | base +10–15 | base +12–20 | base +10–25 |
| RH inner and accompaniment notes | base | base | base |
| LH bass (lowest note) | base +0–6 | base +0–8 | base 0–10 |
| LH inner or repeated notes | base −8–12 | base −8–15 | base −10–20 |
| Random jitter per note (uniform) | ±3 | ±6 | ±9 |

- Base levels: soft section 40–55, medium 55–75, loud 75–95. Cap the melody at 115.
- **Beat-position accents (4/4):**
  - Beat 1 +6–8.
  - Beat 3 +3–4.
  - Beats 2 and 4 +0.
  - Offbeat eighths −3–5.
  - Sixteenth off-positions −5–8.
- **In 3/4:** beat 1 +8, beats 2 and 3 −4.
- **In 12/8:** each dotted-quarter beat +5, triplet partials −4.
- **Phrase shaping:** within each 2- or 4-bar phrase, apply a small arch, +0 rising to +8 at about 60% of the phrase and falling back to −4 at the end.
- **Repeated patterns:** never repeat exact velocities. Each pass of an ostinato gets an independent jitter, plus a slow drift of ±4 across 4–8 bars.

**Timing (ms, applied to note-on)**
- Global jitter per note (Gaussian SD): tight 5 ms, normal 10 ms, loose 18 ms. Clip at ±3 SD.
- **Melody lead:** the melody note is 10–30 ms earlier than the other right-hand notes struck with it (mean 17, matching Hu et al.). Scale it with the velocity difference: about 1 ms earlier per 2 velocity units louder, capped at 35.
- **Between hands:**
  - Bass lags by a mean of 10–15 ms in about 60% of events. Leads by 15–30 ms on downbeats in about 40% of events (largest on beat 1).
  - Never let total hand offset exceed 50 ms (normal) or 70 ms (loose).
- **Chord spread** within a hand: 0–15 ms spread, with the low-to-high order 70% of the time. Larger spread on beats than on offbeats.
- **Rolled chords:** big final chords and gospel cadence chords are rolled bottom to top over 40–120 ms.
- **Tempo rubato** (for normal and loose; off for tight):
  - Phrase-end ritardando: the last beat of a 4-bar phrase is stretched by +5–10% duration (normal) or +10–20% (loose). The last 2 beats of a section are stretched by +10–25%. The last bar of the song by +25–50%, with a fermata of 1.5–2× on the final chord.
  - Phrase start: the first beat after a ritardando is recovered gradually over 1–2 beats, not snapped back.
  - Small inner rubato: a sinusoidal tempo wobble of ±2–4% over 2 bars (loose only).
- **Note durations (articulation):** accompaniment legato at 90–100% of grid duration, the melody at 85–100%, and staccato offbeat chords (gospel) at 40–60%. Let the pedal carry the sustain.

**Sustain pedal (CC64)**
- Change on every chord change: send pedal up (0) 0–20 ms **after** the new chord's onset, and pedal down (127) 30–80 ms after that onset. This is a "syncopated" or legato pedal change, which avoids both a gap and blur.
- Glass and the ostinato styles change per harmony. Mad World holds through each chord. Gospel 12/8 changes per chord, with an optional half-pedal (CC64 60–90) on fast walk-ups.
- Never hold the pedal across two different harmonies when the bass moves by step below 48 (mud).
- Release the pedal at the end of the final chord 200–600 ms after the right-hand notes end, for ring-out.

### 3.3 Voice-leading and playability constraints

**Hand span**
- Maximum interval between the lowest and highest note sounding together in one hand: default 12 semitones, allowed up to 14 (a major 9th to a 10th) on about 15% of chords, and never more than 15.
- A broken figure (arpeggio cell) may spread wider, up to 19 semitones within one beat, because the hand moves. But consecutive notes in one hand must be at most 12 apart unless the figure has a rest or a held note.

**Number of notes**
- At most 5 notes per hand at once. The typical right hand uses 3–4. The left hand uses 1–2 by default, 3 only above 48, and 4 never.
- **Fast notes:** at a sixteenth or faster, one note at a time per hand (or octaves in tremolo only).

**Hand separation and position**
- **No crossing:** the left hand's highest note must be at least 1 semitone below the right hand's lowest note at the same moment. Prefer a gap of 3 or more semitones.
- **Shared territory 55–64:** allowed for both hands, but not at the same time on the same key.
- **Zones:** the left hand stays at or below 60 (C4) 95% of the time and never goes above 67. The right hand stays at or above 55 (G3) and never goes below 52.
- **Leaps:** the left hand may leap up to an octave plus a fifth between beats (bass octave jumps are normal). The right hand moves its chord position by 7 semitones or less between consecutive chords, except at section boundaries.

**Voice-leading**
- **Smooth inversions:** for each new right-hand chord, choose the voicing that minimises the total semitone movement of the voices (keep common tones, move the others by step). The top voice moves by 4 semitones or less unless it is the melody.
- **Bass:** roots by default. Use inversions to make stepwise bass lines in about 20–35% of changes (Hometown Glory, descending ballad basses). Avoid parallel octaves or fifths between the outer voices on consecutive block chords (soft rule).
- **Doubling:** in the right hand, double the root or fifth, not the third. Never double the leading tone or a 7th. In the left hand, an octave on the root or fifth is fine.
- **Melody on top:** the top right-hand note at each melody onset is the melody note. Accompaniment notes in the same hand must stay below it, at least 2 semitones lower.

**Variation**
- Vary every fourth repetition of a pattern slightly: drop a note, add a passing note, or change the inversion. Exact copies more than 4 times in a row sound robotic (except in the Glass style, where repetition is the point but velocity still drifts).

### 3.4 Splitting an existing piano MIDI into left and right hands

What the evidence says:
- Fixed split points are a baseline that is right only about 60% of the time in practice (Logic users).
- Finale's "moving split" uses the player's maximum hand span.
- MuseScore's splitter is "complex, not a simple split point".
- Research uses learned models. Hadjakos et al. (2019) used an RNN that beats heuristics, with an offline version better still. Penner reports HMM and rule models with 90.8% hand accuracy on the PIG dataset. A BiLSTM project reports about 92%. Graph neural networks have been used for voice and staff separation.

A heuristic pipeline that is good enough for Semitone:

1. **Use existing information first.** If the file has two tracks or channels, or track names such as "RH", "LH", "Treble", "Bass", "Upper" or "Lower", trust them.
2. **Slice by time.** Group notes into onset clusters, i.e. notes starting within 30 ms of each other.
3. **Find the initial split point for each cluster.**
   - Find the largest pitch gap within the cluster.
   - If the largest gap is 5 semitones or more and both sides fit within the hand span (14 semitones or less), split there.
   - Otherwise fall back to a moving split point: the running median of recent splits, default 60, clamped to 52–67.
4. **Enforce the span.** If one side spans more than 14 semitones, move its extreme notes to the other hand when that keeps both within span. If neither works, assign the outermost notes and accept the result.
5. **Keep voices continuous (the cost function).** For each note, the cost of assigning it to a hand is:
   - (a) the distance from that hand's recent centre of gravity (an exponential moving average of pitch over about the last 1–2 s);
   - (b) plus a penalty if the hand's span would exceed 14;
   - (c) plus a penalty if the hands would cross;
   - (d) plus a penalty if the hand would have more than 5 simultaneous notes;
   - (e) plus a small penalty for switching a sustained melodic line between hands.

   Solve with a Viterbi or dynamic-programming pass over the clusters (offline), or greedily (real time).
6. **Recognise lines and patterns.**
   - A monophonic line moving by step or small leaps (5 semitones or less) stays in one hand.
   - A repeating broken-chord ostinato (pitch pattern repeating every 1–2 beats) stays together in one hand, even if it straddles middle C.
   - The lowest note of every cluster is almost always the left hand, and the highest is almost always the right hand.
7. **Smooth the result.** Remove single isolated hand-switches (A-B-A flips shorter than one beat) unless the span rules force them.
8. **Return a confidence score** (fraction of clusters with low cost). When confidence is low, let the user drag a split line, as Logic and Synthesia do.

---

## Source URLs

Song and artist sources:
- https://www.pianote.com/blog/how-to-play-someone-like-you-adele/
- https://old.simplymusic.com/song-of-the-month-someone-like-you-by-adele/
- https://www.musicnotes.com/sheetmusic/adele/someone-like-you/MN0090054_U1
- https://acousticbrainz.org/2e26fe2b-9f4d-4efa-96a4-d13de78cacac
- https://www.hooktheory.com/theorytab/view/adele/hometown-glory
- https://www.musicnotes.com/sheetmusic/adele/hometown-glory/MN0072576_U5
- https://www.pianote.com/blog/amelie-soundtrack-tutorial/
- https://hdpiano.com/lesson/comptine-dun-autre-ete-lapres-midi-by-yann-tiersen/
- https://musicanalysis.com.au/product/comptine-dun-autre-ete-lapres-midi-series-4-edition-yann-tiersen/
- https://www.math.brown.edu/tbanchof/Yale/project04/gwanalysis.html
- https://musicality.fandom.com/wiki/Glassworks?oldid=3495
- https://www.sputnikmusic.com/album/20611/Philip-Glass-Glassworks
- https://es.scribd.com/document/76594556/wu-thesis (Chia-Ying Wu, UNT thesis on "Opening")
- https://www.straight.com/node/62860
- https://pianistmusings.com/2022/07/09/metamorphosis-by-philip-glass-solo-piano-album/
- https://classicalexburns.com/2020/02/20/philip-glass-morning-passages-hypnotic-piano/
- https://mdpi.com/2076-0752/14/5/117
- https://www.musicnotes.com/sheetmusic/carly-comando/everyday/MN0078979
- https://api.musicnotes.com/sheetmusic/hdpiano/everyday/MN0194670
- https://deepelmdigital.com/album/everyday
- https://xpn.org/carly-comando-everyday/
- https://www.clashmusic.com/features/when-people-run-in-circles-gary-jules-and-mad-world/
- https://www.slashfilm.com/1156965/recording-mad-world-for-donnie-darko-took-no-time-at-all
- https://en.wikipedia.org/wiki/Donnie_Darko_(soundtrack)
- https://www.sheetmusicdirect.com/en-US/se/ID_No/1034455/Product.aspx
- https://www.hooktheory.com/theorytab/view/clint-mansell/lux-aeterna---requiem-for-a-dream
- https://keyframe.fandor.com/how-clint-mansell-scored-requiem-for-a-dream/
- https://wikipedia.com/wiki/Clinton_Mansell
- https://www.udemy.com/course/piano-fastlane/
- https://hdpiano.com/lesson/lux-aeterna-from-requiem-for-a-dream/
- https://en.wikipedia.org/wiki/Science_Fiction/Double_Feature
- https://www.hooktheory.com/theorytab/view/rocky-horror-picture-show/science-fiction-double-feature
- https://www.guitartabsexplorer.com/rocky-horror/science-fiction-double-feature-chords
- https://michaelfab.substack.com/p/what-it-is-what-it-was-part-3
- https://www.sheetmusicdirect.com/en-US/se/ID_No/15856/Product.aspx
- https://pianowithjonny.com/piano-lessons/top-5-coldplay-piano-riffs/
- https://blog.playgroundsessions.com/?p=511
- https://en.wikipedia.com/wiki/Talk:Clocks_(song)
- https://pianowithjonny.com/piano-lessons/3-steps-to-play-piano-like-elton-john/
- https://vectree.io/c/rock-piano-technique-analysis
- https://boards.straightdope.com/t/what-do-people-who-know-think-of-elton-john-as-a-pianist/606590?page=2
- https://playpiano.com/piano-chords/gospel-blues-piano
- https://www.udemy.com/course/ballad-master/

Performance and humanisation research:
- https://transactions.ismir.net/articles/317 (Hu et al., TISMIR 2026, melody lead and asynchrony across MIDI datasets)
- https://iwk.mdw.ac.at/goebl/papers/Goebl_JASA2001_melodyLead.pdf
- https://continuum-hypothesis.com/music/Goebl-InvestigationsBetweenHandSynchronization-2010.pdf
- https://ofai.at/papers/oefai-tr-2003-11.pdf

Hand-separation sources:
- https://dl.gi.de/items/969b1f5c-6eb0-4761-a35c-ac1b067c4991/full (Hadjakos, Waloschek & Leemhuis 2019, "Detecting Hands from Piano MIDI Data")
- https://zenodo.org/record/14877385/files/000055.pdf
- https://ieee-dataport.org/documents/statistical-models-automatic-fingering-annotated-piano-sheet-music-transcription
- https://devpost.com/software/hannds
- https://archives.ismir.net/ismir2011/paper/000058.pdf
- https://logic-users-group.com/threads/moving-specific-piano-notes-from-lh-to-rh-or-vice-versa.12226/
- https://web.pdx.edu/~jnewton/assignments/043ft_finale_tutorials/finale_tutorials/Finale/TRASPDLG.htm
- https://musescore.org/en/node/364621
- https://cycling74.com/forums/topic/creating-two-midi-files-left-hand-and-right-hand-from-a-single-piano-midi-file/
- https://synthesia.app/support/guide/assignHands
- https://lists.linuxaudio.org/archives/linux-audio-user/2005-August/026137.html
