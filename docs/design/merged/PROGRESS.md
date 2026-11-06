# Build progress (merged design + brain)

Codex: tick a step when it is merged and CI passes, and add 2–4 plain-words lines for Jamie (what changed, what to try).
Claude does a design check after each step against the boards and adds any fixes under "Design check".

| # | Step | Done | Notes for Jamie | Design check |
|---|---|---|---|---|
| 0 | Fix the 3 data ISSUES | ☑ | Three plug-in producer moves now point to a real source. Nothing to try; they're still marked as guesses. | n/a |
| 1 | Look: themes as CSS variables, theme picker | ☑ | Five themes, no reload. Press ◐ in the header and try each one, then Follow the system. Fonts are now inside the app. Your choice is shared by the app and the plug-in. | Checked at 1440×900. Text and secondary text pass 4.5:1 in every theme. Tertiary text in `themes.json` is below 4.5:1 in several themes, so nothing important uses it. Daylight's white-on-blue button is 3.65:1. |
| 2 | One header (recipe sentence incl. length) and transport | ☑ | One header and one bar under it, no second toolbar. Click the dashed words in the recipe. Length and purpose are behind ⋯ unless the window is very wide. Orchestral, Classroom, Library and History are behind the gold logo. | Matches boards 1–3. |
| 3 | One timeline with Write / Produce / Mix lenses; new drawer | ☐ | | |
| 4 | Session panel, collapsible rail | ☐ | | |
| 5 | Open-ended parts, layers, Write it for me (part-roles.json) | ☐ | | |
| 6 | Capture, takes, ghost notes, scale lock, ⌘K actions, section drag, snapshots | ☐ | | |
| 7 | Loops and Splice Bridge | ☐ | | |
| 8 | Start, Words, Versions, Ideas shelf | ☐ | | |
| 9 | Pitch & share | ☐ | | |
| 10 | Song length (song-shapes.json) | ☐ | | |
| 11 | The coach (coach.json): weighing, Review, games, degree | ☐ | | |
