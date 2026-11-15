#!/usr/bin/env bash
# Builds screencap on this machine and captures display 1 the way this OS
# allows from an ssh session, printing "CAPTURED <path>".
#   macOS, Windows: runs it directly; screencap reaches the logged-in desktop itself.
#   Linux:   screencap finds the logged-in user's X11 display; with nobody logged in
#            this starts Xvfb with an xterm on it. Tools come from nix-shell when not installed.
set -euo pipefail

out="$HOME/screencap-test.png"
rm -f "$out"

case "$(uname -s)" in
Darwin | MINGW* | MSYS*)
    just build
    ./build/screencap --list || true
    ./build/screencap --out "$out"
    ./build/screencap --record "$HOME/screencap-test.mp4" --seconds 3
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
        if ! ./build/screencap --list > /dev/null 2>&1; then
            Xvfb :77 -screen 0 1280x800x24 > /dev/null 2>&1 & xvfb=\$!
            export DISPLAY=:77
            sleep 1
            xterm -geometry 80x24+100+100 -title screencap-xterm -e 'echo hello from screencap; sleep 30' > /dev/null 2>&1 & xterm=\$!
            sleep 2
        fi
        ./build/screencap --list
        ./build/screencap --window screencap-xterm --out '$HOME/screencap-test-window.png'
        ./build/screencap --out '$out'
        ./build/screencap --record '$HOME/screencap-test.mp4' --seconds 3
        kill \${xterm:-} \${xvfb:-} 2>/dev/null || true
    "
    ;;
esac

[[ -f "$out" ]] && echo "CAPTURED ${out#"$HOME"/}"
