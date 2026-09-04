#!/usr/bin/env bash
set -euo pipefail
case "$#" in
    1) [[ "$1" == --check || "$1" == --digest ]] || {
        echo "usage: $0 --check [--json] | --digest" >&2
        exit 2
    } ;;
    2) [[ "$1" == --check && "$2" == --json ]] || {
        echo "usage: $0 --check [--json] | --digest" >&2
        exit 2
    } ;;
    *) echo "usage: $0 --check [--json] | --digest" >&2; exit 2 ;;
esac
exec node "$(dirname "$0")/project_profile_lib.js" "$@"
