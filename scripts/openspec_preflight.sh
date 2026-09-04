#!/usr/bin/env bash
set -euo pipefail
top=$(git rev-parse --show-toplevel 2>/dev/null) || { echo "[ERR] Git repository required" >&2; exit 3; }
[[ "$(CDPATH= cd -- "$top" && pwd -P)" == "$(pwd -P)" ]] || { echo "[ERR] run from repository root" >&2; exit 3; }
for tool in node npm npx; do command -v "$tool" >/dev/null 2>&1 || { echo "[ERR] missing $tool" >&2; exit 3; }; done
node -e 'const p=process.versions.node.split(".").map(Number),m=[20,19,0];for(let i=0;i<3;i++){if(p[i]>m[i])process.exit(0);if(p[i]<m[i])process.exit(1)}' || { echo "[ERR] Node >=20.19.0 required" >&2; exit 3; }
[[ "$(scripts/openspec_cli.sh --version | tr -d '\r\n')" == 1.6.0 ]] || { echo "[ERR] OpenSpec runner must be 1.6.0" >&2; exit 3; }
[[ -f openspec/config.yaml ]] || { echo "[ERR] missing OpenSpec config" >&2; exit 4; }
schema=$(awk '
  /^[[:space:]]*#/ { next }
  /^[[:space:]]*schema[[:space:]]*:/ {
    count++
    value=$0
    sub(/^[^:]*:[[:space:]]*/, "", value)
    sub(/[[:space:]]+#.*$/, "", value)
    gsub(/^[[:space:]]+|[[:space:]]+$/, "", value)
    if ((substr(value,1,1)=="\"" && substr(value,length(value),1)=="\"") ||
        (substr(value,1,1)=="\047" && substr(value,length(value),1)=="\047")) {
      value=substr(value,2,length(value)-2)
    }
    found=value
  }
  END { if (count != 1) exit 2; print found }
' openspec/config.yaml) || { echo "[ERR] malformed OpenSpec config: schema must appear exactly once" >&2; exit 4; }
[[ "$schema" == spec-driven ]] || { echo "[ERR] unsupported schema: $schema" >&2; exit 4; }
if find .claude .codex -mindepth 1 \( -name 'openspec-*' -o -name 'opsx' -o -name 'opsx-*' \) -print -quit 2>/dev/null | grep -q .; then echo "[ERR] unsupported OpenSpec /opsx agent assets detected" >&2; exit 4; fi
[[ -f scripts/manifest_policy.js ]] || { echo "[ERR] missing manifest policy" >&2; exit 4; }
node scripts/manifest_policy.js >/dev/null || { echo "[ERR] managed-path manifest is invalid" >&2; exit 4; }
echo "OpenSpec preflight passed."
