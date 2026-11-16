#include "Setup.h"

#include <cstdio>

namespace screencap
{
namespace
{
constexpr int exitOk = 0;
} // namespace

int prepareMachineForCapture()
{
    std::puts("nothing to set up: on Windows screencap captures the logged-in user's desktop as it is");
    return exitOk;
}
} // namespace screencap
