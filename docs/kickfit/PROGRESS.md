# Kick Fit: progress notes for Jamie

## 21 Nov 2026
**Done**
- Measured 1,947 of your kick samples (Splice, FL Cloud, FL's packs, Battery) and 299 of your iTunes songs.
- The sound processing is written and passes its automatic tests: it changes nothing when every control is at rest,
  never clips, reads the boom note within a few cents, and the bass dip only touches the lows.
- The Windows plug-in builds on your PC.

**What your songs say**
- Your library is almost all Alternative (220), Pop (53) and Rock (21). Only Pop and Rock have enough songs to set
  targets. Trap/hip-hop, R&B, house/EDM, drum & bass, afrobeats/amapiano and lo-fi show "not enough data".
- Rock (with Alternative): the kick's lows sit about 4 dB under the whole mix when it hits (most songs 3 to 6 dB under).
- Pop: about 5 dB under, but it varies a lot from song to song.
- The boom is usually near 50 Hz (about G1) in both.
- In these songs the kick's note shows no link to the song's key: it fits the key about as often as chance would.
- The bass barely dips under the kick in these songs (under 1 dB), so Room for bass now starts at zero.

**Test bench: 400 of your kicks through Pop and Rock, against backing cut from your songs** (`docs/kickfit/bench-summary.json`)
- Sit it, Rock: 95 kicks already sat in range and all 95 were left alone. Of the rest, 285 moved into range, 12 moved closer,
  3 got worse, 5 couldn't be measured.
- Sit it, Pop: 184 already in range, all left alone; 208 moved into range, 3 closer, none worse.
- Most kicks straight from the pack are too loud for the song: the usual move was 5 to 11 dB down.
- Phone check: of the 31 thinnest kicks, 30 reach normal phone audibility with Click all the way up.
- Tune to key: more than half of your kicks have no single note (the pitch keeps falling for the whole hit). Kick Fit now
  says so and leaves those to your ears. For kicks with a steady note, tuning landed in key (within 20 cents) 84 times in 100.
  That is not good enough yet.

**Checked in FL Studio 2026 on your PC**
- FL's Plugin Manager finds Kick Fit (status ok, VST3, 64-bit, effect).
- It loads on a mixer track and its window shows the picture and every control. (The first try came up empty; fixed.)
- FL reports its delay as "none" with Tune at zero, and offers "1. Input" and "2. Sidechain" under Processing > Connections.
- GitHub builds the Mac VST3 and AU and the Windows VST3.

**With your kick and bass in FL (21 Nov, evening)**
- The kick and bass were on the same mixer track; they need one each. Set up: kick on Insert 1, bass on Insert 2.
- Works: the picture (kick over bass), the boom note, the key heard from the bass, and the bass's lows dipping under the kick.
- Found: FL Studio won't let two tracks feed each other's sidechains. So Kick Fit now works as a pair: the one on the
  bass hears both and shares it with the one on the kick (picture, Sit it, and the Room for bass knob).
- The boom note no longer flickers between hits, and the time picture shows outlines instead of the raw wave.
- The key guess said C major for a bass loop named C minor. Pick the key at the top when you know it.

**The pair, checked in FL with your kick and bass**
- The Kick Fit on the kick shows the bass "from the Kick Fit on the bass", and Sit it responds.
- The Kick Fit on the bass dips the bass's lows under the kick (7.3 dB with Room for bass at 71).
- FL hands an unused sidechain a copy of the track's own sound; Kick Fit now ignores that.
- Sit it used to compare the kick with the whole song, so with only a bass playing it turned the kick down far too much.
  Fixed (21 Nov, late): all 299 songs were measured again, and Sit it now compares the kick's lows with all the lows it hears
  (everything under 300 Hz). In your rock songs the kick's lows sit about 2 dB under all the lows (most songs 1 to 3.6 dB under),
  and that barely changes whether you count the whole mix or only kick and bass. Pop varies a lot more (1 to 12 dB under).
- Bench again with the new target, 400 kicks: Rock 237 already in range, 125 moved into range, 31 closer, 1 worse.
  Pop 345 already in range, 45 moved into range, 4 closer, none worse.
- Tried in FL with your 505 kick and synth bass: Sit it turned the kick down 8 dB and the marker landed inside the green
  range. That used all of the Level knob (-12 dB), so Level now goes to 24 dB either way (in the next build).
- Still not right: the boom note of the 505 kick reads differently from hit to hit (A1, B1, C2), so the Tune to key
  offer changes too. Don't trust Tune to key on that kick yet.
- The saved test project had lost the kick-to-bass sidechain; it was put back in the open session, not saved.

**Not done yet**
- Tune's delay compensation in FL.
- The test bench on your kicks, and the blind A/B listening test.
- Mac build (GitHub does this when the branch is pushed).

## 21 Nov 2026, night: what a peak meter shows in your songs
Measured on the same 299 songs (finished, mastered songs: they reach about -0.3 dB at their highest).
How far under the song's highest peak each part usually reaches:

| | Rock (242 songs) | Pop (53 songs) |
|---|---|---|
| Kick, each hit | about 2.5 dB under (most 1.5 to 5) | about 4.5 dB under (most 2 to 17.5) |
| Vocal, most of the time | about 6.5 dB under (most 5 to 8) | about 8 dB under (most 5 to 9.5) |
| Vocal, its loudest moments | about 2 dB under | about 2 dB under |
| Bass, most of the time | about 11 dB under (most 9 to 14.5) | about 13.5 dB under (most 10 to 17) |
| Bass, its loudest moments | about 7.5 dB under | about 7.5 dB under |

- The singing is about as loud as everything else put together while it sings: 1 dB under in rock, 1 dB over in pop.
- The kick figure comes from the drum part at kick hits, so a snare or hat landing with the kick is counted in.
- Splitting a song into parts is not perfect, so treat these as within a dB or two.
- Vocal fit, first version (built and tested, not yet tried in FL): a third "I am on: The vocal" setting. It hears the rest of
  the song on its sidechain, shows the vocal against it, and Sit it sets Level to where your songs have the singing
  (rock: 2.5 dB under to 1.3 over everything else; pop: 1.6 under to 3.3 over). Only Level changes the sound.
- Vocal fit tried in FL on a real vocal: Sit it turned it down 12.4 dB into the Pop range. Fixed: the bar now follows Level at once.
- Found in FL: Kick Fit listens before the mixer fader, so it cannot see fader moves. With only a kick, bass and vocal in a
  project the peak heights above (from full songs) and Kick Fit's bar disagree about the bass: in a full song other
  instruments share the low end. Not solved.

## Fit Bus (new, first piece)

A second plug-in for the bus (or master) the kick, bass and vocal meet on. First piece only:
- A limiter that looks 5 ms ahead. Tested: nothing passes the ceiling; under the ceiling the sound is untouched, only late.
- Its picture is the level over time on a plain dB scale (every 6 dB the same distance), each column coloured by pitch:
  red under 60 Hz, orange bass, yellow, green, light blue, deep blue above 5 kHz. Pale tops show what the limiter took off.
- Built and installed on the Windows PC. Not yet opened in FL Studio.
- Not built yet: knowing which part is the kick, bass and vocal (plan: each Kick Fit tells Fit Bus what it is on, which is
  certain, where guessing from the mixed sound is not), and the suggested fixes for common mix problems from measurements.
- Fit Bus now knows the parts: every Kick Fit reports what it is on, how strong its output is in six ranges of pitch, and
  its peak, through a small shared block of memory (numbers only). Fit Bus works out how much of each part is in the bus
  after the faders, shows each part's peak against the loudest peak next to the measured range for the genre, and a
  button asks that Kick Fit to change its Level. Tested on made-up parts: faders read to within 1 dB (2.5 dB for a part
  20 dB down). If the bus holds much more than the parts with a Kick Fit, it says it cannot read them. Not yet tried in FL.
- Kick Fit: Level now works on the bass too. Fixed: with no sidechain bus at all, the right channel was left unprocessed.
- Fit Bus tried in FL on the kick, bass and vocal test project: it found all three Kick Fits, read the faders exactly
  (kick 0, bass -10, vocal 0 dB) within a few seconds of play, and its button moved the vocal into range. It now compares
  the bass and vocal with the kick, song by song as measured (pop: bass 11.1 dB under to 0.9 over the kick, vocal 4.2
  under to 5.7 over; rock: bass 9.7 to 6.4 under, vocal 5.0 to 0.6 under), because with three parts in a bus the tallest
  part is the loudest peak. Known: a reading wobbles by a dB or so, and a part can briefly show "too quiet to read".
- Kick Fit: a fourth "I am on: Everything else" setting for the rest of the music (changes nothing unless Level moves), so
  Fit Bus can read a whole song. Fit Bus now needs the parts to explain 97% of the bus before it trusts its reading.
- Fit Bus: tone balance. Six bars show how the mix is shared between the six pitch ranges, against the finished songs of
  the genre (measured on 244 rock and 54 pop songs). Also measured, not used yet: loudness (rock about -8.5 LUFS, pop
  about -10) and peak over average level. A "spikiness" number was tried and tells songs apart too little to use.
- Fit Bus: Transients (Attack and Sustain) and Tame a range (one of the six ranges held down only when it goes over a
  threshold: a de-esser for the top ranges, a boom tamer for the bottom ones; Hear it plays just that range). Both do
  nothing at all at rest. Tested on made-up sounds. Not yet tried in FL.
- Tried in FL on the test project: the tone bars draw (that mix has far less air than the pop songs), Tame on the sub range
  held it down by up to about 4 dB, and Attack moved the loudest peak from -5.8 dB to -2.8 dB turned up and -7.7 dB turned down.
- Fit Bus: loudness. It measures what leaves the way finished songs are measured (LUFS, BS.1770 gating; a -20 dB test tone
  reads -20), shows it against the songs of the genre (pop -13.0 to -7.9, middle -10.1; rock -10.6 to -7.5, middle -8.5)
  and Set it turns Loudness toward the middle. Not yet tried in FL.
- Loudness tried in FL: the reading works. With the Ceiling at -12 dB, Set it added 11.8 dB of Loudness and the mix got
  only 1 dB louder, so Set it now says to raise the Ceiling in that case and changes nothing.
- Jamie spotted that taming a range did not move that range's tone bar: the bars were reading what arrives. They now read
  what leaves Fit Bus, after Tame, Transients and the limiter, and settle in about 3 s. The three plug-in names (Kick Fit,
  Bass Fit, Vocal Fit) are dropped: Jamie doesn't need them.
