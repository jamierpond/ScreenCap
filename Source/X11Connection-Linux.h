#pragma once

#include <X11/Xlib.h>

namespace screencap
{
class X11Connection
{
public:
    X11Connection();
    ~X11Connection();

    X11Connection(const X11Connection&) = delete;
    X11Connection& operator=(const X11Connection&) = delete;

    bool isOpen() const;

    Display* display;
};
} // namespace screencap
