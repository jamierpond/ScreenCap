#include "Permissions.h"

#include <cstdio>

namespace screencap
{
namespace
{
constexpr int exitOk = 0;
} // namespace

int removeAllPermissions()
{
    std::puts("nothing to remove: screencap holds no permissions on this platform");
    return exitOk;
}

int openPermissionsWindow()
{
    std::puts("no permissions: screencap needs none on this platform");
    return exitOk;
}

int runPermissionsWindow()
{
    return openPermissionsWindow();
}
} // namespace screencap
