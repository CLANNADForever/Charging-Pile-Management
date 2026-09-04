#!/usr/bin/env bash
set -euo pipefail
echo "Project root: $(pwd)"; scripts/openspec_preflight.sh; scripts/context_reset_check.sh; scripts/resume_from_snapshot.sh
