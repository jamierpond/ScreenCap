# ScreenCap

`screencap` captures a display or a window to a PNG (or records an MP4) on macOS, Windows and Linux,
and prints the file's path. `screencap --help` lists the options.

## Over ssh

```bash
ssh <host> screencap --out x.png && scp <host>:x.png .
```

This captures whatever the logged-in user sees on that machine:

- Windows: sshd runs in session 0, which has no desktop, so screencap re-runs itself in the
  logged-in user's desktop through a one-off scheduled task, then prints its output and exit code.
- Linux: with no `$DISPLAY`, screencap takes `DISPLAY` and `XAUTHORITY` from one of your own
  processes in the X11 session you're logged in to. A Wayland-only session can't be captured:
  Wayland shows the screen only through the desktop portal, which asks the person at the screen.
- macOS: ScreenCap.app holds its own Screen Recording grant, so it works from sshd too, once
  someone at that Mac has allowed ScreenCap (`screencap permissions` shows how).

Someone has to be logged in to a desktop on the machine; a login screen doesn't count.

## Build and install

`just install` builds and installs to `~/.local/bin/screencap`. `just remote-test <host>` builds
on an ssh host, captures there and copies the PNG back to `captures/`.
