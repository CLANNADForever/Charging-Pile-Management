#!/usr/bin/env bash
set -euo pipefail
threshold=${QUICK_BRIEF_MIN_LINES:-80}
while IFS= read -r -d '' f; do lines=$(wc -l < "$f"); ((lines>threshold)) || continue; head -n 20 "$f" | grep -Eq '(^|[[:space:]])quick_brief:' && echo "[OK] $f" || echo "[WARN] $f lacks quick_brief"; done < <(find . -type f -name '*.md' \( -path './docs/ai/*' -o -path './CLAUDE.md' -o -path './AGENTS.md' \) -print0)
echo 'OpenSpec artifacts, archive and migration backups are excluded.'
