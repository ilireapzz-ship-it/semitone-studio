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
- Known problem: Sit it compares the kick with the whole song, because that is what was measured in your songs. With only a
  bass playing it turns the kick down much too far (it hit the -12 dB limit). Until that is measured properly, use Sit it
  only when the rest of the song is playing into the bass's Kick Fit, or set Level by ear.

**Not done yet**
- Tune's delay compensation in FL.
- The test bench on your kicks, and the blind A/B listening test.
- Mac build (GitHub does this when the branch is pushed).
