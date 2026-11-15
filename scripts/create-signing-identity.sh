#!/usr/bin/env bash
# Creates, once, a self-signed code-signing identity in its own keychain, so
# every build of screencap has the same signature and macOS keeps its Screen
# Recording grant across rebuilds. Safe to run again: it stops if it exists.
set -euo pipefail

keychain="$HOME/Library/Keychains/screencap-signing.keychain-db"
password="screencap-signing-local"
name="ScreenCap Local Signing"

if [[ -f "$keychain" ]]; then
    security unlock-keychain -p "$password" "$keychain"
    exit 0
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

openssl req -x509 -newkey rsa:2048 -nodes -days 36500 -subj "/CN=$name" \
    -addext "keyUsage=critical,digitalSignature" -addext "extendedKeyUsage=critical,codeSigning" \
    -addext "basicConstraints=critical,CA:false" \
    -keyout "$work/key.pem" -out "$work/cert.pem" 2>/dev/null
openssl pkcs12 -export -legacy -inkey "$work/key.pem" -in "$work/cert.pem" \
    -name "$name" -passout pass:"$password" -out "$work/identity.p12" 2>/dev/null \
    || openssl pkcs12 -export -inkey "$work/key.pem" -in "$work/cert.pem" \
        -name "$name" -passout pass:"$password" -out "$work/identity.p12"

security create-keychain -p "$password" "$keychain"
security set-keychain-settings "$keychain"
security unlock-keychain -p "$password" "$keychain"
security import "$work/identity.p12" -k "$keychain" -P "$password" -T /usr/bin/codesign
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$password" "$keychain" >/dev/null
echo "created $keychain"
