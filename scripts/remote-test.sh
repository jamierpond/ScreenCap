#!/usr/bin/env bash
# Copies this checkout's HEAD to an ssh host, builds screencap there, captures display 1 there,
# and copies the PNG to captures/<host>.png. Usage: remote-test.sh <ssh host>
set -euo pipefail

host="$1"
local_png="captures/${host//[@:]/-}.png"
mkdir -p captures

git archive --format=tar HEAD \
    | ssh "$host" "mkdir -p ~/projects/ScreenCap && tar -x -C ~/projects/ScreenCap"
ssh "$host" "cd ~/projects/ScreenCap && bash scripts/capture-here.sh" \
    > captures/remote-test.log 2>&1 || { cat captures/remote-test.log; exit 1; }

cat captures/remote-test.log
remote_png="$(grep '^CAPTURED ' captures/remote-test.log | tail -1 | cut -d' ' -f2-)"
scp -q "$host:$remote_png" "$local_png"
echo "$local_png"
