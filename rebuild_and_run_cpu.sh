#!/usr/bin/env bash
# rebuild_and_run_cpu.sh — Mac + Linux: reconfigure (if needed), rebuild cpu-lbm, launch tracers2d_live
# Works on macOS (Apple Silicon/Intel, brew) and Linux. No extra deps except cmake + glfw.
# Usage:
#   chmod +x rebuild_and_run_cpu.sh
#   ./rebuild_and_run_cpu.sh                          # build + launch defaults
#   ./rebuild_and_run_cpu.sh -- --nx 512 --particles 5000  # pass args to tracers2d_live
#   ./rebuild_and_run_cpu.sh --watch                  # rebuild+relaunch on every file change
#   ./rebuild_and_run_cpu.sh --watch -- -- --particles 20000 --circle 80 64 18
#
# macOS prereqs (once):  xcode-select --install; brew install cmake glfw  # fswatch optional for --watch
# Linux prereqs (once):  sudo apt update && sudo apt install -y cmake build-essential libglfw3-dev libgl-dev  # inotify-tools optional
set -eo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC="$ROOT/cpu-lbm"
BUILD="$SRC/build"
BIN="$BUILD/tracers2d_live"

# --- arg parse ---
# Supports: ./rebuild_and_run_cpu.sh [--watch] [-- <app-args...>]
# Note: macOS ships bash 3.2 where ${array[@]} + set -u misfire, so we avoid nounset.
WATCH=0
APP_ARGS=()
ARGS=("$@")
HAS_SEP=0
for a in "${ARGS[@]:-}"; do
  if [[ "$a" == "--" ]]; then HAS_SEP=1; break; fi
done

if [[ $HAS_SEP -eq 1 ]]; then
  # split at first --
  BEFORE=()
  AFTER=()
  SEEN=0
  for a in "${ARGS[@]:-}"; do
    if [[ $SEEN -eq 0 && "$a" == "--" ]]; then SEEN=1; continue; fi
    if [[ $SEEN -eq 0 ]]; then BEFORE+=("$a"); else AFTER+=("$a"); fi
  done
  for b in "${BEFORE[@]:-}"; do
    if [[ "$b" == "--watch" ]]; then WATCH=1; fi
  done
  APP_ARGS=("${AFTER[@]:-}")
else
  for a in "${ARGS[@]:-}"; do
    if [[ "$a" == "--watch" ]]; then WATCH=1; fi
  done
fi

build_once() {
  echo "==> [1/2] cmake configure $SRC -> $BUILD"
  # On macOS, GLFW installed via brew is found by CMake; if not, FetchContent will try to fetch (needs network)
  cmake -S "$SRC" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release

  echo "==> [2/2] build (-j)"
  cmake --build "$BUILD" -j
  # tests are not run here — use ctest --test-dir "$BUILD" manually if you want Gate 0a checks
}

launch() {
  if [[ ! -x "$BIN" ]]; then
    echo "!! $BIN not found or not executable"
    echo "   macOS: brew install glfw  (or let CMake FetchContent fetch glfw 3.4 — needs network)"
    echo "   Linux: sudo apt install libglfw3-dev libgl-dev"
    return 1
  fi
  echo "==> launching $BIN ${APP_ARGS[*]:-}"
  # use exec so Ctrl-C goes to viewer when not in watch mode
  exec "$BIN" "${APP_ARGS[@]:-}"
}

# Non-watch: build once and exec viewer (replaces this shell)
if [[ $WATCH -eq 0 ]]; then
  build_once
  launch
fi

# --watch mode
echo "==> watch mode — rebuilding on change (Ctrl-C to stop)"
echo "    src: $SRC/src $SRC/viewer $SRC/apps $SRC/CMakeLists.txt"

# helper to get mtime portably (macOS stat -f %m, Linux stat -c %Y)
mtime() { stat -f %m "$1" 2>/dev/null || stat -c %Y "$1" 2>/dev/null || echo 0; }

if command -v inotifywait >/dev/null 2>&1; then
  echo "    using inotifywait (Linux)"
  build_once
  "$BIN" "${APP_ARGS[@]:-}" &
  APP_PID=$!
  trap 'kill $APP_PID 2>/dev/null || true; exit 0' INT TERM
  while inotifywait -q -r -e modify,create,delete,move "$SRC/src" "$SRC/viewer" "$SRC/apps" "$SRC/CMakeLists.txt" 2>/dev/null; do
    echo ""; echo "--- change detected, rebuilding ---"
    kill $APP_PID 2>/dev/null || true; wait $APP_PID 2>/dev/null || true
    if build_once; then
      "$BIN" "${APP_ARGS[@]:-}" &
      APP_PID=$!
    else
      echo "!! build failed — waiting for next change..."
    fi
  done
elif command -v fswatch >/dev/null 2>&1; then
  echo "    using fswatch (macOS: brew install fswatch)"
  build_once
  "$BIN" "${APP_ARGS[@]:-}" &
  APP_PID=$!
  trap 'kill $APP_PID 2>/dev/null || true; exit 0' INT TERM
  # -r recursive, -0 null-delimited
  fswatch -r -0 "$SRC/src" "$SRC/viewer" "$SRC/apps" "$SRC/CMakeLists.txt" | while read -r -d "" _ev; do
    echo ""; echo "--- change detected, rebuilding ---"
    kill $APP_PID 2>/dev/null || true; wait $APP_PID 2>/dev/null || true
    if build_once; then
      "$BIN" "${APP_ARGS[@]:-}" &
      APP_PID=$!
    else
      echo "!! build failed — waiting for next change..."
    fi
  done
else
  echo "!! neither inotifywait nor fswatch found"
  echo "   instant rebuilds:  Linux: sudo apt install inotify-tools"
  echo "                      macOS: brew install fswatch"
  echo "   falling back to 1s poll (close viewer and save again to rebuild)"
  build_once
  # poll loop — no auto-relaunch to avoid window spam; rebuild on change then relaunch once
  LAST=""
  while true; do
    # portable find + mtime concat
    CUR=""
    while IFS= read -r -d "" f; do
      CUR+="$(mtime "$f"):"
    done < <(find "$SRC/src" "$SRC/viewer" "$SRC/apps" -type f -print0 2>/dev/null; printf "%s\0" "$SRC/CMakeLists.txt")
    if [[ -n "$LAST" && "$CUR" != "$LAST" ]]; then
      echo ""; echo "--- change detected, rebuilding ---"
      if build_once; then
        echo "==> build ok — relaunching viewer"
        break
      else
        echo "!! build failed — fix and save again"
      fi
    fi
    LAST="$CUR"
    sleep 1
  done
  launch
fi
