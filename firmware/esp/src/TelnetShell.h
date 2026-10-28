#pragma once
#include "PortableShellCore.h"
// Canonical PortableShell C implementation, pinned by portable-shell/source.json.
// Compatibility names keep device handlers unchanged; no second state machine.
namespace PakarShell {
using PortableShell::terminalText;
class TelnetParser : public PortableShell::TelnetParser {};
class LineEditor : public PortableShell::LineEditor {};
}
