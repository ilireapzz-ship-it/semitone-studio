#!/usr/bin/env bash
# Put the current interface (web/semitone-studio.html) into the installed app and plug-in on THIS computer, without a build.
# The app uses <app data>/Page/index.html when it is newer than the app itself (SemitoneEditor::getResource), so reopen the app
# (or the plug-in window) to see it. An installed update from GitHub is newer and wins again.
# Windows app data: %APPDATA%/Semitone Studio; Mac: ~/Library/Semitone Studio. SEMITONE_DATA overrides, like the app.
set -euo pipefail
cd "$(dirname "$0")/.."
if [ -n "${SEMITONE_DATA:-}" ]; then dir="$SEMITONE_DATA"
elif [ -n "${APPDATA:-}" ]; then dir="$APPDATA/Semitone Studio"
else dir="$HOME/Library/Semitone Studio"; fi
py=python3; command -v python3 >/dev/null 2>&1 || py=python
"$py" tools/embed_data.py --check >/dev/null || { echo "data/ changed: run tools/embed_data.py first"; exit 1; }
"$py" tools/wrap_web.py web/semitone-studio.html "$dir/Page/index.html"
echo "Page installed in $dir/Page. Reopen Semitone Studio (and the plug-in window in FL) to load it."
