#!/usr/bin/env bash
# Puts the built screencap on PATH at ~/.local/bin. On macOS that is a link
# into ScreenCap.app under ~/.local/libexec/screencap, signed so macOS keeps its
# privacy grants across rebuilds.
set -euo pipefail

build_dir="${1:-build}"
target="$HOME/.local/bin"
mkdir -p "$target"

if [[ -f "$build_dir/screencap.exe" ]]; then
    cp "$build_dir/screencap.exe" "$target/screencap.exe"
    echo "$target/screencap.exe"
elif [[ "$(uname)" == "Darwin" ]]; then
    libexec="$HOME/.local/libexec/screencap"
    mkdir -p "$libexec"
    rm -rf "$libexec/ScreenCap.app"
    cp -R "$build_dir/ScreenCap.app" "$libexec/ScreenCap.app"
    bash "$(dirname "$0")/sign-macos.sh" "$libexec/ScreenCap.app"
    rm -f "$target/screencap"
    ln -s "$libexec/ScreenCap.app/Contents/MacOS/ScreenCap" "$target/screencap"
    echo "$target/screencap -> $libexec/ScreenCap.app"
else
    cp "$build_dir/screencap" "$target/screencap"
    echo "$target/screencap"
fi
