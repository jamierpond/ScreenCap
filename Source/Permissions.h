#pragma once

namespace screencap
{
// Removes every macOS privacy grant screencap holds and prints what it did.
// Returns the process exit code. Other platforms have nothing to remove.
int removeAllPermissions();
} // namespace screencap
