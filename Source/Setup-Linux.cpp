#include "Setup.h"

#include "LinuxSetupScript.h"

#include <cstdio>
#include <optional>
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;

namespace screencap
{
namespace
{
constexpr int exitFailed = 1;
constexpr auto scriptName = "setup-linux-virtual-desktop.sh";

std::optional<pid_t> startSetupScript()
{
    char bash[] = "bash";
    char runScriptFlag[] = "-c";
    auto* const script = const_cast<char*>(linuxSetupScript);
    auto* const scriptNameArgument = const_cast<char*>(scriptName);
    char* arguments[] = {bash, runScriptFlag, script, scriptNameArgument, nullptr};
    auto processId = pid_t {};

    if (posix_spawnp(&processId, bash, nullptr, nullptr, arguments, environ) != 0)
        return std::nullopt;

    return processId;
}

int waitForExitCode(pid_t processId)
{
    auto status = 0;

    if (waitpid(processId, &status, 0) != processId || !WIFEXITED(status))
        return exitFailed;

    return WEXITSTATUS(status);
}
} // namespace

int prepareMachineForCapture()
{
    const auto processId = startSetupScript();

    if (!processId)
    {
        std::fputs("screencap: cannot start bash to run the setup script\n", stderr);
        return exitFailed;
    }

    return waitForExitCode(*processId);
}
} // namespace screencap
