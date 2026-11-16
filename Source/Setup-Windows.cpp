#include "Setup.h"

#include <windows.h>
#include <lmcons.h>
#include <wtsapi32.h>

#include <cstdio>
#include <string>

namespace screencap
{
namespace
{
constexpr int exitOk = 0;
constexpr int exitFailed = 1;
constexpr DWORD servicesSessionId = 0;
constexpr DWORD enumerateVersion = 1;

bool isInServicesSession()
{
    auto sessionId = DWORD {};

    if (!ProcessIdToSessionId(GetCurrentProcessId(), &sessionId))
        return false;

    return sessionId == servicesSessionId;
}

std::string getThisUserName()
{
    auto name = std::string(UNLEN + 1, '\0');
    auto length = static_cast<DWORD>(name.size());

    if (!GetUserNameA(name.data(), &length))
        return {};

    name.resize(length - 1);
    return name;
}

std::string getSessionUserName(DWORD sessionId)
{
    auto* buffer = LPSTR {};
    auto bytes = DWORD {};

    if (!WTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE, sessionId, WTSUserName, &buffer, &bytes))
        return {};

    auto name = std::string {buffer};
    WTSFreeMemory(buffer);
    return name;
}

// The scheduled task screencap runs over ssh starts only in a desktop session where its user is logged in and active.
bool hasActiveDesktopSession(const std::string& userName)
{
    auto* sessions = PWTS_SESSION_INFOA {};
    auto count = DWORD {};

    if (!WTSEnumerateSessionsA(WTS_CURRENT_SERVER_HANDLE, 0, enumerateVersion, &sessions, &count))
        return false;

    auto isFound = false;

    for (auto index = DWORD {0}; index < count && !isFound; ++index)
    {
        const auto& session = sessions[index];
        isFound = session.State == WTSActive && _stricmp(getSessionUserName(session.SessionId).c_str(), userName.c_str()) == 0;
    }

    WTSFreeMemory(sessions);
    return isFound;
}
} // namespace

int prepareMachineForCapture()
{
    if (!isInServicesSession())
        return exitOk;

    const auto userName = getThisUserName();

    if (hasActiveDesktopSession(userName))
        return exitOk;

    std::fprintf(stderr,
                 "screencap setup: %s is not logged in to this machine's desktop. Over ssh screencap captures "
                 "by running a scheduled task in %s's desktop, which exists only while %s is logged in: log "
                 "in as %s at the screen or over Remote Desktop (a disconnected Remote Desktop session does "
                 "not count), then run screencap setup again.\n",
                 userName.c_str(), userName.c_str(), userName.c_str(), userName.c_str());
    return exitFailed;
}

SetupTestCapture takeSetupTestCapture()
{
    return {.capture = captureDisplay(setupTestDisplayNumber), .mismatch = {}};
}
} // namespace screencap
