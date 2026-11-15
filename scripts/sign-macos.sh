#!/usr/bin/env bash
# Signs screencap with the local identity and a fixed identifier, so macOS
# sees every rebuild as the same app and keeps its Screen Recording grant.
set -euo pipefail

binary="$1"
keychain="$HOME/Library/Keychains/screencap-signing.keychain-db"
identifier="com.pond.screencap"

bash "$(dirname "$0")/create-signing-identity.sh"
identity="$(security find-identity -p codesigning "$keychain" | awk '/ScreenCap Local Signing/ {print $2; exit}')"
codesign --force --keychain "$keychain" --sign "$identity" -i "$identifier" "$binary"
