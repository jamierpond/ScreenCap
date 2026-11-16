#include "Setup.h"

#include "Permissions.h"

namespace screencap
{
int prepareMachineForCapture()
{
    return openPermissionsWindow();
}
} // namespace screencap
