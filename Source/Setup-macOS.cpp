#include "Setup.h"

#include <PermissionsPanel/PermissionsPanel.h>

#include <cstdio>
#include <optional>
#include <string>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

namespace screencap
{
namespace
{
constexpr int exitOk = 0;
constexpr int exitFailed = 1;
constexpr uid_t rootUserId = 0;

constexpr auto notInsideAppMessage =
    "screencap setup: this screencap is not inside ScreenCap.app, and macOS gives Screen Recording only "
    "to an app. Install it with `just install` in the ScreenCap repository, then run "
    "~/.local/bin/screencap setup.\n";

constexpr auto nobodyLoggedInMessage =
    "screencap setup: nobody is logged in to this Mac's desktop (the login window does not count), and "
    "screencap captures only a logged-in desktop. Log in at the Mac's screen or through Screen Sharing, "
    "then run screencap setup again.\n";

std::optional<uid_t> findDesktopUserId()
{
    struct stat status {};

    if (stat("/dev/console", &status) != 0)
        return std::nullopt;

    return status.st_uid;
}

std::string getUserName(uid_t userId)
{
    const auto* const user = getpwuid(userId);

    if (user == nullptr)
        return std::to_string(userId);

    return user->pw_name;
}

bool isThisUserAtTheDesktop()
{
    const auto desktopUserId = findDesktopUserId();

    if (!desktopUserId || *desktopUserId == rootUserId)
    {
        std::fputs(nobodyLoggedInMessage, stderr);
        return false;
    }

    if (*desktopUserId == getuid())
        return true;

    std::fprintf(stderr,
                 "screencap setup: %s is logged in to this Mac's desktop, not you (%s), and screencap "
                 "captures only the desktop of the user running it. Run screencap setup over ssh as %s.\n",
                 getUserName(*desktopUserId).c_str(), getUserName(getuid()).c_str(),
                 getUserName(*desktopUserId).c_str());
    return false;
}
} // namespace

int prepareMachineForCapture()
{
    if (pond::permissions::platform::bundlePath().empty())
    {
        std::fputs(notInsideAppMessage, stderr);
        return exitFailed;
    }

    if (!isThisUserAtTheDesktop())
        return exitFailed;

    return exitOk;
}

SetupTestCapture takeSetupTestCapture()
{
    return {.capture = captureDisplay(setupTestDisplayNumber), .mismatch = {}};
}
} // namespace screencap
