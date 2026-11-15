#include "Capture.h"

#include <windows.h>

#include <dwmapi.h>

#include <optional>

namespace screencap
{
namespace
{
std::string convertToUtf8(const std::wstring& wide)
{
    if (wide.empty())
        return {};

    const auto wideLength = static_cast<int>(wide.size());
    const auto length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLength, nullptr, 0,
                                            nullptr, nullptr);
    auto text = std::string(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLength, text.data(), length, nullptr,
                        nullptr);
    return text;
}

void makeProcessDpiAware()
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

Image readBitmapPixels(HDC deviceContext, HBITMAP bitmap, int width, int height)
{
    constexpr auto bitsPerPixel = 32;
    constexpr auto bytesPerPixel = 4;
    constexpr auto opaque = std::uint8_t {255};

    auto header = BITMAPINFO {};
    header.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    header.bmiHeader.biWidth = width;
    header.bmiHeader.biHeight = -height;
    header.bmiHeader.biPlanes = 1;
    header.bmiHeader.biBitCount = bitsPerPixel;
    header.bmiHeader.biCompression = BI_RGB;

    auto image = Image {width, height, {}};
    image.rgba.resize(static_cast<std::size_t>(width * height * bytesPerPixel));
    const auto rowCount = static_cast<UINT>(height);
    GetDIBits(deviceContext, bitmap, 0, rowCount, image.rgba.data(), &header, DIB_RGB_COLORS);

    for (auto offset = std::size_t {0}; offset < image.rgba.size(); offset += bytesPerPixel)
    {
        std::swap(image.rgba[offset], image.rgba[offset + 2]);
        image.rgba[offset + 3] = opaque;
    }

    return image;
}

template <typename DrawInto>
CaptureResult captureWith(int width, int height, DrawInto drawInto)
{
    const auto screen = GetDC(nullptr);
    const auto memory = CreateCompatibleDC(screen);
    const auto bitmap = CreateCompatibleBitmap(screen, width, height);
    const auto previous = SelectObject(memory, bitmap);

    auto result = CaptureResult {};

    if (drawInto(memory, screen))
        result.image = readBitmapPixels(memory, bitmap, width, height);
    else
        result.error = "the desktop could not be copied (is the screen locked or the user logged out?)";

    SelectObject(memory, previous);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    return result;
}

std::vector<RECT> listMonitorRects()
{
    auto rects = std::vector<RECT> {};
    const auto collect = [](HMONITOR, HDC, LPRECT rect, LPARAM context) -> BOOL
    {
        reinterpret_cast<std::vector<RECT>*>(context)->push_back(*rect);
        return TRUE;
    };
    EnumDisplayMonitors(nullptr, nullptr, collect, reinterpret_cast<LPARAM>(&rects));
    return rects;
}

struct WindowInfo
{
    HWND handle {};
    std::string title;
    std::string appName;
    RECT bounds {};
};

std::string getProcessName(HWND window)
{
    auto processId = DWORD {0};
    GetWindowThreadProcessId(window, &processId);
    const auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);

    if (process == nullptr)
        return {};

    auto path = std::wstring(MAX_PATH, L'\0');
    auto length = static_cast<DWORD>(path.size());
    const auto hasPath = QueryFullProcessImageNameW(process, 0, path.data(), &length) != 0;
    CloseHandle(process);

    if (!hasPath)
        return {};

    path.resize(length);
    const auto nameStart = path.find_last_of(L"\\/") + 1;
    return convertToUtf8(path.substr(nameStart));
}

RECT getVisibleBounds(HWND window)
{
    auto bounds = RECT {};

    if (DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &bounds, sizeof(bounds)) != S_OK)
        GetWindowRect(window, &bounds);

    return bounds;
}

bool isCloakedWindow(HWND window)
{
    auto cloaked = DWORD {0};
    DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    return cloaked != 0;
}

bool isShownWindow(HWND window)
{
    return IsWindowVisible(window) && !isCloakedWindow(window);
}

std::vector<WindowInfo> listVisibleWindows()
{
    auto windows = std::vector<WindowInfo> {};
    const auto collect = [](HWND handle, LPARAM context) -> BOOL
    {
        const auto titleLength = GetWindowTextLengthW(handle);

        if (!isShownWindow(handle) || titleLength == 0)
            return TRUE;

        auto title = std::wstring(static_cast<std::size_t>(titleLength) + 1, L'\0');
        GetWindowTextW(handle, title.data(), titleLength + 1);
        title.resize(static_cast<std::size_t>(titleLength));

        auto* windows = reinterpret_cast<std::vector<WindowInfo>*>(context);
        windows->push_back({handle, convertToUtf8(title), getProcessName(handle),
                            getVisibleBounds(handle)});
        return TRUE;
    };
    EnumWindows(collect, reinterpret_cast<LPARAM>(&windows));
    return windows;
}

long getArea(const RECT& rect)
{
    return (rect.right - rect.left) * (rect.bottom - rect.top);
}

std::optional<WindowInfo> findLargestMatchingWindow(const std::string& titleOrAppName)
{
    auto largest = std::optional<WindowInfo> {};

    for (const auto& window: listVisibleWindows())
    {
        if (!isTitleOrAppNameMatch(window.title, window.appName, titleOrAppName))
            continue;

        if (!largest || getArea(window.bounds) > getArea(largest->bounds))
            largest = window;
    }

    return largest;
}

CaptureResult captureScreenRect(const RECT& rect)
{
    const auto width = rect.right - rect.left;
    const auto height = rect.bottom - rect.top;

    return captureWith(width, height,
                       [&](HDC memory, HDC screen)
                       {
                           return BitBlt(memory, 0, 0, width, height, screen, rect.left,
                                         rect.top, SRCCOPY | CAPTUREBLT)
                                  != 0;
                       });
}
CaptureResult captureWindowContent(HWND window)
{
    constexpr auto renderFullContent = UINT {2};

    auto rect = RECT {};
    GetWindowRect(window, &rect);

    return captureWith(rect.right - rect.left, rect.bottom - rect.top,
                       [&](HDC memory, HDC)
                       { return PrintWindow(window, memory, renderFullContent) != 0; });
}
} // namespace

CaptureResult captureDisplay(int displayNumber)
{
    makeProcessDpiAware();
    const auto monitors = listMonitorRects();

    if (displayNumber > static_cast<int>(monitors.size()))
        return {.error = "no display " + std::to_string(displayNumber) + " (screencap --list)"};

    return captureScreenRect(monitors[static_cast<std::size_t>(displayNumber - 1)]);
}

CaptureResult captureWindow(const std::string& titleOrAppName)
{
    makeProcessDpiAware();
    const auto window = findLargestMatchingWindow(titleOrAppName);

    if (!window)
        return {.error = "no window matches \"" + titleOrAppName + "\" (screencap --list)"};

    return captureWindowContent(window->handle);
}

std::vector<std::string> listCaptureTargets()
{
    makeProcessDpiAware();
    auto lines = std::vector<std::string> {};
    auto displayNumber = 1;

    for (const auto& rect: listMonitorRects())
    {
        lines.push_back("display " + std::to_string(displayNumber++) + "  "
                        + std::to_string(rect.right - rect.left) + "x"
                        + std::to_string(rect.bottom - rect.top));
    }

    for (const auto& window: listVisibleWindows())
        lines.push_back("window  " + window.appName + "  \"" + window.title + "\"");

    return lines;
}
} // namespace screencap
