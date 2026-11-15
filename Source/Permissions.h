#pragma once

namespace screencap
{
// Removes every macOS privacy grant screencap holds and prints what it did.
// Returns the process exit code. Other platforms have nothing to remove.
int removeAllPermissions();

// `screencap permissions`: on macOS asks LaunchServices to open ScreenCap.app
// showing its Permissions window, in front; elsewhere prints that there are
// no permissions. Returns the process exit code.
int openPermissionsWindow();

// The Permissions window itself, in the process LaunchServices started.
// Runs until the window closes. macOS only.
int runPermissionsWindow();

// The arguments that make ScreenCap.app run the window rather than the command line.
constexpr auto permissionsCommand = "permissions";
constexpr auto permissionsWindowFlag = "--show-window";
} // namespace screencap
