#!/usr/bin/env bash
# Gives a Linux machine a permanent X11 desktop that nobody has to log in to, so
# `screencap` works over ssh. Run once per machine; running it again updates it.
#
# Two systemd user services, started at boot (lingering) and restarted if they die:
#   screencap-desktop.service          Xvfb, an X11 server drawing into memory
#   screencap-desktop-windows.service  openbox, so windows get title bars and a size
# Xvfb and openbox come from nix and are pinned under ~/.local/state/screencap-desktop.
#
# It is a virtual screen, not the monitor plugged into the machine. To put an app on it:
#   DISPLAY=:90 some-app &
# then `screencap` or `screencap --window <title>` from any ssh session.
set -euo pipefail

display=":90"
screen_size="1920x1080x24"
seconds_to_wait_for_desktop=10

state="$HOME/.local/state/screencap-desktop"
units="$HOME/.config/systemd/user"
mkdir -p "$state" "$units"

nix build nixpkgs#xorg-server --out-link "$state/xorg-server"
nix build nixpkgs#openbox --out-link "$state/openbox"

cat > "$units/screencap-desktop.service" <<EOF
[Unit]
Description=Virtual X11 desktop that screencap captures over ssh

[Service]
ExecStart=$state/xorg-server/bin/Xvfb $display -screen 0 $screen_size -nolisten tcp
Restart=always
RestartSec=1

[Install]
WantedBy=default.target
EOF

cat > "$units/screencap-desktop-windows.service" <<EOF
[Unit]
Description=Window manager on the virtual X11 desktop
BindsTo=screencap-desktop.service
After=screencap-desktop.service

[Service]
Environment=DISPLAY=$display
ExecStart=$state/openbox/bin/openbox
Restart=always
RestartSec=1

[Install]
WantedBy=default.target
EOF

# Lingering starts this user's services at boot, with nobody logged in.
loginctl enable-linger "$USER"

systemctl --user daemon-reload
systemctl --user enable screencap-desktop.service screencap-desktop-windows.service
systemctl --user restart screencap-desktop.service screencap-desktop-windows.service

for ((second = 0; second < seconds_to_wait_for_desktop; second++)); do
    if systemctl --user is-active --quiet screencap-desktop-windows.service; then
        break
    fi
    sleep 1
done

systemctl --user is-active screencap-desktop.service screencap-desktop-windows.service
loginctl show-user "$USER" -p Linger
echo "virtual desktop on DISPLAY=$display"
