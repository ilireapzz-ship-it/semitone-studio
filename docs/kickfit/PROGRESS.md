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

**Not done yet**
- Loading it in FL Studio and checking the sidechain steps.
- The test bench on your kicks, and the blind A/B listening test.
- Mac build (GitHub does this when the branch is pushed).
