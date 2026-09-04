#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/harness_lock.sh"

change= action= as_json=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --plan-check|--refresh|--check) [[ -z "$action" ]] || { echo '[ERR] exactly one action is required' >&2; exit 2; }; action=$1 ;;
    --json) as_json=1 ;;
    *) [[ -z "$change" ]] || { echo '[ERR] unexpected argument' >&2; exit 2; }; change=$1 ;;
  esac
  shift
done
[[ -n "$action" ]] || { echo "usage: $0 [<change>] --plan-check|--refresh|--check [--json]" >&2; exit 2; }
[[ -z "$change" ]] || harness_validate_change_id "$change"
case "$action" in --plan-check) purpose=integration-plan-check ;; --refresh) purpose=integration-refresh ;; --check) purpose=integration-check ;; esac
harness_lock_acquire "$purpose" "$change"
[[ "$action" == --check || "$action" == --plan-check ]] || harness_require_no_archive_failure
if [[ "$action" == --refresh ]]; then
  change=$(harness_resolve_change "$change")
elif [[ -n "$change" ]]; then
  harness_validate_change_id "$change"
else
  change=$(harness_resolve_change)
fi
harness_lock_bind_change "$change"
base="openspec/changes/$change"; harness="$base/harness"; report="$harness/integration-surface-report.json"
[[ -d "$base" && ! -L "$base" && -d "$harness" && ! -L "$harness" ]] || { echo '[ERR] change or harness is stale' >&2; exit 4; }
if [[ "$action" == --refresh ]]; then
  harness_verification_workspace_control cleanup "$change" || { echo '[ERR] temporary verification program cleanup failed before final surface inventory' >&2; exit 6; }
  harness_verification_workspace_control assert-clean "$change" || { echo '[ERR] temporary verification workspace is not clean' >&2; exit 6; }
elif [[ "$action" == --check ]]; then
  harness_verification_workspace_control assert-clean "$change" || { echo '[ERR] temporary verification workspace is not clean' >&2; exit 6; }
fi

emit_diagnostic() {
  local status=$1 reason=$2
  node - "$change" "$status" "$reason" "$as_json" <<'NODE'
const [change,status,reason,asJson]=process.argv.slice(2),x={schema_version:1,change_name:change,status,reason};if(asJson==='1')process.stdout.write(JSON.stringify(x,null,2)+'\n');else process.stdout.write(`${status}: ${reason}\n`);
NODE
}

if [[ "$action" == --plan-check ]]; then
  tmp=$(mktemp "${TMPDIR:-/tmp}/autoai-integration-plan.XXXXXX"); trap 'rm -f -- "$tmp"; harness_lock_release' EXIT
  if ! node - "$change" "$tmp" <<'NODE'
const fs=require('fs'),[change,out]=process.argv.slice(2);try{const plan=require(process.cwd()+'/scripts/integration_surface_lib.js').parsePlan(process.cwd(),change);fs.writeFileSync(out,JSON.stringify({schema_version:1,change_name:change,mode:'plan-check',status:'valid',planning_block_sha256:plan.block_sha256,planned_surface_ids:plan.block.surfaces.map(x=>x.id)},null,2)+'\n')}catch(e){console.error(e.message);process.exit(6)}
NODE
  then reason=$(tail -n 1 "$tmp" 2>/dev/null || true); [[ -n "$reason" ]] || reason='integration_planning_invalid'; emit_diagnostic invalid "$reason"; exit 6; fi
  if [[ "$as_json" -eq 1 ]]; then cat "$tmp"; else echo 'valid'; fi
  exit 0
fi

instructions=$(mktemp "${TMPDIR:-/tmp}/autoai-integration-instructions.XXXXXX")
computed=$(mktemp "${TMPDIR:-/tmp}/autoai-integration-report.XXXXXX")
error_file=$(mktemp "${TMPDIR:-/tmp}/autoai-integration-error.XXXXXX")
trap 'rm -f -- "$instructions" "$computed" "$error_file"; harness_lock_release' EXIT
if [[ "$action" == --refresh && -f "$harness/evaluation-baseline.json" ]] && [[ "$(node -p "JSON.parse(require('fs').readFileSync(process.argv[1])).status||''" "$harness/evaluation-baseline.json" 2>/dev/null || true)" == in_progress ]]; then emit_diagnostic blocked evaluation_in_progress; exit 6; fi
scripts/openspec_cli.sh instructions apply --change "$change" --json > "$instructions" || { emit_diagnostic blocked openspec_instructions_failed; exit 6; }
token=${AUTOAI_LOCK_TOKEN:-$HARNESS_OWNED_TOKEN}; parent=${AUTOAI_PARENT_PURPOSE:-$purpose}
AUTOAI_LOCK_TOKEN="$token" AUTOAI_PARENT_PURPOSE="$parent" scripts/change_footprint.sh "$change" --check --json >/dev/null || { emit_diagnostic blocked stale_change_footprint; exit 6; }
if ! node - "$change" "$instructions" "$computed" "$error_file" <<'NODE'
const fs=require('fs'),[change,instructions,out,errorFile]=process.argv.slice(2),lib=require(process.cwd()+'/scripts/integration_surface_lib.js');try{const report=lib.buildReviewedReport(process.cwd(),change,instructions);fs.writeFileSync(out,lib.reportBytes(report))}catch(e){fs.writeFileSync(errorFile,JSON.stringify({status:e.gateStatus||'invalid',reason:e.message}));process.exit(6)}
NODE
then
  status=$(node -p "JSON.parse(require('fs').readFileSync(process.argv[1])).status" "$error_file" 2>/dev/null || echo invalid)
  reason=$(node -p "JSON.parse(require('fs').readFileSync(process.argv[1])).reason" "$error_file" 2>/dev/null || echo integration_report_invalid)
  [[ "$status" == blocked || "$status" == invalid ]] || status=invalid
  emit_diagnostic "$status" "$reason"; exit 6
fi
if [[ "$action" == --check ]]; then
  if [[ ! -f "$report" || -L "$report" ]] || ! cmp -s "$computed" "$report"; then emit_diagnostic blocked stale_integration_surface_report; exit 6; fi
else
  tmp_report=$(mktemp "$harness/.integration-surface-report.XXXXXX")
  cp -- "$computed" "$tmp_report"; chmod 644 "$tmp_report"; mv -f -- "$tmp_report" "$report"
fi
status=$(node -p "JSON.parse(require('fs').readFileSync(process.argv[1])).status" "$computed")
if [[ "$as_json" -eq 1 ]]; then cat "$computed"; else echo "$status"; fi
[[ "$status" != orphaned ]] || exit 6
