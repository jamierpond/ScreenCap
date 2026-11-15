#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace screencap
{
struct Image
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

struct CaptureResult
{
    Image image;
    std::string error;
    bool isPermissionDenied = false;

    bool hasImage() const { return error.empty() && image.width > 0 && image.height > 0; }
};

// Displays are numbered from 1, in the order listCaptureTargets prints them.
CaptureResult captureDisplay(int displayNumber);

// Matches a case-insensitive substring of a window's title or its app's name;
// the largest matching window wins.
CaptureResult captureWindow(const std::string& titleOrAppName);

std::vector<std::string> listCaptureTargets();

bool containsIgnoringCase(const std::string& text, const std::string& part);
} // namespace screencap
