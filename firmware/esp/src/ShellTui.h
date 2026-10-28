#pragma once
#include "TelnetShell.h"
#include "ShellPixelCommand.h"
#include "ShellRemoteQuery.h"
#include "ShellToneCommand.h"
#include "ShellLabels.h"
#include "portable-shell/portable_device_view.hpp"
// Product snapshots and authorized action routing remain in ShellTuiBinding.h.
namespace PakarShell {
class ShellTui : public PortableShell::DeviceView {};
}
