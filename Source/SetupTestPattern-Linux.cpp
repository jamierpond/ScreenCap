#include "Setup.h"
#include "X11Connection-Linux.h"

#include <X11/Xutil.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>

namespace screencap
{
namespace
{
struct Rgb
{
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
};

struct NamedColour
{
    const char* name = "";
    Rgb rgb;
};

struct ScreenPoint
{
    int x = 0;
    int y = 0;
};

constexpr auto testPatternColours = std::array {
    NamedColour {"red", {255, 0, 0}},
    NamedColour {"green", {0, 255, 0}},
    NamedColour {"blue", {0, 0, 255}},
    NamedColour {"yellow", {255, 255, 0}},
};

constexpr auto blockSize = 120;
constexpr auto blockCount = static_cast<int>(testPatternColours.size());
constexpr auto requestedWindowPosition = ScreenPoint {100, 100};
constexpr auto channelTolerance = 8;
constexpr auto testScreen = setupTestDisplayNumber - 1;
constexpr auto windowTitle = "screencap setup test pattern";
constexpr auto bytesPerPixel = 4;

// X11 colour channels are 16 bits; 257 maps 0..255 onto 0..65535 exactly.
unsigned short widenChannelToSixteenBits(std::uint8_t channel)
{
    constexpr auto eightToSixteenBitScale = 257;
    const auto widened = channel * eightToSixteenBitScale;
    return static_cast<unsigned short>(widened);
}

unsigned long allocatePixel(Display* display, const Rgb& colour)
{
    auto xColour = XColor {};
    xColour.red = widenChannelToSixteenBits(colour.red);
    xColour.green = widenChannelToSixteenBits(colour.green);
    xColour.blue = widenChannelToSixteenBits(colour.blue);
    XAllocColor(display, DefaultColormap(display, testScreen), &xColour);
    return xColour.pixel;
}

ScreenPoint getBlockCentre(ScreenPoint windowPosition, int blockIndex)
{
    constexpr auto halfBlock = blockSize / 2;
    return {windowPosition.x + blockIndex * blockSize + halfBlock, windowPosition.y + halfBlock};
}

// Shown above everything, unmanaged (override-redirect) so openbox neither moves nor decorates it.
class TestPatternWindow
{
public:
    explicit TestPatternWindow(Display* displayToDrawOn)
        : display(displayToDrawOn)
        , window(createWindow(displayToDrawOn))
    {
        XStoreName(display, window, windowTitle);
        XMapRaised(display, window);
        waitUntilExposed();
        drawBlocks();
        XSync(display, False);
    }

    ~TestPatternWindow()
    {
        XDestroyWindow(display, window);
        XSync(display, False);
    }

    TestPatternWindow(const TestPatternWindow&) = delete;
    TestPatternWindow& operator=(const TestPatternWindow&) = delete;

    ScreenPoint getScreenPosition() const
    {
        auto position = ScreenPoint {};
        auto unusedChild = Window {};
        XTranslateCoordinates(display, window, RootWindow(display, testScreen), 0, 0, &position.x,
                              &position.y, &unusedChild);
        return position;
    }

private:
    static Window createWindow(Display* display)
    {
        constexpr auto windowWidth = static_cast<unsigned>(blockSize * blockCount);
        constexpr auto windowHeight = static_cast<unsigned>(blockSize);
        constexpr auto borderWidth = 0U;
        constexpr auto attributeMask = CWOverrideRedirect | CWBackPixel | CWEventMask;

        auto attributes = XSetWindowAttributes {};
        attributes.override_redirect = True;
        attributes.background_pixel = BlackPixel(display, testScreen);
        attributes.event_mask = ExposureMask;
        return XCreateWindow(display, RootWindow(display, testScreen), requestedWindowPosition.x,
                             requestedWindowPosition.y, windowWidth, windowHeight, borderWidth,
                             CopyFromParent, InputOutput, CopyFromParent, attributeMask, &attributes);
    }

    void waitUntilExposed()
    {
        auto event = XEvent {};
        XWindowEvent(display, window, ExposureMask, &event);
    }

    void drawBlocks()
    {
        auto* const graphicsContext = XCreateGC(display, window, 0, nullptr);

        for (auto index = 0; index < blockCount; ++index)
        {
            const auto& colour = testPatternColours.at(static_cast<std::size_t>(index));
            XSetForeground(display, graphicsContext, allocatePixel(display, colour.rgb));
            XFillRectangle(display, window, graphicsContext, index * blockSize, 0, blockSize, blockSize);
        }

        XFreeGC(display, graphicsContext);
    }

    Display* display;
    Window window;
};

bool isInsideImage(const Image& image, ScreenPoint point)
{
    const auto isInsideWidth = point.x >= 0 && point.x < image.width;
    const auto isInsideHeight = point.y >= 0 && point.y < image.height;
    return isInsideWidth && isInsideHeight;
}

std::optional<Rgb> readPixel(const Image& image, ScreenPoint point)
{
    if (!isInsideImage(image, point))
        return std::nullopt;

    const auto pixelIndex = static_cast<std::size_t>(point.y) * static_cast<std::size_t>(image.width)
                            + static_cast<std::size_t>(point.x);
    const auto byteIndex = pixelIndex * bytesPerPixel;
    return Rgb {image.rgba[byteIndex], image.rgba[byteIndex + 1], image.rgba[byteIndex + 2]};
}

bool isChannelClose(std::uint8_t found, std::uint8_t expected)
{
    return std::abs(found - expected) <= channelTolerance;
}

bool isSameColour(const Rgb& found, const Rgb& expected)
{
    return isChannelClose(found.red, expected.red) && isChannelClose(found.green, expected.green)
           && isChannelClose(found.blue, expected.blue);
}

std::string describeRgb(const Rgb& colour)
{
    return "rgb(" + std::to_string(colour.red) + ", " + std::to_string(colour.green) + ", "
           + std::to_string(colour.blue) + ")";
}

std::string describePoint(ScreenPoint point)
{
    return "(" + std::to_string(point.x) + ", " + std::to_string(point.y) + ")";
}

std::string describeTestPattern(ScreenPoint windowPosition)
{
    auto names = std::string {};

    for (const auto& colour: testPatternColours)
        names += std::string {names.empty() ? "" : ", "} + colour.name;

    return "a window of " + std::to_string(blockCount) + " blocks (" + names + "), each "
           + std::to_string(blockSize) + " pixels square, at " + describePoint(windowPosition)
           + " on display " + std::to_string(setupTestDisplayNumber);
}

// Empty when every block's centre pixel has the colour drawn there.
std::string findTestPatternMismatch(const Image& image, ScreenPoint windowPosition)
{
    auto mismatch = std::string {};

    for (auto index = 0; index < blockCount; ++index)
    {
        const auto& expected = testPatternColours.at(static_cast<std::size_t>(index));
        const auto centre = getBlockCentre(windowPosition, index);
        const auto found = readPixel(image, centre);

        if (!found)
            mismatch += "; " + describePoint(centre) + " is outside the capture";
        else if (!isSameColour(*found, expected.rgb))
            mismatch += "; at " + describePoint(centre) + " " + expected.name + " "
                        + describeRgb(expected.rgb) + " was drawn and the capture has "
                        + describeRgb(*found);
    }

    if (mismatch.empty())
        return {};

    return "capturing does not show what is on the screen: screencap drew "
           + describeTestPattern(windowPosition) + mismatch;
}
} // namespace

SetupTestCapture takeSetupTestCapture()
{
    const auto connection = X11Connection {};

    if (!connection.isOpen())
    {
        return {.capture = captureDisplay(setupTestDisplayNumber),
                .mismatch = "cannot open the X11 display to show a test pattern on it"};
    }

    auto capture = CaptureResult {};
    auto windowPosition = ScreenPoint {};

    {
        const auto window = TestPatternWindow {connection.display};
        windowPosition = window.getScreenPosition();
        capture = captureDisplay(setupTestDisplayNumber);
    }

    if (!capture.hasImage())
        return {.capture = std::move(capture), .mismatch = {}};

    auto mismatch = findTestPatternMismatch(capture.image, windowPosition);

    if (mismatch.empty())
    {
        std::printf("screencap setup: drew %s and found every block in the capture\n",
                    describeTestPattern(windowPosition).c_str());
    }

    return {.capture = std::move(capture), .mismatch = std::move(mismatch)};
}
} // namespace screencap
