Kick Fit
========
A plug-in for the kick channel that helps the kick sit in the mix. VST3 (Windows and Mac) and AU (Mac).

Install
- Windows: copy the "Kick Fit.vst3" folder into C:\Program Files\Common Files\VST3, then restart FL Studio.
- Mac: copy "Kick Fit.vst3" into ~/Library/Audio/Plug-Ins/VST3 and "Kick Fit.component" into ~/Library/Audio/Plug-Ins/Components.

Use
1. Put Kick Fit on the kick's mixer channel. Leave "I am on" set to The kick.
2. Send the bass (or the whole beat without the kick) to Kick Fit's sidechain, so it can judge the kick in context.
3. Pick the genre and play the song. The picture shows the kick (orange) over the sidechain (blue), in pitch and in time.
   The green zone is where the kick's boom sits in your own songs of that genre. No zone means too few songs were measured.
4. Tune to key moves the boom to the nearest note that fits. Sit it sets the kick's level to where your songs have it.
5. Thump, Punch, Click and Tail shape the kick. At rest (0) they change nothing at all.

Make room in the bass
A plug-in on the kick can hear the bass but cannot change it. So put a second Kick Fit on the bass channel, set
"I am on" to The bass, and send the kick to its sidechain. Room for bass then dips only the bass's lows when the kick hits.

Delay
None, unless Tune is moving the note: 3 ms when tuning down, up to 63 ms when tuning up. FL Studio lines this up by itself.

Where the numbers come from
data/kickfit-targets.json: measured from the kicks and songs on Jamie's PC with tools/kick_analyse.py. Totals only.
