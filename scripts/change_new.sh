#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/harness_lock.sh"
switch=0; [[ $# -ge 1 && $# -le 2 ]] || { echo "usage: $0 <kebab-name> [--switch]" >&2; exit 2; }; change=$1; harness_validate_change_id "$change"
if [[ $# -eq 2 ]]; then [[ "$2" == --switch ]] || exit 2; switch=1; fi
harness_lock_acquire change-new "$change"; harness_require_no_archive_failure; harness_assert_changes_root || { echo '[ERR] unsafe OpenSpec changes tree' >&2; exit 4; }; active=$(harness_active_optional); [[ -z "$active" || "$active" == "$change" ]] || harness_require_isolation_authority "$active" change-new; [[ -z "$active" || "$switch" -eq 1 ]] || { echo "[ERR] '$active' is active; pass --switch explicitly" >&2; exit 4; }; [[ ! -e "openspec/changes/$change" && ! -L "openspec/changes/$change" ]] || { echo "[ERR] change already exists" >&2; exit 4; }
new_json=$(mktemp "${TMPDIR:-/tmp}/autoai-new-change.XXXXXX"); trap 'rm -f -- "$new_json"; harness_lock_release' EXIT
scripts/openspec_cli.sh new change "$change" --json > "$new_json" || { echo '[ERR] OpenSpec failed to create change' >&2; exit 6; }
node - "$new_json" "$change" <<'NODE' || { echo '[ERR] OpenSpec new-change JSON or filesystem contract mismatch; preserving the new directory for inspection' >&2; exit 6; }
const fs=require('fs'),path=require('path');const [file,id]=process.argv.slice(2),root=process.cwd(),v=JSON.parse(fs.readFileSync(file)),c=v?.change,target=path.resolve(root,'openspec','changes',id);if(!c||typeof c!=='object'||c.id!==id||c.schema!=='spec-driven'||typeof c.path!=='string'||typeof c.metadataPath!=='string'||!path.isAbsolute(c.path)||!path.isAbsolute(c.metadataPath)||path.resolve(c.path)!==target||path.resolve(c.metadataPath)!==path.join(target,'.openspec.yaml'))process.exit(1);const st=fs.lstatSync(target),mt=fs.lstatSync(path.join(target,'.openspec.yaml')),entries=fs.readdirSync(target);if(!st.isDirectory()||st.isSymbolicLink()||!mt.isFile()||mt.isSymbolicLink()||entries.length!==1||entries[0]!=='.openspec.yaml')process.exit(1);
NODE
mkdir "openspec/changes/$change/harness"
tmp="openspec/changes/$change/harness/ai_snapshot.json.tmp.$$"; cat > "$tmp" <<JSON
{
  "schema_version": 4,
  "phase": "planning",
  "planned_base_specs_fingerprint": null,
  "planned_change_fingerprint": null,
  "planned_tdd_policy_sha256": null,
  "planned_integration_completeness_sha256": null,
  "planning_approved_at": null,
  "implementation_base_commit": null,
  "adopted_preexisting_paths": [],
  "implementation_baselined_at": null,
  "current_step": "complete planning artifacts",
  "next_step": "strict validate and obtain human review"
}
JSON
mv "$tmp" "openspec/changes/$change/harness/ai_snapshot.json"
printf '{\n  "schema_version": 3,\n  "change_name": "%s",\n  "migration": null,\n  "tasks": []\n}\n' "$change" > "openspec/changes/$change/harness/verification.json"
printf '# Verification — %s\n\n' "$change" > "openspec/changes/$change/harness/verification.md"; printf '# Evaluation history — %s\n\n' "$change" > "openspec/changes/$change/harness/evaluation.md"; printf '# Defect RCA — %s\n\n' "$change" > "openspec/changes/$change/harness/defect-rca.md"
if [[ -z "$active" || "$switch" -eq 1 ]]; then harness_atomic_json_update ai_snapshot.json active_change="$change" phase=planning current_step=change-created next_step="Write and review planning artifacts"; fi
echo "Created change: $change"
