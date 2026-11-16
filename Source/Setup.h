#pragma once

namespace screencap
{
// `screencap setup`: makes this machine ready to capture. Linux: installs the
// virtual X11 desktop (scripts/setup-linux-virtual-desktop.sh). macOS: opens
// the Permissions window. Windows: nothing to do. Returns the process exit code.
int prepareMachineForCapture();

constexpr auto setupCommand = "setup";
} // namespace screencap
