#include "Capture.h"

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <optional>

namespace screencap
{
namespace
{
constexpr auto permissionHint =
    "screen capture not permitted. Allow the app running screencap (your terminal, or sshd "
    "over ssh) in System Settings > Privacy & Security > Screen & System Audio Recording, "
    "then start it again";

struct ShareableContent
{
    SCShareableContent* content = nil;
    std::string error;
};

ShareableContent fetchShareableContent()
{
    __block auto fetched = ShareableContent {};
    const auto finished = dispatch_semaphore_create(0);

    [SCShareableContent getShareableContentExcludingDesktopWindows:YES
                                               onScreenWindowsOnly:YES
                                                 completionHandler:^(SCShareableContent* content,
                                                                     NSError* error) {
                                                   fetched.content = content;
                                                   if (error != nil)
                                                       fetched.error =
                                                           error.localizedDescription.UTF8String;
                                                   dispatch_semaphore_signal(finished);
                                                 }];

    dispatch_semaphore_wait(finished, DISPATCH_TIME_FOREVER);
    return fetched;
}

Image copyPixels(CGImageRef cgImage)
{
    constexpr auto bytesPerPixel = 4;
    constexpr auto bitsPerComponent = 8;

    auto image = Image {};
    image.width = static_cast<int>(CGImageGetWidth(cgImage));
    image.height = static_cast<int>(CGImageGetHeight(cgImage));
    image.rgba.resize(static_cast<std::size_t>(image.width * image.height * bytesPerPixel));

    const auto colourSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    const auto bitmapInfo = static_cast<uint32_t>(kCGImageAlphaPremultipliedLast)
                            | static_cast<uint32_t>(kCGBitmapByteOrder32Big);
    const auto context = CGBitmapContextCreate(image.rgba.data(), image.width, image.height,
                                               bitsPerComponent, image.width * bytesPerPixel,
                                               colourSpace, bitmapInfo);
    CGContextDrawImage(context, CGRectMake(0, 0, image.width, image.height), cgImage);
    CGContextRelease(context);
    CGColorSpaceRelease(colourSpace);
    return image;
}

CaptureResult captureFilter(SCContentFilter* filter)
{
    auto* configuration = [[SCStreamConfiguration alloc] init];
    const auto scale = filter.pointPixelScale;
    configuration.width = static_cast<size_t>(filter.contentRect.size.width * scale);
    configuration.height = static_cast<size_t>(filter.contentRect.size.height * scale);
    configuration.showsCursor = NO;

    __block auto result = CaptureResult {};
    const auto finished = dispatch_semaphore_create(0);

    [SCScreenshotManager captureImageWithFilter:filter
                                  configuration:configuration
                              completionHandler:^(CGImageRef cgImage, NSError* error) {
                                if (cgImage != nullptr)
                                    result.image = copyPixels(cgImage);
                                else
                                    result.error = error != nil
                                                       ? error.localizedDescription.UTF8String
                                                       : "capture returned no image";
                                dispatch_semaphore_signal(finished);
                              }];

    dispatch_semaphore_wait(finished, DISPATCH_TIME_FOREVER);
    return result;
}

std::optional<ShareableContent> fetchPermittedContent(CaptureResult& failure)
{
    if (!CGPreflightScreenCaptureAccess())
    {
        CGRequestScreenCaptureAccess();
        failure.error = permissionHint;
        failure.isPermissionDenied = true;
        return std::nullopt;
    }

    auto shareable = fetchShareableContent();

    if (shareable.content == nil)
    {
        failure.error = "cannot list displays and windows: " + shareable.error;
        return std::nullopt;
    }

    return shareable;
}

std::string getAppName(SCWindow* window)
{
    const auto* name = window.owningApplication.applicationName;
    return name != nil ? name.UTF8String : "";
}

std::string getTitle(SCWindow* window)
{
    return window.title != nil ? window.title.UTF8String : "";
}

bool isWindowMatch(SCWindow* window, const std::string& titleOrAppName)
{
    constexpr auto normalWindowLayer = 0;

    if (window.windowLayer != normalWindowLayer)
        return false;

    return containsIgnoringCase(getTitle(window), titleOrAppName)
           || containsIgnoringCase(getAppName(window), titleOrAppName);
}

SCWindow* findLargestMatchingWindow(SCShareableContent* content, const std::string& titleOrAppName)
{
    SCWindow* largest = nil;
    auto largestArea = 0.0;

    for (SCWindow* window in content.windows)
    {
        const auto area = window.frame.size.width * window.frame.size.height;

        if (isWindowMatch(window, titleOrAppName) && area > largestArea)
        {
            largest = window;
            largestArea = area;
        }
    }

    return largest;
}
} // namespace

CaptureResult captureDisplay(int displayNumber)
{
    @autoreleasepool
    {
        auto failure = CaptureResult {};
        const auto shareable = fetchPermittedContent(failure);

        if (!shareable)
            return failure;

        const auto* displays = shareable->content.displays;

        if (displayNumber > static_cast<int>(displays.count))
            return {.error = "no display " + std::to_string(displayNumber) + " (screencap --list)"};

        auto* display = displays[static_cast<NSUInteger>(displayNumber - 1)];
        auto* filter = [[SCContentFilter alloc] initWithDisplay:display excludingWindows:@[]];
        return captureFilter(filter);
    }
}

CaptureResult captureWindow(const std::string& titleOrAppName)
{
    @autoreleasepool
    {
        auto failure = CaptureResult {};
        const auto shareable = fetchPermittedContent(failure);

        if (!shareable)
            return failure;

        auto* window = findLargestMatchingWindow(shareable->content, titleOrAppName);

        if (window == nil)
            return {.error = "no window matches \"" + titleOrAppName + "\" (screencap --list)"};

        auto* filter = [[SCContentFilter alloc] initWithDesktopIndependentWindow:window];
        return captureFilter(filter);
    }
}

std::vector<std::string> listCaptureTargets()
{
    @autoreleasepool
    {
        auto failure = CaptureResult {};
        const auto shareable = fetchPermittedContent(failure);

        if (!shareable)
            return {"screencap: " + failure.error};

        auto lines = std::vector<std::string> {};
        auto displayNumber = 1;

        for (SCDisplay* display in shareable->content.displays)
        {
            lines.push_back("display " + std::to_string(displayNumber++) + "  "
                            + std::to_string(display.width) + "x"
                            + std::to_string(display.height));
        }

        for (SCWindow* window in shareable->content.windows)
        {
            if (window.windowLayer == 0 && !getTitle(window).empty())
                lines.push_back("window  " + getAppName(window) + "  \"" + getTitle(window) + "\"");
        }

        return lines;
    }
}
} // namespace screencap
