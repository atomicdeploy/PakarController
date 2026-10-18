#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Delegate to the SAME reviewed build owner and cache as Windows.
exec bash "$root/pakar.sh" build "$@"
