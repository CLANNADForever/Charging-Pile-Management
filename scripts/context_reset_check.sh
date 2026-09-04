#!/usr/bin/env bash
set -euo pipefail
scripts/attribution_check.sh --quiet || { echo '[ERR] project attribution contract is invalid; context recovery is blocked' >&2; exit 6; }
required=(PROJECT_ATTRIBUTION.md CLAUDE.md AGENTS.md ai_snapshot.json .ai-harness/manifest.json openspec/config.yaml docs/ai/openspec.md scripts/openspec_cli.sh scripts/attribution_check.sh scripts/manifest_policy.js scripts/change_scope.js scripts/integration_surface_lib.js scripts/integration_surface_check.sh scripts/clang_ast_surface_adapter.js scripts/change_adopt.sh scripts/change_status.sh scripts/evaluator_check.sh scripts/verification_workspace.sh scripts/change_archive.sh scripts/archive_recover.sh); missing=0
for p in "${required[@]}"; do [[ -e "$p" ]] && echo "[OK] $p" || { echo "[MISS] $p"; missing=1; }; done; exit "$missing"
