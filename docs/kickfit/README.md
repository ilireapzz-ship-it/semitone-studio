Kick Fit
========
A plug-in that helps the kick sit in the mix. VST3 (Windows and Mac) and AU (Mac).

Install
- Windows: copy the "Kick Fit.vst3" folder into C:\Program Files\Common Files\VST3, then in FL Studio use
  Options > Manage plugins > Find installed plugins.
- Mac: copy "Kick Fit.vst3" into ~/Library/Audio/Plug-Ins/VST3 and "Kick Fit.component" into ~/Library/Audio/Plug-Ins/Components.

Set it up in FL Studio (checked in FL Studio 2026 on Windows)
The kick and the bass must be on two different mixer tracks. You use two Kick Fits, one on each. They work as a pair.
1. Kick on its own mixer track (say Insert 1). Bass on another (say Insert 2). In the Channel rack, the small number
   left of each sound is its mixer track.
2. Put Kick Fit on the kick's track. Leave "I am on" set to The kick.
3. Put a second Kick Fit on the bass's track and set its "I am on" to The bass.
4. Send the kick to the bass's Kick Fit: click the kick's mixer track to select it, then right-click the small arrow at
   the bottom of the bass's track and choose "Sidechain to this track".
5. In the bass's Kick Fit window: click the gear (top left), then the second icon (a plug with a gear), then the
   Processing tab. Under Connections, right-click the box next to "2. Sidechain" and pick the kick's track.
   Click the first icon (the plug) to get back to Kick Fit.
6. Play. The Kick Fit on the kick now shows the kick (orange) over the bass (blue), and Sit it works.

Why two: a plug-in on the kick can't change the bass, and FL Studio won't let two tracks feed each other's sidechains.
So the one on the bass listens to both and does the dipping, and shares what it hears with the one on the kick.

Use
- Pick the genre. The green zone is where the kick's boom sits in your own songs of that genre; the green part of the
  bar is how loud the kick's lows are against the mix in those songs. No zone means too few songs were measured.
- Sit it sets the kick's level to where your songs have it. Your ears do the rest.
- Tune to key moves the boom to the nearest note that fits the key (picked at the top, or heard from the bass).
  If the kick's pitch keeps falling it has no single note, and Kick Fit says so.
- Thump, Punch, Click and Tail shape the kick. At 0 they change nothing at all.
- Room for bass (on the kick's Kick Fit) sets how far the bass's lows dip when the kick hits. Only the lows dip.

On a vocal
Put Kick Fit on the vocal's mixer track and set "I am on" to The vocal. Send everything except the vocal to its
sidechain (the same two steps as above: "Sidechain to this track", then Processing > Connections > "2. Sidechain").
The bar shows how loud the singing is against everything else, with the range your songs sit in. Sit it sets Level.
On a vocal only Level changes the sound.

Delay
None, unless Tune is moving the note: 3 ms when tuning down, up to 63 ms when tuning up. FL Studio lines this up by itself.

Where the numbers come from
data/kickfit-targets.json: measured from the kicks and songs on Jamie's PC with tools/kick_analyse.py. Totals only.
