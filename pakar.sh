#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
command -v node >/dev/null || { printf '[ERROR] Node.js20.19 or newer is required.\n' >&2; exit 3; }
# Actual PU build.sh scoped repository cwd also handles WSL/Git-Bash calling
# Windows node.exe: a /mnt/c absolute path is not a native Windows filename.
cd "$root"
exec node tools/pakar.mjs "$@"
