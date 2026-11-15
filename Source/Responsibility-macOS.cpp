#include "Responsibility.h"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;

namespace screencap
{
namespace
{
constexpr auto disclaimedMarker = "SCREENCAP_DISCLAIMED";
constexpr auto disclaimFunctionName = "responsibility_spawnattrs_setdisclaim";
constexpr int exitSpawnFailed = 1;
constexpr int exitKilledBySignalBase = 128;

using DisclaimFunction = int (*)(posix_spawnattr_t*, int);

std::optional<DisclaimFunction> findDisclaimFunction()
{
    const auto symbol = dlsym(RTLD_DEFAULT, disclaimFunctionName);

    if (symbol == nullptr)
        return std::nullopt;

    return reinterpret_cast<DisclaimFunction>(symbol);
}

int waitForExitCode(pid_t child)
{
    auto status = 0;

    while (waitpid(child, &status, 0) < 0)
    {
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    if (WIFSIGNALED(status))
        return exitKilledBySignalBase + WTERMSIG(status);

    return exitSpawnFailed;
}
std::string getExecutablePath()
{
    auto size = std::uint32_t {0};
    _NSGetExecutablePath(nullptr, &size);
    auto path = std::string(size, '\0');
    _NSGetExecutablePath(path.data(), &size);
    path.resize(std::char_traits<char>::length(path.c_str()));
    return path;
}
} // namespace

std::optional<int> runAsResponsibleProcess(char** argv)
{
    if (std::getenv(disclaimedMarker) != nullptr)
        return std::nullopt;

    const auto disclaim = findDisclaimFunction();

    if (!disclaim)
        return std::nullopt;

    setenv(disclaimedMarker, "1", 1);

    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    (*disclaim)(&attributes, 1);

    const auto executablePath = getExecutablePath();
    auto child = pid_t {};
    const auto spawnError = posix_spawn(&child, executablePath.c_str(), nullptr, &attributes, argv, environ);
    posix_spawnattr_destroy(&attributes);

    if (spawnError != 0)
        return std::nullopt;

    return waitForExitCode(child);
}
} // namespace screencap
