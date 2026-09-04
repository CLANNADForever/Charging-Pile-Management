#!/usr/bin/env bash
set -euo pipefail
case "$#" in 0) ;; 1) [[ "$1" == --json ]] || exit 2 ;; *) exit 2 ;; esac
exec node "$(dirname "$0")/workflow_contract_check.js" "$@"
