#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/harness_lock.sh"
[[ $# -eq 1 ]] || { echo "usage: $0 <kebab-name> | --clear" >&2; exit 2; }
arg=$1
if [[ "$arg" == --clear ]]; then harness_lock_acquire change-select ""; harness_require_no_archive_failure; current=$(harness_active_optional); [[ -z "$current" ]] || harness_require_isolation_authority "$current" change-select; harness_atomic_json_update ai_snapshot.json active_change=null phase=idle current_step=change-cleared next_step="Select or create a change"; echo "Active change cleared."; exit 0; fi
harness_validate_change_id "$arg"; harness_lock_acquire change-select "$arg"; harness_require_no_archive_failure; current=$(harness_active_optional); [[ -z "$current" || "$current" == "$arg" ]] || harness_require_isolation_authority "$current" change-select; harness_assert_change_harness "$arg" || { echo "[ERR] change is missing a safe Harness; use scripts/change_adopt.sh first: $arg" >&2; exit 4; }
harness_atomic_json_update ai_snapshot.json active_change="$arg" phase=planning current_step=change-selected next_step="Review OpenSpec status and instructions"; echo "Active change: $arg"
