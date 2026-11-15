#include "Permissions.h"

#include <PermissionsPanel/PermissionsPanel.h>

#include <eacp/Core/App/App.h>
#include <eacp/Graphics/Window/Window.h>
#include <eacp/UI/UI.h>

#include <cstdio>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <vector>

extern char** environ;

namespace screencap
{
namespace
{
using pond::permissions::Info;
using pond::permissions::Kind;
using eacp::UI::Color;

constexpr int exitOk = 0;
constexpr int exitFailed = 1;

constexpr auto appName = "ScreenCap";
constexpr auto relaunchScriptName = "screencap-relaunch";
constexpr auto fontName = "Menlo";
constexpr auto windowWidth = 760;
constexpr auto windowHeight = 480;

// The colours of CowTerm's default theme (Tokyo Night), so the two panels match.
constexpr auto foreground = 0xc0caf5;
constexpr auto background = 0x1a1b26;
constexpr auto selection = 0x33467c;
constexpr auto green = 0x9ece6a;
constexpr auto red = 0xf7768e;
constexpr auto yellow = 0xe0af68;
constexpr auto blue = 0x7aa2f7;
constexpr auto grey = 0xa9b1d6;
constexpr auto darkGrey = 0x414868;

struct PrivacyService
{
    const char* tccName;
    const char* settingsName;
};

constexpr auto manualStep =
    "remove ScreenCap by hand: System Settings > Privacy & Security > Screen & System Audio "
    "Recording, select ScreenCap, click the minus (-) button\n";

constexpr PrivacyService privacyServices[] = {
    {"ScreenCapture", "Screen Recording"},
    {"AudioCapture", "System Audio Recording"},
};

Color toColour(int rgb)
{
    constexpr auto channelMax = 255.0f;
    constexpr auto redShift = 16;
    constexpr auto greenShift = 8;
    constexpr auto channelMask = 0xff;

    return {(float) ((rgb >> redShift) & channelMask) / channelMax,
            (float) ((rgb >> greenShift) & channelMask) / channelMax,
            (float) (rgb & channelMask) / channelMax,
            1.0f};
}

std::vector<Info> getScreenCapGrants()
{
    return {
        {Kind::screenRecording,
         "Screen Recording",
         "Every capture and recording: screencap sees nothing without it.",
         "Screen & System Audio Recording",
         true,
         true},
        {Kind::systemAudioRecording,
         "System Audio Recording",
         "Sound in recordings, once screencap records audio.",
         "Screen & System Audio Recording",
         true,
         false},
    };
}

pond::PermissionsPanelSettings makePanelSettings()
{
    return {appName,
            getScreenCapGrants(),
            {"macOS asks for these once, for ScreenCap itself, whichever terminal",
             "or agent runs screencap. Each row turns green by itself."},
            "screencap keeps nothing running, so relaunching only reopens this window.",
            fontName,
            {toColour(foreground),
             toColour(background),
             toColour(selection),
             toColour(green),
             toColour(red),
             toColour(yellow),
             toColour(blue),
             toColour(grey),
             toColour(darkGrey)}};
}

struct PermissionsHost final : eacp::UI::ComponentHost
{
    PermissionsHost()
    {
        panel.onClosed = [] { eacp::Apps::quit(); };
        panel.onRelaunch = []
        {
            if (pond::permissions::platform::startRelaunchAfterExit(
                    relaunchScriptName, {permissionsCommand, permissionsWindowFlag}))
                eacp::Apps::quit();
        };

        setRootComponent(panel);
        panel.show();
    }

    pond::PermissionsPanel panel {makePanelSettings()};
};

eacp::Graphics::WindowOptions makeWindowOptions()
{
    auto options = eacp::Graphics::WindowOptions {};
    options.width = windowWidth;
    options.height = windowHeight;
    options.title = "ScreenCap permissions";
    return options;
}

struct PermissionsApp
{
    PermissionsApp() { window.toFront(); }

    PermissionsHost host;
    eacp::Graphics::Window window {host, makeWindowOptions()};
};

int runToCompletion(const std::vector<std::string>& args)
{
    auto argv = std::vector<char*> {};

    for (const auto& arg: args)
        argv.push_back(const_cast<char*>(arg.c_str()));

    argv.push_back(nullptr);

    auto child = pid_t {};

    if (posix_spawn(&child, argv[0], nullptr, nullptr, argv.data(), environ) != 0)
        return exitFailed;

    auto status = 0;

    if (waitpid(child, &status, 0) != child || !WIFEXITED(status))
        return exitFailed;

    return WEXITSTATUS(status);
}
} // namespace

int removeAllPermissions()
{
    auto isEveryServiceReset = true;

    for (const auto& service: privacyServices)
    {
        auto error = std::string {};

        if (pond::permissions::platform::reset(service.tccName, error))
        {
            std::printf("removed %s\n", service.settingsName);
            continue;
        }

        isEveryServiceReset = false;
        std::fprintf(stderr, "screencap: could not remove %s: %s\n", service.settingsName, error.c_str());
    }

    if (isEveryServiceReset)
        return exitOk;

    std::fputs(manualStep, stderr);
    return exitFailed;
}

int openPermissionsWindow()
{
    const auto bundle = pond::permissions::platform::bundlePath();

    if (bundle.empty())
    {
        std::fputs("screencap: not running from ScreenCap.app, so there is no window to open\n", stderr);
        return exitFailed;
    }

    return runToCompletion({"/usr/bin/open", bundle, "--args", permissionsCommand, permissionsWindowFlag});
}

int runPermissionsWindow()
{
    return eacp::Apps::run<PermissionsApp>();
}
} // namespace screencap
