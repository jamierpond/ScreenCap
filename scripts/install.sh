#!/usr/bin/env bash
# Copies the built screencap onto PATH at ~/.local/bin.
set -euo pipefail

build_dir="${1:-build}"
target="$HOME/.local/bin"
mkdir -p "$target"

if [[ -f "$build_dir/screencap.exe" ]]; then
    cp "$build_dir/screencap.exe" "$target/screencap.exe"
    echo "$target/screencap.exe"
else
    cp "$build_dir/screencap" "$target/screencap"
    if [[ "$(uname)" == "Darwin" ]]; then
        bash "$(dirname "$0")/sign-macos.sh" "$target/screencap"
    fi
    echo "$target/screencap"
fi
