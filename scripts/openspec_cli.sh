#!/usr/bin/env bash
set -euo pipefail
OPENSPEC_PACKAGE="@fission-ai/openspec"
OPENSPEC_VERSION="1.6.0"
OPENSPEC_TELEMETRY=0 \
    npx --yes --package=@fission-ai/openspec@1.6.0 \
    -- openspec "$@"
