# Change workflow

<!-- autoai:workflow-binding:v1
{"role":"workflow-documentation","facts":["schema-upgrade-v2-explicit","schema-upgrade-v3-explicit","project-profile-command-ids","single-evaluator-verdict"]}
-->

quick_brief: Plan every product surface and real consumer in OpenSpec, bind Generator and Evaluator commands to it, reject unmatched candidates, and archive only a fresh unified Pass.

```mermaid
flowchart LR
  U[User document or request] --> P[Planner investigates callers and entrypoints]
  P --> O[Proposal + specs + design + tasks + surface inventory]
  O --> R{Human review}
  R -->|revise| P
  R -->|approved| G["Generator: one task"]
  G --> T["RED → GREEN → REFACTOR → REGRESSION"]
  T --> V["Surface-bound task evidence"]
  T -->|unexpected failure| D[Systematic RCA]
  D -->|direct fix| T
  D -->|three failed fixes or changed assumptions| P
  V --> C["all_done: footprint + surface candidate report"]
  C -->|unplanned surface or wrong plan| P
  C --> S["Evaluator stage 1: specification compliance"]
  S -->|implementation gap| G
  S -->|planning gap| P
  S --> Q["Evaluator stage 2: quality + candidate review"]
  Q -->|missing consumer or bad implementation| G
  Q -->|contract, scope or budget change| P
  Q --> E["Independent real-consumer commands + one verdict"]
  E -->|Fail| G
  E -->|Blocked| P
  E -->|fresh Pass| A[Archive wrapper]
  A --> M[Main specs + archived evidence]
```

## Product-surface closure

Every added, modified, deprecated or removed production surface must form this closed chain:

```text
approved requirement -> task -> producer -> real consumer/entrypoint
                    -> Generator closing evidence -> independent Evaluator command
                    -> observable result
```

The seven surface kinds and their minimum consumers are:

| kind | required consumer | minimum proof |
|---|---|---|
| `internal_api` | `production_caller` | A production path invokes it through a real product entrypoint; a unit test calling it directly does not count. |
| `external_api` | `representative_external` | A representative downstream consumer uses installed/exported artifacts and compiles, links and, where applicable, runs. |
| `callback_or_plugin` | `registration_dispatch` | Both registration and an actual dispatch that reaches the implementation. |
| `cli` | `real_entrypoint` | The real executable, with observable arguments, output, exit code or state change. |
| `configuration` | `real_entrypoint` | A real process reads the value and changes observable behavior; parser-only tests are insufficient. |
| `protocol_or_persistence` | `producer_consumer_pair` | A real producer/consumer or compatibility path, not one-sided serialization alone. |
| `build_or_install` | `downstream_build` | A downstream configure/build/link and any necessary runtime check. |

“Might be useful later” is not a consumer. Delete such an interface or return to Planner. File-local helpers remain implementation details, but every changed production path/candidate must still receive an Evaluator disposition.

## Integration Completeness v1

Planner puts exactly one closed block in `design.md`. `reviewed_inventory` is the portable default. Choose `clang_ast` only by explicit design decision, with a safe repository-relative `compile_commands_path`, complete translation-unit coverage and compatible Clang tools. AST only improves candidate discovery; it never proves a consumer or runtime behavior.

````markdown
<!-- autoai:integration-completeness:v1 -->
```json
{
  "schema_version": 1,
  "discovery": {
    "mode": "reviewed_inventory",
    "compile_commands_path": null
  },
  "surfaces": [
    {
      "id": "surface-health-refresh",
      "kind": "internal_api",
      "name": "health::Service::refresh()",
      "change_kind": "added",
      "contract_impact": "compatible",
      "compatibility": null,
      "producer_paths": ["include/health/service.hpp", "src/health/service.cpp"],
      "consumer_kind": "production_caller",
      "consumer_paths": ["src/health/daemon.cpp"],
      "entrypoint": "health daemon refresh cycle",
      "evidence_contracts": [
        {
          "probe_id": "probe-health-refresh-test-current",
          "kind": "test",
          "role": "current",
          "argv": ["ctest", "--test-dir", "build", "-R", "health_refresh", "--output-on-failure"],
          "expected_exit_codes": [0],
          "output_contains": "100% tests passed"
        },
        {
          "probe_id": "probe-health-refresh-behavior-current",
          "kind": "behavior",
          "role": "current",
          "argv": ["build/bin/health-daemon", "--refresh-once"],
          "expected_exit_codes": [0],
          "output_contains": "health-refresh-ok"
        }
      ],
      "requirement_refs": [
        {
          "spec_path": "specs/health/spec.md",
          "operation": "ADDED",
          "requirement": "Refresh running health state",
          "scenarios": ["Daemon triggers refresh"]
        }
      ],
      "task_ids": ["1.2"],
      "verify_kinds": ["test", "behavior"],
      "task_obligations": [
        {
          "task_id": "1.2",
          "verify_kinds": ["test", "behavior"],
          "evidence_roles": ["current"]
        }
      ],
      "expected_observation": "The running daemon invokes refresh and publishes the new state.",
      "symbol_identities": null
    }
  ]
}
```
<!-- /autoai:integration-completeness:v1 -->
````

`id` starts with `surface-`. Requirement references must exactly match a delta requirement/scenario; task IDs must exactly match leaf tasks and their `Covers` edges. Each task obligation uses only Verify kinds declared by that task. Across a surface, obligations must exactly cover the Cartesian product of top-level Verify kinds and required roles. Use an explicitly reviewed empty `surfaces` array only when no production surface changes; later production candidates still force review.

`evidence_contracts` contains exactly one approved probe for every surface Verify-kind/required-role pair. `probe_id` is globally unique; `argv` and canonical shell exit codes are exact, not patterns; `output_contains` is a non-empty observable marker. Credential-bearing argv is forbidden. A `build_or_install` surface declares the closed boolean `runnable_artifact` field and always needs a downstream `build` probe; when the field is true, it also needs an executable `test`/`behavior` probe. Other surface kinds must not carry that field. The plan therefore authorizes a repeatable consumer experiment, not an arbitrary command that merely exits successfully.

`producer_paths`, `consumer_paths`, compatibility paths, focused RED tests and evidence argv must not point into `.ai-harness/logs/verification-workspaces`. Keep a consumer/test in the repository only when it is approved long-term regression or compatibility coverage. For a one-off C++ consumer, approve an exact command whose durable driver is invoked through `scripts/verification_workspace.sh run <change> -- ...`; the wrapper provides `AUTOAI_VERIFY_TMPDIR`, starts empty and removes generated sources, binaries and output when the synchronous driver returns. A killed wrapper or detached background writer may leave crash residue, which the next lifecycle gate must clean or reject.

`change_kind` is `added`, `modified`, `deprecated` or `removed`. `contract_impact` is `compatible`, `breaking`, `deprecation` or `removal`; the last two pair only with deprecated/removed. Non-compatible surfaces carry the closed compatibility fields `old_consumer_paths`, `replacement_consumer_paths`, `replacement_policy`, `expected_old_result`, `migration_path` and `exit_condition`. Set `replacement_policy` to `required` whenever a replacement path exists. Only a removal whose referenced requirements are all `REMOVED` may use `requirement_approved_none`, and then the replacement path array must be empty. Evidence roles are `current`, `old_consumer`, `replacement_consumer` and `absence_probe`: compatible needs current; breaking/deprecation need old and replacement; removal needs old and absence, plus replacement when planned.

Run `scripts/integration_surface_check.sh <change> --plan-check --json` before human approval and planning freeze. A valid plan is intent, not proof that paths are implemented.

## Generator closure

Generator handles one verifiable task at a time. Before coding, compare the requested change with the frozen surface inventory. If implementation needs a new interface, entrypoint, consumer, dependency, target, compatibility behavior or evidence obligation, stop and return to Planner; do not add it first and document it later.

A behavior task records RED as `ExpectedFailure`, implements the smallest GREEN against the same immutable focused test, then records declared REGRESSION evidence. Closing commands for an integrated task use repeatable bindings:

```text
scripts/task_verify.sh <task-id> --phase regression --cycle <id> --kind <kind> \
  --surface <surface-id> \
  --surface-role <surface-id>=old_consumer \
  ... --project-command <command-id>
```

Use `--project-command <command-id>` for a reviewed Project Profile operation, or `-- <argv...>` for an approved durable repository driver that is not a Profile command; the two forms are mutually exclusive. The command-ID form is recorded and matched as the normalized argv `scripts/project_command.sh <command-id> --change <active-change> --json`, so the Integration probe must declare that exact wrapper argv. Generator must not run `project_command.sh` first and then submit a second command to the evidence wrapper.

`--surface` means role `current`. Bind only obligations assigned to this task and command kind. A surface-bound REGRESSION, or an approved non-environment ALTERNATIVE that closes an obligation, must match the planned probe's argv and expected exit codes byte-for-byte and its captured output must contain the planned marker; the ledger records the derived `surface_probe_bindings`. Hardware/external-service provisional ALTERNATIVE records the assigned surface roles and blocking exception but no fake closing probe. RED/GREEN, grep, static inspection, compile-only checks, self-reported success, or an unrelated successful command cannot close a consumer obligation. `scripts/task_verify.sh --complete <task-id>` is the only task-checkbox transition and rechecks current planning, source, TDD and surface evidence.

Before adding a test or consumer fixture, decide whether it has durable regression/compatibility value. Durable assets remain in the project and count in the footprint. One-off experiments use the managed workspace and are cleaned after every command; `--complete`, final report refresh, Evaluation finish and archive all fail closed unless that workspace is empty.

After every task is complete, run `scripts/integration_surface_check.sh <change> --refresh --json`. It derives `harness/integration-surface-report.json` from the frozen plan, implementation-base diff, footprint and optional AST discovery. It does not edit OpenSpec. A later `--check` must match exact current bytes; stale evidence is never silently refreshed by Evaluation or archive.

## Independent review and verdict

Evaluator is a separate attempt. Before `--begin`, read the actual committed, staged, unstaged, untracked and dirty-gitlink state plus `integration-surface-report.json`. Review every path/structural/AST candidate, including candidates not mapped to a planned surface. Independently execute real-consumer commands with the same `--surface` or `--surface-role` syntax on `evaluator_check.sh --run`.

Evaluator must reject an accidental one-off probe in the Git review input and any closing command that depends on a temporary path scheduled for deletion. Independent one-off probes run through a fresh managed workspace. Archived evidence keeps exact argv, exit code, output digest/summary and assessment, not temporary source or binaries.

Every candidate gets exactly one disposition and every planned surface gets exactly one assessment. A missing real consumer, unrelated evidence or missing evidence role is `Fail` and returns to Generator. An unplanned candidate or wrong requirement/task/classification/scope is `Blocked` and returns to Planner. An unavailable required environment is `Blocked`; it cannot be downgraded to residual risk. `Fail` has priority over `Blocked`, and the unified Evaluation remains the only verdict.

When planning changes after implementation starts, affected tasks remain unchecked. Planner revises artifacts, runs strict validation and `--plan-check`, obtains renewed human approval, then uses `snapshot_update.sh --refresh-planning-baseline`. This never advances the frozen implementation base. An old v1/v2 evidence family is not implicitly upgraded; only an eligible evidence-empty change may use `task_verify.sh --upgrade-v3 <change>`.

Fresh context is optional for high-risk tasks and independent review. Generate it with `change_status.sh --agent-context investigator|reviewer` or `--agent-context generator --task <id>`. The same worktree always has one writer; parallel agents may only perform read-only investigation or review. The optional linked-worktree pilot is explicitly leased through `harness_lock.sh isolation-*`; it never creates, commits, merges, pushes, stashes, resets, prunes or deletes Git state. Worktree creation and Git integration/cleanup remain user-owned.

## Project capability control and derived navigation

`.ai-harness/project-profile.json` is the reviewed description of modules, adapters, path roles, build targets/graph/distribution surfaces, stable command IDs and optional `capability_status` declarations. It is not a specification or state store. A command-backed capability is available by declaration and then subject to runtime checks; a commandless capability may be `unavailable`, `not-applicable` or `needs-approval`, while omission is reported as `absent`. Run `scripts/harness_doctor.sh --json` for read-only control-plane, capability and active-evidence diagnostics. Execute a reviewed operation through the evidence wrapper's `--project-command` option; the wrapper invokes `scripts/project_command.sh` and retains the canonical environment envelope under the active change. Direct execution does not create task or Evaluation evidence.

The following navigation and governance views are advisory or policy inputs, never a second source of truth:

- `scripts/project_index.sh --refresh --json` builds a Profile/source/toolchain-contract-bound project index; `--check` rejects stale bytes.
- `scripts/context_slice.sh <change> --refresh --json` orders P0 through P3 reading candidates under an explicit heuristic token budget. It never limits Evaluator diff or untracked-file review.
- `scripts/campaign.sh <campaign> --status --json` validates a read-only cross-change DAG and may suggest `--next-ready`; it never creates or selects a change.
- `scripts/event_audit.sh <change> --refresh --json` rebuilds a hash-chained projection from retained evidence.
- `scripts/organization_policy.sh --check --json` validates optional local/CI/release execution restrictions; policy can deny a Profile command but cannot create one.

Legacy evidence stays in its declared schema. An eligible evidence-empty change may explicitly run `scripts/task_verify.sh --upgrade-v2 <change>` and then `scripts/task_verify.sh --upgrade-v3 <change>`; normal reruns and `--force` never perform an implicit evidence migration.
