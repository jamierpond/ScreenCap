#include "Permissions.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace screencap
{
namespace
{
constexpr int exitOk = 0;
constexpr int exitFailed = 1;

#if defined(__APPLE__)
constexpr auto bundleIdentifier = "audio.pond.screencap";
constexpr auto manualStep =
    "remove screencap by hand: System Settings > Privacy & Security > Screen & System Audio "
    "Recording, select screencap, click the minus (-) button\n";

struct PrivacyService
{
    const char* tccName;
    const char* settingsName;
};

constexpr PrivacyService privacyServices[] = {
    {"ScreenCapture", "Screen Recording"},
};

bool resetService(const PrivacyService& service)
{
    const auto command = std::string("/usr/bin/tccutil reset ") + service.tccName + " " + bundleIdentifier
                         + " >/dev/null 2>&1";
    return std::system(command.c_str()) == 0;
}
#endif
} // namespace

int removeAllPermissions()
{
#if defined(__APPLE__)
    auto isEveryServiceReset = true;

    for (const auto& service : privacyServices)
    {
        if (resetService(service))
        {
            std::printf("removed %s\n", service.settingsName);
            continue;
        }

        isEveryServiceReset = false;
        std::fprintf(stderr, "screencap: tccutil could not remove %s\n", service.settingsName);
    }

    if (isEveryServiceReset)
        return exitOk;

    std::fputs(manualStep, stderr);
    return exitFailed;
#else
    std::puts("nothing to remove: screencap holds no permissions on this platform");
    return exitOk;
#endif
}
} // namespace screencap
