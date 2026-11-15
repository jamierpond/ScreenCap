#include "Capture.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <algorithm>
#include <optional>

namespace screencap
{
namespace
{
constexpr auto noX11Message =
    "cannot open the X11 display ($DISPLAY). On a Wayland session without XWayland, "
    "screencap can't capture: Wayland only allows it through the desktop portal, which asks "
    "the user every time. Log in to an X11 session, or run under Xvfb";

class X11Connection
{
public:
    X11Connection()
        : display(XOpenDisplay(nullptr))
    {
    }

    ~X11Connection()
    {
        if (display != nullptr)
            XCloseDisplay(display);
    }

    X11Connection(const X11Connection&) = delete;
    X11Connection& operator=(const X11Connection&) = delete;

    bool isOpen() const { return display != nullptr; }

    Display* display;
};

struct ScreenRect
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct WindowInfo
{
    Window handle {};
    std::string title;
    std::string appName;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

std::uint8_t readChannel(unsigned long pixel, unsigned long mask)
{
    constexpr auto channelMaximum = 255UL;

    if (mask == 0)
        return 0;

    auto shift = 0;

    while (((mask >> shift) & 1UL) == 0)
        ++shift;

    const auto maximum = mask >> shift;
    const auto value = (pixel & mask) >> shift;
    return static_cast<std::uint8_t>(value * channelMaximum / maximum);
}

CaptureResult captureRootRect(Display* display, int screen, const ScreenRect& rect)
{
    const auto width = rect.width;
    const auto height = rect.height;
    constexpr auto bytesPerPixel = 4;
    constexpr auto opaque = std::uint8_t {255};

    const auto root = RootWindow(display, screen);
    auto* ximage = XGetImage(display, root, rect.x, rect.y, static_cast<unsigned>(width),
                             static_cast<unsigned>(height), AllPlanes, ZPixmap);

    if (ximage == nullptr)
        return {.error = "XGetImage failed"};

    auto image = Image {width, height, {}};
    image.rgba.resize(static_cast<std::size_t>(width * height * bytesPerPixel));
    auto* out = image.rgba.data();

    for (auto row = 0; row < height; ++row)
    {
        for (auto column = 0; column < width; ++column)
        {
            const auto pixel = XGetPixel(ximage, column, row);
            *out++ = readChannel(pixel, ximage->red_mask);
            *out++ = readChannel(pixel, ximage->green_mask);
            *out++ = readChannel(pixel, ximage->blue_mask);
            *out++ = opaque;
        }
    }

    XDestroyImage(ximage);
    return {.image = std::move(image)};
}

std::string fetchTitle(Display* display, Window window)
{
    char* name = nullptr;

    if (XFetchName(display, window, &name) == 0 || name == nullptr)
        return {};

    auto title = std::string {name};
    XFree(name);
    return title;
}

std::string fetchAppName(Display* display, Window window)
{
    auto hint = XClassHint {};

    if (XGetClassHint(display, window, &hint) == 0)
        return {};

    auto appName = std::string {hint.res_class != nullptr ? hint.res_class : ""};
    XFree(hint.res_name);
    XFree(hint.res_class);
    return appName;
}

void collectNamedWindows(Display* display, Window parent, std::vector<WindowInfo>& windows)
{
    auto root = Window {};
    auto parentOfParent = Window {};
    Window* children = nullptr;
    auto childCount = 0U;

    if (XQueryTree(display, parent, &root, &parentOfParent, &children, &childCount) == 0)
        return;

    for (auto index = 0U; index < childCount; ++index)
    {
        const auto child = children[index];
        auto attributes = XWindowAttributes {};
        XGetWindowAttributes(display, child, &attributes);

        if (attributes.map_state != IsViewable)
            continue;

        auto title = fetchTitle(display, child);

        if (title.empty())
        {
            collectNamedWindows(display, child, windows);
            continue;
        }

        auto x = 0;
        auto y = 0;
        auto unusedChild = Window {};
        XTranslateCoordinates(display, child, root, 0, 0, &x, &y, &unusedChild);
        windows.push_back({child, std::move(title), fetchAppName(display, child), x, y,
                           attributes.width, attributes.height});
    }

    if (children != nullptr)
        XFree(children);
}

std::vector<WindowInfo> listNamedWindows(Display* display)
{
    auto windows = std::vector<WindowInfo> {};
    collectNamedWindows(display, DefaultRootWindow(display), windows);
    return windows;
}

std::optional<WindowInfo> findLargestMatchingWindow(Display* display,
                                                    const std::string& titleOrAppName)
{
    auto largest = std::optional<WindowInfo> {};

    for (const auto& window: listNamedWindows(display))
    {
        if (!isTitleOrAppNameMatch(window.title, window.appName, titleOrAppName))
            continue;

        if (!largest || window.width * window.height > largest->width * largest->height)
            largest = window;
    }

    return largest;
}

int clampToScreen(int value, int limit)
{
    return std::clamp(value, 0, limit);
}
} // namespace

CaptureResult captureDisplay(int displayNumber)
{
    const auto connection = X11Connection {};

    if (!connection.isOpen())
        return {.error = noX11Message};

    if (displayNumber > ScreenCount(connection.display))
        return {.error = "no display " + std::to_string(displayNumber) + " (screencap --list)"};

    const auto screen = displayNumber - 1;
    const auto wholeScreen = ScreenRect {0, 0, DisplayWidth(connection.display, screen),
                                         DisplayHeight(connection.display, screen)};
    return captureRootRect(connection.display, screen, wholeScreen);
}

CaptureResult captureWindow(const std::string& titleOrAppName)
{
    const auto connection = X11Connection {};

    if (!connection.isOpen())
        return {.error = noX11Message};

    const auto window = findLargestMatchingWindow(connection.display, titleOrAppName);

    if (!window)
        return {.error = "no window matches \"" + titleOrAppName + "\" (screencap --list)"};

    const auto screen = DefaultScreen(connection.display);
    const auto screenWidth = DisplayWidth(connection.display, screen);
    const auto screenHeight = DisplayHeight(connection.display, screen);
    const auto left = clampToScreen(window->x, screenWidth);
    const auto top = clampToScreen(window->y, screenHeight);
    const auto right = clampToScreen(window->x + window->width, screenWidth);
    const auto bottom = clampToScreen(window->y + window->height, screenHeight);

    if (right <= left || bottom <= top)
        return {.error = "window \"" + window->title + "\" is off screen"};

    return captureRootRect(connection.display, screen, {left, top, right - left, bottom - top});
}

std::vector<std::string> listCaptureTargets()
{
    const auto connection = X11Connection {};

    if (!connection.isOpen())
        return {std::string {"screencap: "} + noX11Message};

    auto lines = std::vector<std::string> {};

    for (auto screen = 0; screen < ScreenCount(connection.display); ++screen)
    {
        lines.push_back("display " + std::to_string(screen + 1) + "  "
                        + std::to_string(DisplayWidth(connection.display, screen)) + "x"
                        + std::to_string(DisplayHeight(connection.display, screen)));
    }

    for (const auto& window: listNamedWindows(connection.display))
        lines.push_back("window  " + window.appName + "  \"" + window.title + "\"");

    return lines;
}
} // namespace screencap
