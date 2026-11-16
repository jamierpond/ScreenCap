#pragma once

namespace screencap
{
// The first half of `screencap setup`: checks what this machine needs before screencap can capture
// and fixes what it can. Linux: installs the virtual X11 desktop (scripts/setup-linux-virtual-desktop.sh).
// macOS: checks screencap is ScreenCap.app and that you are logged in to the desktop. Windows: checks
// you are logged in to a desktop. Prints what a person must do for anything it cannot fix.
// Returns 0 when a test capture is worth trying, else the process exit code.
int prepareMachineForCapture();

constexpr auto setupCommand = "setup";
} // namespace screencap
