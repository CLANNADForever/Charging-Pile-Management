# OpenSpec in this Harness

OpenSpec versions the project intent as `proposal -> delta specs/design -> tasks -> archive`. It is the canonical planning data plane. AutoAI supplies the control and acceptance plane: active selection, role prompts, real-command verification, fingerprints, implementation economy, independent Evaluation, RCA and archive gates.

Supported scope is intentionally narrow: OpenSpec 1.6.0, official `spec-driven` schema and core profile. This Harness does not install `/opsx` skills, alter global Codex/OpenSpec settings, define custom schemas, use Stores/sync, or bulk archive.

Use:

```text
scripts/change_new.sh <kebab-name> [--switch]
scripts/change_adopt.sh <kebab-name> [--switch]
scripts/change_select.sh <kebab-name> | --clear
scripts/change_status.sh [<kebab-name>] [--json]
scripts/snapshot_update.sh --freeze-planning-baseline | --freeze-implementation-base | --refresh-planning-baseline
scripts/integration_surface_check.sh [<kebab-name>] --plan-check | --refresh | --check [--json]
scripts/task_verify.sh --upgrade-v3 <kebab-name>
scripts/verification_workspace.sh run <kebab-name> -- <durable-driver...>
scripts/change_archive.sh [<kebab-name>]
scripts/archive_recover.sh --status
scripts/archive_recover.sh --acknowledge <kebab-name> --reason <single-line>
```

The `harness/` directory inside a change contains verification, footprint, the derived integration surface report, Evaluation and RCA evidence. It is not a second specification and is archived with the change. `--refresh` may create the report only after all tasks close; `--check` never refreshes stale evidence.

`verification_workspace.sh run` is used as the direct nested command of `task_verify.sh` or `evaluator_check.sh --run`, not as an unmanaged standalone runner. Its managed parent first checks the receiver protocol and then grants a change/action/lock/purpose-bound one-shot capability, which the receiver consumes before any workspace side effect. It creates a fresh ignored local directory, exports `AUTOAI_VERIFY_TMPDIR`, and removes one-off sources, consumers, binaries and output after a synchronous success or failure. Catchable interruptions trigger best-effort cleanup; later lifecycle gates safely clean or reject crash residue and require the workspace to be empty.

`change_adopt.sh` is the only supported way to attach AutoAI evidence to an existing, unarchived OpenSpec change. It validates the fixed 1.6.0 status contract and refuses any change that already has a `harness/` path; it stages all initial files and atomically installs the directory without replacing existing evidence. If archive reports a partial failure, all managed work remains globally blocked. Inspect with `archive_recover.sh --status`; acknowledgment only records a reviewed recovery after the current filesystem state is unambiguous, the recorded log path and every ancestor are non-symlinks, and main specs pass strict validation. Its single-line reason must be secret-free. It does not retry or roll back archive.
