# Problem search, 25 November 2026

Found by searching and testing copies of Semitone. Nothing here is fixed yet unless it says so.

**Rechecked** = tried again separately (or confirmed in the code that does it). **Tested** = reproduced once, with a script. **Read** = found by reading the code, not run.

## A. Losing work

1. **Two verses recorded in one take: the first verse's take is lost**, and the message says both were recorded. Also choruses, chords and drums. Rechecked (melody).
2. **Ctrl+A then Delete in any pane wipes the melody you can't see** (tried in the mixer). It empties every section sharing it. Undo works. Rechecked.
3. **A damaged autosave is replaced by a starter song** when Semitone opens. Saved song files are safe. Rechecked.
4. **A full disk can cut a saved song short and still say "Saved".** Rechecked in the code.
5. **Save while a part is soloed: the other parts reopen muted**, with no solo lit. Solo also carries into the next song. Rechecked (save).
6. **Start blank from the command list keeps your lyrics, loops and song name**; on the web, Save then goes over the old song. Rechecked (lyrics and name).
7. **Switching arrangement A/B loses named versions** saved since. Rechecked.
8. **Undo after moving, deleting or splitting a section puts vocal takes in the wrong place.** Tested.
9. **A sample loaded straight onto a part is lost** by saving, undo, a key change or moving a section. Tested.
10. **An edit made just after Undo isn't recorded**, so a later Undo or Redo can wipe it. Rechecked.
11. Undoing New song brings the music back but not its name or versions. Tested.
12. The last few seconds before closing can be lost; two open copies overwrite each other's autosave; a crash during a take leaves a recording that looks empty. Read.

## B. Wrong sound or silence

13. **Mixer plug-ins opened with no inputs go silent** (Valhalla VintageVerb, T-RackS 5 and others). Known; needs a build.
14. **M during solo does the opposite**, and unsoloing forgets it. Rechecked.
15. **S on an automation lane doesn't solo.** Rechecked in the code.
16. **Percussion's volume knob does nothing in the app.** Rechecked.
17. **Added parts with your own sample or SoundFont go silent as soon as any part uses a plug-in.** Rechecked.
18. **Section energy under 25% silences its drums**, with nothing saying so. Rechecked.
19. **The final chorus is thinner in the app** than on the web page (no octave doubles); swing and Silence skip added parts. Tested.
20. One bad number from a plug-in silences the song until restart. Read.
21. Recording when the microphone can't open stops all sound until Audio settings is reopened. Rechecked in the code.
22. Choosing a preset while playing drops every plug-in for a moment and can leave notes hanging. Read.
23. After a plug-in fails to load, clicking its notes can stop note preview for all plug-in parts. Read.
24. "Start from [song]" leaves your own bass and added parts in the old key. Rechecked (bass).
25. New song keeps the last song's room, polish, bass style, swing, quirks and emotion. Tested.

## C. Exports

26. **Pitch packs and DAW stems leave out plug-in parts.** Tested.
27. **The click is printed into pitch packs and stems** when Click is set to always. Tested.
28. **The first beat plays again at the end of an export.** Read.
29. **Export doesn't wait** for loops, samples or plug-ins still loading, so they can be missing. Read.
30. DAW export mixes up two parts with the same name (names repeat by themselves: rechecked) and empties parts with accents. Tested.
31. The pitch pack "instrumental" still has the vocal if you comped it. Tested.

## D. Sections

32. **"Write an intro/bridge" shifts added parts and loops onto the wrong sections.** Rechecked (added part).
33. **Moving or deleting a section leaves transition samples and lengths behind.** Rechecked (move).
34. **Bars + on a verse of its own also stretches the other verses**, and resets voicings. Rechecked (length).
35. Splitting a section restarts your bass, turns an automation ramp into a step and doubles a transition. Tested.
36. Changing structure deletes section versions and misplaces loops. Tested.

## E. FL Studio

37. Changing sound mode while playing hangs notes. Read.
38. Macro positions aren't saved; a state loaded with the window open isn't shown. Read.
39. Melody velocity or sample changes don't re-render the section. Tested.

## F. Smaller

40. Undo of "Keep as a new version" does nothing; a newly opened song keeps the last one's loop area. Tested.
41. Kick-pump keeps pumping on the web page where drums are left out; recording into a muted Melody says nothing. Tested.
42. Hosted plug-ins are always told 4/4; settings can be wiped if the file is locked; closing during a scan can hang for a minute. Read.

## Fixed 25 November: plug-in sounds in saves

- Opening a song with the same plug-in on the same part kept the other song's sound, then saved it over the right one.
- Save missed up to ten seconds of knob changes.
- Undo after trying another plug-in lost the first one's sound.
- Two of the same effect took each other's settings when one was removed.
- A late answer from the last song could land in the new one.
- Every check of a plug-in's sound made an undo step that undid nothing.
- A missing effect was retried every second.
