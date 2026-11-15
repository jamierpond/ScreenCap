#include "Capture.h"

#include <algorithm>
#include <cctype>

namespace screencap
{
bool containsIgnoringCase(const std::string& text, const std::string& part)
{
    const auto isSameLetter = [](char left, char right)
    {
        const auto leftLetter = static_cast<unsigned char>(left);
        const auto rightLetter = static_cast<unsigned char>(right);
        return std::tolower(leftLetter) == std::tolower(rightLetter);
    };

    return std::search(text.begin(), text.end(), part.begin(), part.end(), isSameLetter)
           != text.end();
}

bool isTitleOrAppNameMatch(const std::string& title,
                           const std::string& appName,
                           const std::string& titleOrAppName)
{
    return containsIgnoringCase(title, titleOrAppName)
           || containsIgnoringCase(appName, titleOrAppName);
}
} // namespace screencap
