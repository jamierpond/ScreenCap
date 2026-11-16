#pragma once

namespace screencap
{
// What to do when Linux has no X11 display screencap can open.
constexpr auto virtualDesktopAdvice =
    "Run `screencap setup` once: it creates a permanent virtual X11 desktop on DISPLAY=:90 that "
    "starts at boot with nobody logged in, and screencap captures it from any ssh session. It is a "
    "separate screen in memory, not your physical monitor or your Wayland desktop: put apps on it "
    "with `DISPLAY=:90 some-app &`.";
} // namespace screencap
