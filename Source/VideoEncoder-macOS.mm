#include "VideoEncoder.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>

#include <thread>

namespace screencap
{
namespace
{
void copyRgbaToBgra(const Image& frame, CVPixelBufferRef pixelBuffer)
{
    constexpr auto bytesPerPixel = 4;

    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    auto* base = static_cast<std::uint8_t*>(CVPixelBufferGetBaseAddress(pixelBuffer));
    const auto rowBytes = CVPixelBufferGetBytesPerRow(pixelBuffer);

    for (auto row = 0; row < frame.height; ++row)
    {
        const auto* source = frame.rgba.data() + row * frame.width * bytesPerPixel;
        auto* destination = base + static_cast<std::size_t>(row) * rowBytes;

        for (auto column = 0; column < frame.width; ++column, source += 4, destination += 4)
        {
            destination[0] = source[2];
            destination[1] = source[1];
            destination[2] = source[0];
            destination[3] = source[3];
        }
    }

    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
}

class AssetWriterEncoder final : public VideoEncoder
{
public:
    AssetWriterEncoder(AVAssetWriter* writerToUse,
                       AVAssetWriterInput* inputToUse,
                       AVAssetWriterInputPixelBufferAdaptor* adaptorToUse,
                       int framesPerSecondToUse)
        : writer(writerToUse)
        , input(inputToUse)
        , adaptor(adaptorToUse)
        , framesPerSecond(framesPerSecondToUse)
    {
    }

    bool appendFrame(const Image& frame, long long frameIndex) override
    {
        constexpr auto readyPollInterval = std::chrono::milliseconds {2};

        while (!input.readyForMoreMediaData)
            std::this_thread::sleep_for(readyPollInterval);

        CVPixelBufferRef pixelBuffer = nullptr;
        CVPixelBufferPoolCreatePixelBuffer(nullptr, adaptor.pixelBufferPool, &pixelBuffer);

        if (pixelBuffer == nullptr)
            return false;

        copyRgbaToBgra(frame, pixelBuffer);
        const auto time = CMTimeMake(frameIndex, framesPerSecond);
        const auto isAppended = [adaptor appendPixelBuffer:pixelBuffer withPresentationTime:time];
        CVPixelBufferRelease(pixelBuffer);
        return isAppended;
    }

    bool appendAudio(const std::vector<float>&) override { return false; }

    bool finish() override
    {
        [input markAsFinished];
        const auto finished = dispatch_semaphore_create(0);
        [writer finishWritingWithCompletionHandler:^{
          dispatch_semaphore_signal(finished);
        }];
        dispatch_semaphore_wait(finished, DISPATCH_TIME_FOREVER);
        return writer.status == AVAssetWriterStatusCompleted;
    }

private:
    AVAssetWriter* writer;
    AVAssetWriterInput* input;
    AVAssetWriterInputPixelBufferAdaptor* adaptor;
    int framesPerSecond;
};
} // namespace

VideoEncoderResult createVideoEncoder(const VideoSettings& settings)
{
    if (settings.audioFormat)
        return {nullptr, "recording audio is not wired up yet"};

    std::filesystem::remove(settings.path);
    auto* url = [NSURL fileURLWithPath:@(settings.path.string().c_str())];
    NSError* error = nil;
    auto* writer = [[AVAssetWriter alloc] initWithURL:url fileType:AVFileTypeMPEG4 error:&error];

    if (writer == nil)
        return {nullptr, error.localizedDescription.UTF8String};

    auto* videoSettings = @{
        AVVideoCodecKey: AVVideoCodecTypeH264,
        AVVideoWidthKey: @(settings.width),
        AVVideoHeightKey: @(settings.height),
    };
    auto* input = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo
                                                     outputSettings:videoSettings];
    input.expectsMediaDataInRealTime = YES;

    auto* bufferAttributes = @{
        (NSString*) kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
        (NSString*) kCVPixelBufferWidthKey: @(settings.width),
        (NSString*) kCVPixelBufferHeightKey: @(settings.height),
    };
    auto* adaptor = [AVAssetWriterInputPixelBufferAdaptor
        assetWriterInputPixelBufferAdaptorWithAssetWriterInput:input
                                   sourcePixelBufferAttributes:bufferAttributes];

    [writer addInput:input];

    if (![writer startWriting])
        return {nullptr, writer.error.localizedDescription.UTF8String};

    [writer startSessionAtSourceTime:kCMTimeZero];
    return {std::make_unique<AssetWriterEncoder>(writer, input, adaptor, settings.framesPerSecond),
            {}};
}
} // namespace screencap
