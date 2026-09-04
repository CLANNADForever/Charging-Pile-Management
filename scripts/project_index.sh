#!/usr/bin/env bash
set -euo pipefail
exec node "$(dirname "$0")/project_index.js" "$@"
