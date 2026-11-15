#include "Capture.h"

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

constexpr auto usage = R"(usage: screencap [--display <n> | --window <title or app>] [--out <file.png>]
       screencap --list

Captures one display (default 1) or one window to a PNG and prints its path.
--window matches part of a window's title or its app's name, ignoring case;
the largest match wins. --list prints the displays and windows it can see.
Without --out the PNG goes to the system temp folder.

Exit codes: 0 ok, 1 failed, 2 screen capture not permitted (macOS), 64 usage.
)";

struct Options
{
    int displayNumber = 1;
    std::optional<std::string> windowMatch;
    std::optional<fs::path> outputPath;
    bool isListRequested = false;
};

std::optional<int> parseDisplayNumber(const std::string& text)
{
    try
    {
        auto parsedLength = std::size_t {0};
        const auto number = std::stoi(text, &parsedLength);

        if (parsedLength == text.size() && number >= 1)
            return number;
    }
    catch (...)
    {
    }

    return std::nullopt;
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

        const auto& value = args[++index];

        if (flag == "--display")
        {
            const auto number = parseDisplayNumber(value);

            if (!number)
                return std::nullopt;

            options.displayNumber = *number;
        }
        else if (flag == "--window")
            options.windowMatch = value;
        else if (flag == "--out" || flag == "-o")
            options.outputPath = value;
        else
            return std::nullopt;
    }

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

int captureAndWrite(const Options& options)
{
    const auto result = options.windowMatch ? screencap::captureWindow(*options.windowMatch)
                                            : screencap::captureDisplay(options.displayNumber);

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

    return captureAndWrite(*options);
}
