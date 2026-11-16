#!/usr/bin/env bash
# Builds screencap on this machine and captures display 1 the way this OS
# allows from an ssh session, printing "CAPTURED <path>".
#   macOS, Windows: runs it directly; screencap reaches the logged-in desktop itself.
#   Linux:   puts an xterm on the screencap-desktop service's display (:90, from
#            screencap setup), which screencap finds by itself, or else on its own Xvfb on :77.
#            Tools come from nix-shell when not installed.
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
        if systemctl --user is-active --quiet screencap-desktop.service; then
            unset DISPLAY
            test_display=:90
        else
            Xvfb :77 -screen 0 1280x800x24 > /dev/null 2>&1 & xvfb=\$!
            export DISPLAY=:77
            test_display=:77
            sleep 1
        fi
        DISPLAY=\$test_display xterm -geometry 80x24+100+100 -title screencap-xterm -e 'echo hello from screencap; sleep 30' > /dev/null 2>&1 & xterm=\$!
        sleep 2
        ./build/screencap --list
        ./build/screencap --window screencap-xterm --out '$HOME/screencap-test-window.png'
        ./build/screencap --out '$out'
        ./build/screencap --record '$HOME/screencap-test.mp4' --seconds 3
        kill \${xterm:-} \${xvfb:-} 2>/dev/null || true
    "
    ;;
esac

[[ -f "$out" ]] && echo "CAPTURED ${out#"$HOME"/}"
