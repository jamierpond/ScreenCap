#include "DesktopSession.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace screencap
{
namespace
{
constexpr int exitFailed = 1;

constexpr auto waylandOnlyMessage =
    "screencap: your desktop session is Wayland with no X11 display. Wayland lets a program see the "
    "screen only through the desktop portal, which asks the person at the screen every time, so "
    "screencap can't capture it over ssh. Log in to an X11 session instead\n";

constexpr auto xwaylandNote =
    "screencap: this desktop is Wayland; through its X11 display (XWayland) screencap sees only "
    "X11 windows, and Wayland windows come out black\n";

constexpr auto noSessionNote =
    "screencap: $DISPLAY is not set and none of your processes has one: nobody is logged in to a "
    "desktop as you (the login screen does not count)\n";

using Environment = std::map<std::string, std::string>;

bool hasVariable(const char* name)
{
    const auto value = std::getenv(name);
    return value != nullptr && *value != '\0';
}

bool isOwnedByThisUser(const fs::path& processFolder)
{
    struct stat status {};

    if (stat(processFolder.c_str(), &status) != 0)
        return false;

    return status.st_uid == getuid();
}

// /proc/<pid>/environ holds the process's variables as NAME=value strings separated by NUL bytes.
Environment readProcessEnvironment(const fs::path& processFolder)
{
    auto stream = std::ifstream(processFolder / "environ", std::ios::binary);
    const auto contents = std::string(std::istreambuf_iterator<char>(stream), {});
    auto environment = Environment {};
    auto start = std::size_t {0};

    while (start < contents.size())
    {
        const auto end = contents.find('\0', start);
        const auto entry = contents.substr(start, end - start);
        const auto equals = entry.find('=');

        if (equals != std::string::npos)
            environment[entry.substr(0, equals)] = entry.substr(equals + 1);

        if (end == std::string::npos)
            break;

        start = end + 1;
    }

    return environment;
}

struct GraphicalSession
{
    bool hasX11Display = false;
    bool hasWaylandDisplay = false;
    Environment environment;
};

GraphicalSession findGraphicalSession()
{
    auto session = GraphicalSession {};
    auto error = std::error_code {};

    for (const auto& entry: fs::directory_iterator("/proc", error))
    {
        if (!isOwnedByThisUser(entry.path()))
            continue;

        auto environment = readProcessEnvironment(entry.path());

        if (environment.contains("DISPLAY"))
            return {true, environment.contains("WAYLAND_DISPLAY"), environment};

        if (environment.contains("WAYLAND_DISPLAY"))
            session.hasWaylandDisplay = true;
    }

    return session;
}

void useVariableFrom(const Environment& environment, const char* name)
{
    if (const auto found = environment.find(name); found != environment.end())
        setenv(name, found->second.c_str(), 1);
}
} // namespace

std::optional<int> runInUserDesktopSession(char**)
{
    if (hasVariable("DISPLAY") || hasVariable("WAYLAND_DISPLAY"))
        return std::nullopt;

    const auto session = findGraphicalSession();

    if (session.hasX11Display)
    {
        useVariableFrom(session.environment, "DISPLAY");
        useVariableFrom(session.environment, "XAUTHORITY");

        if (session.hasWaylandDisplay)
            std::fputs(xwaylandNote, stderr);

        return std::nullopt;
    }

    if (session.hasWaylandDisplay)
    {
        std::fputs(waylandOnlyMessage, stderr);
        return exitFailed;
    }

    std::fputs(noSessionNote, stderr);
    return std::nullopt;
}
} // namespace screencap
