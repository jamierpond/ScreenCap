#pragma once

#include "AudioSource.h"
#include "Capture.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace screencap
{
struct RecordingSettings
{
    std::filesystem::path path;
    double seconds = 0.0;
    int framesPerSecond = 0;
    std::function<CaptureResult()> captureFrame;
    std::optional<std::reference_wrapper<AudioSource>> audioSource;
};

struct RecordingResult
{
    std::string error;
    bool isPermissionDenied = false;
};

RecordingResult record(const RecordingSettings& settings);
} // namespace screencap
