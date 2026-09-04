#!/usr/bin/env bash
set -euo pipefail
echo '# Heuristic debt scan'; rg -n --glob '*.{cc,cpp,cxx,h,hpp,cmake}' --glob CMakeLists.txt 'TODO|FIXME|HACK' . || true; echo 'Review manually; this command does not edit OpenSpec artifacts.'
