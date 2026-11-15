#pragma once

#include <optional>

namespace screencap
{
// macOS charges Screen Recording to the app at the top of the process tree
// (the terminal, tmux server or sshd). This re-runs screencap as its own
// responsible process so the grant belongs to screencap. Returns the child's
// exit code when it re-ran, or nothing when this process should do the work.
std::optional<int> runAsResponsibleProcess(char** argv);
} // namespace screencap
