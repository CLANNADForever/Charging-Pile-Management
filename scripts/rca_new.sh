#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/harness_lock.sh"; requested=${1:-}; [[ -z "$requested" ]] || harness_validate_change_id "$requested"; harness_lock_acquire rca "$requested"; harness_require_no_archive_failure; change=$(harness_resolve_change "$requested"); harness_lock_bind_change "$change"; file="openspec/changes/$change/harness/defect-rca.md"
relative="openspec/changes/$change/harness/defect-rca.md"; harness_assert_repo_path "$relative" file-or-missing || { echo '[ERR] unsafe RCA evidence path' >&2; exit 4; }
tmp=$(mktemp "openspec/changes/$change/harness/.defect-rca.XXXXXX") || exit 4; trap 'rm -f -- "$tmp"; harness_lock_release' EXIT
if [[ -f "$file" && ! -L "$file" ]]; then cp -- "$file" "$tmp"; before=$(sha256sum -- "$file" | awk '{print $1}'); else printf '# Defect RCA — %s\n\n' "$change" > "$tmp"; before=missing; fi
printf '## %s\n\n- Symptom:\n- Reproducer:\n- Root cause:\n- Direct fix:\n- Regression evidence:\n- Reusable Trigger/Check rule:\n\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$tmp"
if [[ "$before" == missing ]]; then [[ ! -e "$file" && ! -L "$file" ]] || { echo '[ERR] RCA path changed concurrently' >&2; exit 4; }; else [[ -f "$file" && ! -L "$file" && "$(sha256sum -- "$file" | awk '{print $1}')" == "$before" ]] || { echo '[ERR] RCA path changed concurrently' >&2; exit 4; }; fi
mv -f -- "$tmp" "$file"; trap harness_lock_release EXIT; echo "RCA entry added: $file"
