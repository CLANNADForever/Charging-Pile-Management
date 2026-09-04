# Archive prompt

<!-- autoai:workflow-binding:v1
{"role":"archive","facts":["archive-wrapper-only","archive-fail-closed"]}
-->

Archive only the active change and only through `scripts/change_archive.sh`. The wrapper first cleans and checks the fixed local verification workspace, then rechecks tasks, strict validation, fingerprints, footprint, the exact surface report, unified Evaluation Pass and archive destination. It never deletes project tests, consumers or evidence paths. Do not refresh evidence during archive, call OpenSpec archive directly, or use `--no-validate`, `--skip-specs` or force. If the wrapper reports partial archive failure, preserve the scene and follow its recovery log; do not automatically retry, roll back or guess the archive directory.
