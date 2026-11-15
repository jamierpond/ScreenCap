#include "VideoEncoder.h"

#include <windows.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

namespace screencap
{
namespace
{
constexpr auto hundredNanosecondsPerSecond = 10'000'000LL;
constexpr auto bitsPerSecondPerPixel = 4;

template <typename Com>
void release(Com*& pointer)
{
    if (pointer != nullptr)
        pointer->Release();

    pointer = nullptr;
}

IMFMediaType* createVideoType(const GUID& subtype, const VideoSettings& settings)
{
    IMFMediaType* type = nullptr;
    MFCreateMediaType(&type);
    type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    type->SetGUID(MF_MT_SUBTYPE, subtype);
    type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(type, MF_MT_FRAME_SIZE, static_cast<UINT32>(settings.width),
                       static_cast<UINT32>(settings.height));
    MFSetAttributeRatio(type, MF_MT_FRAME_RATE, static_cast<UINT32>(settings.framesPerSecond), 1);
    MFSetAttributeRatio(type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    return type;
}

class SinkWriterEncoder final : public VideoEncoder
{
public:
    SinkWriterEncoder(IMFSinkWriter* writerToUse, DWORD streamToUse, int framesPerSecondToUse)
        : writer(writerToUse)
        , stream(streamToUse)
        , frameDuration(hundredNanosecondsPerSecond / framesPerSecondToUse)
    {
    }

    ~SinkWriterEncoder() override
    {
        release(writer);
        MFShutdown();
    }

    SinkWriterEncoder(const SinkWriterEncoder&) = delete;
    SinkWriterEncoder& operator=(const SinkWriterEncoder&) = delete;

    bool appendFrame(const Image& frame, long long frameIndex) override
    {
        const auto byteCount = static_cast<DWORD>(frame.rgba.size());
        IMFMediaBuffer* buffer = nullptr;
        IMFSample* sample = nullptr;

        if (FAILED(MFCreateMemoryBuffer(byteCount, &buffer)))
            return false;

        copyRgbaToBgra(frame, buffer);
        buffer->SetCurrentLength(byteCount);
        MFCreateSample(&sample);
        sample->AddBuffer(buffer);
        sample->SetSampleTime(frameIndex * frameDuration);
        sample->SetSampleDuration(frameDuration);
        const auto isWritten = SUCCEEDED(writer->WriteSample(stream, sample));
        release(sample);
        release(buffer);
        return isWritten;
    }

    bool appendAudio(const std::vector<float>&) override { return false; }

    bool finish() override { return SUCCEEDED(writer->Finalize()); }

private:
    static void copyRgbaToBgra(const Image& frame, IMFMediaBuffer* buffer)
    {
        BYTE* destination = nullptr;
        buffer->Lock(&destination, nullptr, nullptr);

        for (auto offset = std::size_t {0}; offset < frame.rgba.size(); offset += 4)
        {
            destination[offset] = frame.rgba[offset + 2];
            destination[offset + 1] = frame.rgba[offset + 1];
            destination[offset + 2] = frame.rgba[offset];
            destination[offset + 3] = frame.rgba[offset + 3];
        }

        buffer->Unlock();
    }

    IMFSinkWriter* writer;
    DWORD stream;
    long long frameDuration;
};
} // namespace

VideoEncoderResult createVideoEncoder(const VideoSettings& settings)
{
    if (settings.audioFormat)
        return {nullptr, "recording audio is not wired up yet"};

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    if (FAILED(MFStartup(MF_VERSION)))
        return {nullptr, "Media Foundation is not available"};

    IMFSinkWriter* writer = nullptr;
    const auto path = settings.path.wstring();

    if (FAILED(MFCreateSinkWriterFromURL(path.c_str(), nullptr, nullptr, &writer)))
        return {nullptr, "cannot create " + settings.path.string()};

    auto* outputType = createVideoType(MFVideoFormat_H264, settings);
    const auto bitRate = settings.width * settings.height * bitsPerSecondPerPixel;
    outputType->SetUINT32(MF_MT_AVG_BITRATE, static_cast<UINT32>(bitRate));
    auto stream = DWORD {0};
    auto isReady = SUCCEEDED(writer->AddStream(outputType, &stream));
    release(outputType);

    auto* inputType = createVideoType(MFVideoFormat_RGB32, settings);
    const auto topDownStride = static_cast<UINT32>(settings.width * 4);
    inputType->SetUINT32(MF_MT_DEFAULT_STRIDE, topDownStride);
    isReady = isReady && SUCCEEDED(writer->SetInputMediaType(stream, inputType, nullptr));
    release(inputType);

    if (!isReady || FAILED(writer->BeginWriting()))
    {
        release(writer);
        return {nullptr, "Media Foundation refused the H.264 settings"};
    }

    return {std::make_unique<SinkWriterEncoder>(writer, stream, settings.framesPerSecond), {}};
}
} // namespace screencap
