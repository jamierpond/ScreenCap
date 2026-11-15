#pragma once

#include <optional>

namespace screencap
{
// Over ssh screencap runs outside the logged-in user's desktop: on Windows
// sshd's session 0 has no desktop, on Linux the shell has no $DISPLAY.
// Windows: re-runs screencap inside the user's desktop session and returns
// its exit code. Linux: points this process at the user's X11 display and
// returns nothing. macOS: see runAsResponsibleProcess. Returns nothing when this process should do the work.
std::optional<int> runInUserDesktopSession(char** argv);
} // namespace screencap
