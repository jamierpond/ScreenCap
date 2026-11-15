#include "Recorder.h"

#include "VideoEncoder.h"

#include <chrono>
#include <thread>

namespace screencap
{
namespace
{
using Clock = std::chrono::steady_clock;

// H.264 needs even dimensions.
int roundDownToEven(int size)
{
    return size - size % 2;
}

Image cropToSize(const Image& frame, int width, int height)
{
    constexpr auto bytesPerPixel = 4;

    auto cropped = Image {width, height, {}};
    cropped.rgba.resize(static_cast<std::size_t>(width * height * bytesPerPixel));
    const auto rowBytes = static_cast<std::size_t>(width * bytesPerPixel);
    const auto copyBytes = static_cast<std::size_t>(std::min(width, frame.width) * bytesPerPixel);

    for (auto row = 0; row < std::min(height, frame.height); ++row)
    {
        const auto sourceOffset = static_cast<std::size_t>(row * frame.width * bytesPerPixel);
        std::copy_n(frame.rgba.begin() + static_cast<long>(sourceOffset), copyBytes,
                    cropped.rgba.begin() + static_cast<long>(row * rowBytes));
    }

    return cropped;
}

long long countFramesDue(Clock::time_point start, int framesPerSecond)
{
    const auto elapsed = std::chrono::duration<double>(Clock::now() - start).count();
    return static_cast<long long>(elapsed * framesPerSecond) + 1;
}
} // namespace

RecordingResult record(const RecordingSettings& settings)
{
    auto firstCapture = settings.captureFrame();

    if (!firstCapture.hasImage())
        return {firstCapture.error, firstCapture.isPermissionDenied};

    const auto width = roundDownToEven(firstCapture.image.width);
    const auto height = roundDownToEven(firstCapture.image.height);
    auto audioFormat = std::optional<AudioFormat> {};

    if (settings.audioSource)
        audioFormat = settings.audioSource->get().getFormat();

    auto created = createVideoEncoder(
        {settings.path, width, height, settings.framesPerSecond, audioFormat});

    if (!created.encoder)
        return {created.error};

    const auto totalFrames = static_cast<long long>(settings.seconds * settings.framesPerSecond);
    const auto frameInterval = std::chrono::duration<double>(1.0 / settings.framesPerSecond);
    const auto start = Clock::now();
    auto frame = cropToSize(firstCapture.image, width, height);
    auto frameIndex = 0LL;

    while (frameIndex < totalFrames)
    {
        // A capture slower than the frame rate repeats the last frame, so the video keeps real time.
        const auto framesDue = std::min(countFramesDue(start, settings.framesPerSecond), totalFrames);

        for (; frameIndex < framesDue; ++frameIndex)
        {
            if (!created.encoder->appendFrame(frame, frameIndex))
                return {"the encoder refused frame " + std::to_string(frameIndex)};
        }

        if (settings.audioSource)
            created.encoder->appendAudio(settings.audioSource->get().takeAvailableSamples());

        std::this_thread::sleep_until(start + frameIndex * frameInterval);

        if (auto capture = settings.captureFrame(); capture.hasImage())
            frame = cropToSize(capture.image, width, height);
    }

    if (!created.encoder->finish())
        return {"the encoder could not finish " + settings.path.string()};

    return {};
}
} // namespace screencap
