#!/usr/bin/env bash
# remote-setup.sh for a Windows host reachable only through another ssh host (a Parallels VM behind its Mac):
# copies HEAD there, builds with MSVC, runs `screencap setup` and copies its test capture to
# captures/<windows host>-setup.png. Usage: remote-setup-windows.sh <jump host> <windows host>
set -euo pipefail

jump_host="$1"
windows_host="$2"
local_png="captures/${windows_host//[@:]/-}-setup.png"
log="captures/remote-setup.log"
remote_folder='projects\ScreenCap-setup'
mkdir -p captures

# The Windows host's ssh shell is cmd.exe in the user's home folder.
run_on_windows() {
    ssh -o BatchMode=yes -o ConnectTimeout=8 "$jump_host" \
        "ssh -o BatchMode=yes -o ConnectTimeout=8 $windows_host $(printf '%q' "$1")"
}

git archive --format=tar HEAD \
    | run_on_windows "(if not exist $remote_folder mkdir $remote_folder) && tar -xm -C $remote_folder"
run_on_windows "cd $remote_folder && (scripts\\build-windows.cmd > build.log 2>&1 || (powershell -NoProfile -Command Get-Content build.log -Tail 40 & exit 1)) && build\\screencap.exe setup" \
    < /dev/null > "$log" 2>&1 && status=0 || status=$?

cat "$log"
remote_png="$(grep -E '^[A-Za-z]:.*screencap-setup-test\.png' "$log" | tail -1 | tr -d '\r' || true)"

if [[ -n "$remote_png" ]]; then
    run_on_windows "tar -cf - -C \"${remote_png%\\*}\" screencap-setup-test.png" < /dev/null | tar -xO > "$local_png"
    echo "$local_png"
fi

exit "$status"
