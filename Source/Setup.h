#pragma once

#include "Capture.h"

#include <string>

namespace screencap
{
// The first half of `screencap setup`: checks what this machine needs before screencap can capture
// and fixes what it can. Linux: installs the virtual X11 desktop (scripts/setup-linux-virtual-desktop.sh).
// macOS: checks screencap is ScreenCap.app and that you are logged in to the desktop. Windows: checks
// you are logged in to a desktop. Prints what a person must do for anything it cannot fix.
// Returns 0 when a test capture is worth trying, else the process exit code.
int prepareMachineForCapture();

struct SetupTestCapture
{
    CaptureResult capture;
    // Empty when the capture shows what setup put on the screen; otherwise what it shows instead.
    std::string mismatch;
};

// The second half of `screencap setup`: captures display 1. Linux's virtual desktop is empty, so there
// screencap first shows a test pattern window and checks the capture contains it. macOS and Windows
// capture the real desktop and check nothing more.
SetupTestCapture takeSetupTestCapture();

constexpr auto setupCommand = "setup";
constexpr auto setupTestDisplayNumber = 1;
} // namespace screencap
