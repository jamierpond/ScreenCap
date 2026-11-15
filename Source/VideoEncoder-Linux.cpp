#include "VideoEncoder.h"

#include <cstdio>

namespace screencap
{
namespace
{
std::string quoteForShell(const std::string& text)
{
    auto quoted = std::string {"'"};

    for (const auto character: text)
        quoted += character == '\'' ? std::string {"'\\''"} : std::string {character};

    return quoted + "'";
}

class FfmpegPipeEncoder final : public VideoEncoder
{
public:
    explicit FfmpegPipeEncoder(FILE* pipeToUse)
        : pipe(pipeToUse)
    {
    }

    ~FfmpegPipeEncoder() override
    {
        if (pipe != nullptr)
            pclose(pipe);
    }

    FfmpegPipeEncoder(const FfmpegPipeEncoder&) = delete;
    FfmpegPipeEncoder& operator=(const FfmpegPipeEncoder&) = delete;

    bool appendFrame(const Image& frame, long long) override
    {
        return std::fwrite(frame.rgba.data(), 1, frame.rgba.size(), pipe) == frame.rgba.size();
    }

    bool appendAudio(const std::vector<float>&) override { return false; }

    bool finish() override
    {
        const auto status = pclose(pipe);
        pipe = nullptr;
        return status == 0;
    }

private:
    FILE* pipe;
};
} // namespace

VideoEncoderResult createVideoEncoder(const VideoSettings& settings)
{
    if (settings.audioFormat)
        return {nullptr, "recording audio is not wired up yet"};

    const auto size = std::to_string(settings.width) + "x" + std::to_string(settings.height);
    const auto command = "ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgba -s " + size
                         + " -framerate " + std::to_string(settings.framesPerSecond)
                         + " -i - -c:v libx264 -pix_fmt yuv420p "
                         + quoteForShell(settings.path.string());

    auto* pipe = popen(command.c_str(), "w");

    if (pipe == nullptr)
        return {nullptr, "cannot start ffmpeg; recording on Linux needs ffmpeg on PATH"};

    return {std::make_unique<FfmpegPipeEncoder>(pipe), {}};
}
} // namespace screencap
