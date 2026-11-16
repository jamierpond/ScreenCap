#!/usr/bin/env bash
# Copies this checkout's HEAD to an ssh host, builds screencap there (not installed, not signed),
# runs `screencap setup` there and copies its test capture to captures/<host>-setup.png.
# Usage: remote-setup.sh <ssh host>
set -euo pipefail

host="$1"
local_png="captures/${host//[@:]/-}-setup.png"
log="captures/remote-setup.log"
mkdir -p captures

git archive --format=tar HEAD \
    | ssh "$host" "mkdir -p ~/projects/ScreenCap-setup && tar -xm -C ~/projects/ScreenCap-setup"
ssh "$host" "cd ~/projects/ScreenCap-setup && just build < /dev/null > build.log 2>&1 || { tail -40 build.log; exit 1; }
    ./build/screencap setup < /dev/null" > "$log" 2>&1 && status=0 || status=$?

cat "$log"
remote_png="$(grep -E '^(/|[A-Za-z]:).*screencap-setup-test\.png' "$log" | tail -1 | tr -d '\r' || true)"

if [[ -n "$remote_png" ]]; then
    scp -q "$host:$remote_png" "$local_png"
    echo "$local_png"
fi

exit "$status"
