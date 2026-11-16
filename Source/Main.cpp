#include "Capture.h"
#include "DesktopSession.h"
#include "Permissions.h"
#include "Recorder.h"
#include "Setup.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace fs = std::filesystem;

namespace
{
constexpr int exitOk = 0;
constexpr int exitFailed = 1;
constexpr int exitPermissionDenied = 2;
constexpr int exitUsage = 64;

constexpr auto defaultFramesPerSecond = 15;

constexpr auto usage = R"(usage: screencap [--display <n> | --window <title or app>] [--out <file.png>]
       screencap [--display <n> | --window <title or app>] --record <file.mp4> --seconds <s>
                 [--fps <n>]
       screencap --list
       screencap setup
       screencap permissions
       screencap remove-all-permissions

Captures one display (default 1) or one window to a PNG and prints its path.
--window matches part of a window's title or its app's name, ignoring case;
the largest match wins. --list prints the displays and windows it can see.
Without --out the PNG goes to the system temp folder.
--record writes an H.264 MP4 (default 15 fps), video only for now. On Linux it
needs ffmpeg on PATH.
setup checks what this machine needs to capture, fixes what it can, says what a
person must do for the rest, then takes a test capture. Linux: installs a
virtual X11 desktop on DISPLAY=:90 that runs at boot with nobody logged in
(needs nix). macOS and Windows: checks you are logged in to the desktop; macOS
also checks the Screen Recording permission.
permissions opens ScreenCap's Permissions window (macOS): each grant's live
state, the app to drag into System Settings, reset and relaunch. A capture
macOS refuses opens it too.
remove-all-permissions removes the privacy grants screencap holds (macOS).

Exit codes: 0 ok, 1 failed, 2 screen capture not permitted (macOS), 64 usage.
)";

struct Options
{
    int displayNumber = 1;
    std::optional<std::string> windowMatch;
    std::optional<fs::path> outputPath;
    std::optional<fs::path> recordingPath;
    double recordingSeconds = 0.0;
    int framesPerSecond = defaultFramesPerSecond;
    bool isListRequested = false;
};

template <typename Number>
std::optional<Number> parsePositiveNumber(const std::string& text)
{
    try
    {
        auto parsedLength = std::size_t {0};
        const auto number = static_cast<Number>(std::stod(text, &parsedLength));

        if (parsedLength == text.size() && number > 0)
            return number;
    }
    catch (...)
    {
    }

    return std::nullopt;
}

using OptionSetter = void (*)(Options&, const std::string&);

struct OptionFlag
{
    const char* flag;
    OptionSetter apply;
};

constexpr OptionFlag optionFlags[] = {
    {"--window", [](Options& options, const std::string& value) { options.windowMatch = value; }},
    {"--out", [](Options& options, const std::string& value) { options.outputPath = value; }},
    {"-o", [](Options& options, const std::string& value) { options.outputPath = value; }},
    {"--record", [](Options& options, const std::string& value) { options.recordingPath = value; }},
    {"--display",
     [](Options& options, const std::string& value)
     { options.displayNumber = parsePositiveNumber<int>(value).value_or(0); }},
    {"--seconds",
     [](Options& options, const std::string& value)
     { options.recordingSeconds = parsePositiveNumber<double>(value).value_or(0.0); }},
    {"--fps",
     [](Options& options, const std::string& value)
     { options.framesPerSecond = parsePositiveNumber<int>(value).value_or(0); }},
};

bool applyOption(Options& options, const std::string& flag, const std::string& value)
{
    const auto* found = std::find_if(std::begin(optionFlags), std::end(optionFlags),
                                     [&](const OptionFlag& option) { return flag == option.flag; });

    if (found == std::end(optionFlags))
        return false;

    found->apply(options, value);
    return options.displayNumber > 0 && options.framesPerSecond > 0;
}

std::optional<Options> parseOptions(const std::vector<std::string>& args)
{
    auto options = Options {};

    for (auto index = std::size_t {0}; index < args.size(); ++index)
    {
        const auto& flag = args[index];

        if (flag == "--list")
        {
            options.isListRequested = true;
            continue;
        }

        if (index + 1 >= args.size())
            return std::nullopt;

        if (!applyOption(options, flag, args[++index]))
            return std::nullopt;
    }

    if (options.recordingPath && options.recordingSeconds <= 0.0)
        return std::nullopt;

    return options;
}

fs::path createDefaultOutputPath()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    const auto fileName = "screencap-" + std::to_string(milliseconds) + ".png";
    return fs::temp_directory_path() / fileName;
}

bool writePng(const screencap::Image& image, const fs::path& path)
{
    constexpr auto bytesPerPixel = 4;
    const auto rowStride = image.width * bytesPerPixel;
    const auto pathText = path.string();
    return stbi_write_png(pathText.c_str(), image.width, image.height, bytesPerPixel,
                          image.rgba.data(), rowStride)
           != 0;
}

int printCaptureTargets()
{
    for (const auto& line: screencap::listCaptureTargets())
        std::printf("%s\n", line.c_str());

    return exitOk;
}

screencap::CaptureResult captureTarget(const Options& options)
{
    if (options.windowMatch)
        return screencap::captureWindow(*options.windowMatch);

    return screencap::captureDisplay(options.displayNumber);
}

// Exit code 2: on macOS the Permissions window opens, so the fix is in front of whoever reads this.
int reportPermissionDenied()
{
#if defined(__APPLE__)
    screencap::openPermissionsWindow();
#endif
    return exitPermissionDenied;
}

int recordAndWrite(const Options& options)
{
    const auto path = fs::absolute(*options.recordingPath);
    const auto result = screencap::record({.path = path,
                                           .seconds = options.recordingSeconds,
                                           .framesPerSecond = options.framesPerSecond,
                                           .captureFrame = [&] { return captureTarget(options); },
                                           .audioSource = std::nullopt});

    if (!result.error.empty())
    {
        std::fprintf(stderr, "screencap: %s\n", result.error.c_str());
        return result.isPermissionDenied ? reportPermissionDenied() : exitFailed;
    }

    std::printf("%s\n", path.string().c_str());
    return exitOk;
}

int writeCapture(const screencap::CaptureResult& result, const std::optional<fs::path>& outputPath)
{
    if (!result.hasImage())
    {
        std::fprintf(stderr, "screencap: %s\n", result.error.c_str());
        return result.isPermissionDenied ? reportPermissionDenied() : exitFailed;
    }

    const auto path = fs::absolute(outputPath.value_or(createDefaultOutputPath()));

    if (!writePng(result.image, path))
    {
        std::fprintf(stderr, "screencap: cannot write %s\n", path.string().c_str());
        return exitFailed;
    }

    std::printf("%s\n", path.string().c_str());
    return exitOk;
}

int captureAndWrite(const Options& options)
{
    return writeCapture(captureTarget(options), options.outputPath);
}

constexpr auto testCaptureFileName = "screencap-setup-test.png";

constexpr auto permissionMissingAfterSetup =
    "screencap setup: capturing does not work yet: allow ScreenCap as the line above says (its "
    "Permissions window is now open on this Mac's screen), then run screencap setup again.\n";

// Captures display 1 the way a plain `screencap --out` would, through the same route to the user's desktop.
int takeTestCapture(const fs::path& executable, const fs::path& path)
{
    auto arguments = std::vector<std::string> {executable.string(), "--out", path.string()};
    auto argumentPointers = std::vector<char*> {};

    for (auto& argument: arguments)
        argumentPointers.push_back(argument.data());

    argumentPointers.push_back(nullptr);

    if (const auto childExitCode = screencap::runInUserDesktopSession(argumentPointers.data()))
        return *childExitCode;

    const auto testCapture = screencap::takeSetupTestCapture();
    const auto exitCode = writeCapture(testCapture.capture, path);

    if (exitCode != exitOk || testCapture.mismatch.empty())
        return exitCode;

    std::fprintf(stderr, "screencap setup: %s\n", testCapture.mismatch.c_str());
    return exitFailed;
}

int runSetup(const fs::path& executable)
{
    const auto preparedExitCode = screencap::prepareMachineForCapture();

    if (preparedExitCode != exitOk)
    {
        std::fputs("screencap setup: capturing does not work yet: do what the message above says\n", stderr);
        return preparedExitCode;
    }

    std::puts("screencap setup: taking a test capture of display 1");
    const auto exitCode = takeTestCapture(executable, fs::temp_directory_path() / testCaptureFileName);

    if (exitCode == exitOk)
        std::puts("screencap setup: capturing works; the test capture is the PNG named above");
    else if (exitCode == exitPermissionDenied)
        std::fputs(permissionMissingAfterSetup, stderr);
    else
        std::fputs("screencap setup: capturing does not work yet: the test capture failed with the error above\n", stderr);

    return exitCode;
}

struct NamedCommand
{
    std::vector<std::string> arguments;
    int (*run)();
};

std::optional<int> runNamedCommand(const std::vector<std::string>& args)
{
    const auto commands = std::vector<NamedCommand> {
        {{"remove-all-permissions"}, screencap::removeAllPermissions},
        {{screencap::permissionsCommand, screencap::permissionsWindowFlag}, screencap::runPermissionsWindow},
        {{screencap::permissionsCommand}, screencap::openPermissionsWindow},
    };

    for (const auto& command: commands)
    {
        if (command.arguments == args)
            return command.run();
    }

    return std::nullopt;
}
} // namespace

int main(int argc, char** argv)
{
    const auto args = std::vector<std::string>(argv + 1, argv + argc);

    if (args == std::vector<std::string> {screencap::setupCommand})
        return runSetup(fs::path {argv[0]});

    if (const auto commandExitCode = runNamedCommand(args))
        return *commandExitCode;

    if (const auto childExitCode = screencap::runInUserDesktopSession(argv))
        return *childExitCode;

    const auto options = parseOptions(args);

    if (!options)
    {
        std::fputs(usage, stderr);
        return exitUsage;
    }

    if (options->isListRequested)
        return printCaptureTargets();

    if (options->recordingPath)
        return recordAndWrite(*options);

    return captureAndWrite(*options);
}
