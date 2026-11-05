#!/bin/bash
# Build Semitone Studio on this Mac and install it, in a few minutes instead of a 20-minute cloud build.
#
#   tools/build-mac.sh            build, test, install the plugin (VST3 + AU) and the app
#   tools/build-mac.sh --run      ...and open the app afterwards
#   tools/build-mac.sh --no-install   build and test only
#
# Installs to:
#   ~/Library/Audio/Plug-Ins/VST3/Semitone Studio.vst3
#   ~/Library/Audio/Plug-Ins/Components/Semitone Studio.component
#   ~/Applications/Semitone Studio.app
# Restart FL Studio to load a new plugin build. Only this Mac's processor is built (faster).
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$PWD"
RUN=0; INSTALL=1
for a in "$@"; do case "$a" in --run) RUN=1 ;; --no-install) INSTALL=0 ;; esac; done

say() { printf '\n\033[1m== %s\033[0m\n' "$*"; }

# 1. Compiler: Xcode command line tools
if ! xcode-select -p >/dev/null 2>&1 || ! command -v clang++ >/dev/null; then
  echo "The Xcode command line tools are missing. Run:  xcode-select --install"
  echo "click Install in the window that opens, then run this script again."
  exit 1
fi

# 2. CMake: use the one on this Mac, or put a private copy in .tools/ (no admin rights needed)
CMAKE="$(command -v cmake || true)"
if [ -z "$CMAKE" ]; then
  if [ ! -x "$ROOT/.tools/bin/cmake" ]; then
    say "Getting CMake (one time)"
    python3 -m venv "$ROOT/.tools"
    "$ROOT/.tools/bin/pip" install --quiet --upgrade pip cmake ninja
  fi
  CMAKE="$ROOT/.tools/bin/cmake"
  export PATH="$ROOT/.tools/bin:$PATH"
fi
GEN=()
command -v ninja >/dev/null && GEN=(-G Ninja)

# 3. Tests (sequencer + clip player, no JUCE needed)
say "Tests"
clang++ -std=c++17 -O1 -Wall -ISource tests/sequencer_test.cpp -o build-local-seqtest
./build-local-seqtest | tail -3
rm -f build-local-seqtest

# 4. Build (the first build also downloads JUCE: allow ~10 minutes; later builds take a minute or two)
say "Building ($(uname -m))"
"$CMAKE" -S . -B build-local "${GEN[@]}" -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=$(uname -m)" >/dev/null
"$CMAKE" --build build-local --config Release --parallel "$(sysctl -n hw.ncpu)"

A="build-local/SemitoneStudio_artefacts/Release"
[ -d "$A" ] || A="build-local/SemitoneStudio_artefacts"
for b in "$A/VST3/Semitone Studio.vst3" "$A/AU/Semitone Studio.component" "$A/Standalone/Semitone Studio.app"; do
  codesign --force --deep -s - "$b" >/dev/null 2>&1
done

if [ "$INSTALL" = 1 ]; then
  say "Installing"
  P="$HOME/Library/Audio/Plug-Ins"
  mkdir -p "$P/VST3" "$P/Components" "$HOME/Applications"
  install_bundle() { rm -rf "$2"; ditto "$1" "$2"; echo "  $2"; }
  install_bundle "$A/VST3/Semitone Studio.vst3" "$P/VST3/Semitone Studio.vst3"
  install_bundle "$A/AU/Semitone Studio.component" "$P/Components/Semitone Studio.component"
  install_bundle "$A/Standalone/Semitone Studio.app" "$HOME/Applications/Semitone Studio.app"
  # tell macOS about the fresh app (and forget stale copies elsewhere)
  /System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f "$HOME/Applications/Semitone Studio.app" 2>/dev/null || true
  killall -9 AudioComponentRegistrar 2>/dev/null || true   # so FL and Logic see the new AU straight away
  echo "Done. Restart FL Studio to load the new plugin."
fi

if [ "$RUN" = 1 ]; then
  pkill -x "Semitone Studio" 2>/dev/null || true
  sleep 0.5
  open "$HOME/Applications/Semitone Studio.app"
fi
