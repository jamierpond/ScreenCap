#!/usr/bin/env bash
# Builds screencap on this machine and captures display 1 the way this OS
# allows from an ssh session, printing "CAPTURED <path>".
#   macOS:   runs it directly (the grant belongs to whatever started sshd).
#   Windows: runs it as a scheduled task in the logged-in user's desktop
#            session; an ssh session has no desktop to copy.
#   Linux:   uses the user's X11 display if one is set, else starts Xvfb
#            with an xterm on it. Tools come from nix-shell when not installed.
set -euo pipefail

out="$HOME/screencap-test.png"
rm -f "$out"

case "$(uname -s)" in
Darwin)
    just build
    ./build/screencap --list || true
    ./build/screencap --out "$out"
    ./build/screencap --record "$HOME/screencap-test.mp4" --seconds 3
    ;;
MINGW* | MSYS*)
    just build
    exe="$(cygpath -w "$PWD/build/screencap.exe")"
    win_out="$(cygpath -w "$out")"
    task="screencap-test"
    batch="$HOME/screencap-test.cmd"
    printf '"%s" --out "%s" > "%s.log" 2>&1\r\n"%s" --record "%s.mp4" --seconds 3 >> "%s.log" 2>&1\r\necho finished >> "%s.log"\r\n' \
        "$exe" "$win_out" "$win_out" "$exe" "$win_out" "$win_out" "$win_out" > "$batch"
    rm -f "$out.log"
    schtasks //create //f //tn "$task" //sc once //st 00:00 //it //tr "$(cygpath -w "$batch")" > /dev/null 2>&1
    schtasks //run //tn "$task" > /dev/null
    for _ in $(seq 60); do grep -q finished "$out.log" 2> /dev/null && break; sleep 1; done
    schtasks //delete //f //tn "$task" > /dev/null
    cat "$out.log" 2>/dev/null || true
    ;;
Linux)
    nix_packages="cmake ninja just xorg.libX11 xorg.xorgserver xterm ffmpeg"
    run_in_tools() {
        if command -v cmake > /dev/null && command -v Xvfb > /dev/null && command -v ffmpeg > /dev/null; then
            bash -c "$1"
        else
            nix-shell -I nixpkgs=flake:nixpkgs -p $nix_packages --run "$1"
        fi
    }
    run_in_tools "
        set -euo pipefail
        just build
        if [[ -z \"\${DISPLAY:-}\" ]]; then
            Xvfb :77 -screen 0 1280x800x24 & xvfb=\$!
            export DISPLAY=:77
            sleep 1
            xterm -geometry 80x24+100+100 -title screencap-xterm -e 'echo hello from screencap; sleep 30' &
            sleep 2
        fi
        ./build/screencap --list
        ./build/screencap --window screencap-xterm --out '$HOME/screencap-test-window.png'
        ./build/screencap --out '$out'
        ./build/screencap --record '$HOME/screencap-test.mp4' --seconds 3
        kill \${xvfb:-} 2>/dev/null || true
    "
    ;;
esac

[[ -f "$out" ]] && echo "CAPTURED ${out#"$HOME"/}"
