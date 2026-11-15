#pragma once

#include <vector>

namespace screencap
{
struct AudioFormat
{
    int sampleRate = 48000;
    int channelCount = 2;
};

// Where system audio plugs into a recording (MakeASound's capture, once it has one).
// Samples are interleaved 32-bit float; the recorder drains them once per video frame.
class AudioSource
{
public:
    virtual ~AudioSource() = default;
    virtual AudioFormat getFormat() const = 0;
    virtual std::vector<float> takeAvailableSamples() = 0;
};
} // namespace screencap
