#!/usr/bin/env bash
# Builds screencap on an ssh host from GitHub main, captures display 1 there,
# and copies the PNG to captures/<host>.png. Usage: remote-test.sh <ssh host>
set -euo pipefail

host="$1"
repo_url="https://github.com/jamierpond/ScreenCap.git"
local_png="captures/${host//[@:]/-}.png"
mkdir -p captures

ssh "$host" "bash -s" -- "$repo_url" <<'REMOTE' > captures/remote-test.log 2>&1 || { cat captures/remote-test.log; exit 1; }
set -euo pipefail
repo_url="$1"
if [[ -d ~/projects/ScreenCap/.git ]]; then
    git -C ~/projects/ScreenCap pull --ff-only -q
else
    git clone -q "$repo_url" ~/projects/ScreenCap
fi
cd ~/projects/ScreenCap
bash scripts/capture-here.sh
REMOTE

cat captures/remote-test.log
remote_png="$(grep '^CAPTURED ' captures/remote-test.log | tail -1 | cut -d' ' -f2-)"
scp -q "$host:$remote_png" "$local_png"
echo "$local_png"
