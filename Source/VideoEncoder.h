#pragma once

#include "AudioSource.h"
#include "Capture.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace screencap
{
struct VideoSettings
{
    std::filesystem::path path;
    int width = 0;
    int height = 0;
    int framesPerSecond = 0;
    std::optional<AudioFormat> audioFormat;
};

// H.264 in an MP4: AVAssetWriter on macOS, Media Foundation's sink writer on
// Windows, an ffmpeg process on Linux. Frames arrive at a constant rate.
class VideoEncoder
{
public:
    virtual ~VideoEncoder() = default;
    virtual bool appendFrame(const Image& frame, long long frameIndex) = 0;
    virtual bool appendAudio(const std::vector<float>& interleavedSamples) = 0;
    virtual bool finish() = 0;
};

struct VideoEncoderResult
{
    std::unique_ptr<VideoEncoder> encoder;
    std::string error;
};

VideoEncoderResult createVideoEncoder(const VideoSettings& settings);
} // namespace screencap
