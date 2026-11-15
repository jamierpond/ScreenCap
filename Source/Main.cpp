#include "Capture.h"
#include "Recorder.h"

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

Captures one display (default 1) or one window to a PNG and prints its path.
--window matches part of a window's title or its app's name, ignoring case;
the largest match wins. --list prints the displays and windows it can see.
Without --out the PNG goes to the system temp folder.
--record writes an H.264 MP4 (default 15 fps), video only for now. On Linux it
needs ffmpeg on PATH.

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
        return result.isPermissionDenied ? exitPermissionDenied : exitFailed;
    }

    std::printf("%s\n", path.string().c_str());
    return exitOk;
}

int captureAndWrite(const Options& options)
{
    const auto result = captureTarget(options);

    if (!result.hasImage())
    {
        std::fprintf(stderr, "screencap: %s\n", result.error.c_str());
        return result.isPermissionDenied ? exitPermissionDenied : exitFailed;
    }

    const auto path = fs::absolute(options.outputPath.value_or(createDefaultOutputPath()));

    if (!writePng(result.image, path))
    {
        std::fprintf(stderr, "screencap: cannot write %s\n", path.string().c_str());
        return exitFailed;
    }

    std::printf("%s\n", path.string().c_str());
    return exitOk;
}
} // namespace

int main(int argc, char** argv)
{
    const auto args = std::vector<std::string>(argv + 1, argv + argc);
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
