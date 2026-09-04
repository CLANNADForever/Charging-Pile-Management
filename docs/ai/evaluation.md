# Independent Evaluation contract

quick_brief: Begin a fresh attempt, run independent real commands, fill the exact closed shapes below, and finish through the checker; static-only evidence cannot Pass executable behavior.

Evaluation lives at `openspec/changes/<change>/harness/evaluation.json`; there is no root evaluation file. Start with `scripts/evaluator_check.sh --begin`, run each independent command through `scripts/evaluator_check.sh --run --kind <kind> [--surface <id>] [--surface-role <id>=<role>] [--expect-exit <csv>] [--expected <text>] [--observed <text>] {--project-command <command-id> | -- <argv...>}`, copy the resulting closed command objects from `evaluation-command-ledger.json` into the candidate report without edits, and finish with `--finish`. The command-ID form is the normal route for a reviewed Project Profile operation and is recorded as `scripts/project_command.sh <command-id> --change <active-change> --json`; raw argv remains available only for an approved durable repository driver. The two forms are mutually exclusive. Use `--abort --reason ...` if the attempt cannot continue.

One-off independent programs run only in a fresh managed verification workspace. `--begin` clears crash residue, every `--run` cleans after the command, and `--finish`/`--recheck` require the workspace to be empty. A Pass may preserve command metadata and output digests, but cannot cite a temporary source/binary path or depend on material that disappears after evidence is recorded.

A valid report binds the evaluation ID, OpenSpec version, source/artifact/base-spec fingerprints, verification/budget/footprint digests, behavior criteria, real commands, implementation-economy assessments, blocking untested items and residual risks. Schema v3 additionally binds the frozen Integration plan, derived report and discovery identity, and assesses every candidate and every planned surface. IDs and references must close from delta scenario through task, surface and criterion to a real command. Static inspection alone cannot pass executable behavior.

`Fail` has priority over `Blocked`; all behavior criteria, economy assessments and Integration assessments must pass for top-level `Pass`. A completed Fail or Blocked attempt is useful history but cannot be archived. Editing code, tasks, specs, verification, footprint, the surface report or the completed Evaluation makes the attempt stale.

The candidate report is a closed JSON object. Its exact top-level keys are:

```text
schema_version, evaluation_id, change_name, verdict,
evaluation_started_at, evaluated_at, openspec_version, evaluator_role,
input_source_fingerprint, input_artifact_fingerprint, input_base_specs_fingerprint,
source_fingerprint, artifact_fingerprint, base_specs_fingerprint,
budget_block_sha256, change_footprint_json_sha256,
review_input, change_review,
implementation_economy, criteria, commands, blocking_untested, residual_risks
```

For schema v3, append the one additional key `integration_completeness`. A v3 command appends `surface_ids`, `surface_evidence_roles` and `surface_probe_bindings`; these fields must be copied exactly from ledger v2. Do not add those fields to the older v2 family.

Use these exact closed shapes; arrays may be empty only where the checker permits:

```text
requirement_ref = {spec_path, operation, requirement, scenarios[, renamed_to]}
command = {id, kind, argv, command, working_directory, started_at, finished_at,
           expected_exit_codes, exit_code, expected, observed, result,
           output_sha256}
command_v3_extra = {surface_ids, surface_evidence_roles, surface_probe_bindings}
surface_evidence_role = {surface_id, role}
surface_probe_binding = {surface_id, role, probe_id}
criterion = {id, description, requirement_refs, task_ids, status,
             evidence_command_ids, blocking_untested_ids}
blocking_untested = {id, requirement_refs, task_ids, reason, required_evidence}
residual_risk = {id, impact, rationale}

implementation_economy = {
  footprint_status, drift_explanation,
  classification_assessment, repository_impact_assessment,
  reuse_assessments, structural_assessments,
  obsolete_item_assessments, exception_assessments, result
}
assessment = {id, result, reason, evidence_paths[, evidence_command_ids]}
classification_assessment = {result, reason, evidence_paths[, evidence_command_ids]}
structural_assessment = {allowance_id, candidate_ids, result, reason,
                         evidence_paths, evidence_command_ids}
repository_impact_assessment = {result, surfaces}
surface = {surface, applicability, result, reason, evidence_paths,
           evidence_command_ids, not_applicable_reason}
drift_explanation = null | {metric_keys, reason, why_no_replan}

review_input = {
  schema_version, implementation_base_commit, head_commit,
  source_fingerprint, artifact_fingerprint, base_specs_fingerprint,
  layers, review_paths, git_state_fingerprint, raw_diff_persisted
}
layers = {effective, committed, staged, unstaged, untracked, dirty_gitlinks}
layer = {paths, state_fingerprint}
change_review = {schema_version, git_state_fingerprint, stages, findings}
review_stage = {name, started_at, completed_at, status, requirement_refs,
                task_ids, reviewed_paths, dimensions, evidence_command_ids,
                finding_ids, blocking_untested_ids, not_run_reason}
finding = {id, stage, category, severity, status, summary, requirement_refs,
           task_ids, evidence_paths, evidence_command_ids, return_to,
           resolution, tracking}
resolution = null | {summary, evidence_paths, evidence_command_ids}
tracking = null | {kind, id}
```

For an integrated change, the additional closed object is:

```text
integration_completeness = {
  planning_block_sha256, report_sha256, discovery_identity_sha256,
  inventory_assessment, candidate_assessments, surface_assessments,
  orphan_surfaces, result
}
inventory_assessment = {result, reason, evidence_paths, evidence_command_ids}
candidate_assessment = {
  candidate_id, source, disposition, surface_ids, surface_bindings,
  reason, producer_paths, implementation_consumer,
  evidence_paths, evidence_command_ids, orphan_ids
}
surface_assessment = {
  surface_id, result, reason,
  consumer_paths, old_consumer_paths, replacement_consumer_paths,
  kind_evidence, role_evidence, evidence_command_ids,
  blocking_untested_ids, orphan_ids
}
orphan_surface = {
  id, type, reason_code, mismatch_kind, candidate_ids, surface_id,
  kind, name, producer_paths, consumer_paths, reason,
  finding_id, blocking_untested_ids, route
}
```

Read candidate IDs and typed bindings only from the frozen `integration-surface-report.json`; do not invent, rename or drop them. Every report candidate has exactly one assessment. `mapped` binds the exact planned surface; `implementation_detail` requires a real production caller; `private_removal` and `non_semantic_change` need direct reviewed evidence; `orphan` must backlink to one orphan record. Every planned surface has exactly one assessment whose independent command IDs cover each planned Verify kind and each required evidence role.

An unplanned candidate or a planned surface with wrong requirement, task, classification or scope is `Blocked -> Planner` and needs an open specification-compliance finding. A planned surface with a missing consumer, unrelated command or missing role is `Fail -> Generator`. A required unavailable environment is `Blocked -> Environment` and must reference `blocking_untested`; it cannot be written only as residual risk. `inventory_assessment` must cover every changed production path and Pass before finish.

`operation` is `ADDED`, `MODIFIED`, `REMOVED`, or `RENAMED`; only RENAMED carries `renamed_to`. Command kind is `build`, `test`, `behavior`, or `static`, and command result is only `Pass` or `Fail`. Criterion/assessment/verdict values are `Pass`, `Fail`, or `Blocked`. Repository surfaces are exactly `product_targets`, `install`, `package`, and `ci`. An applicable surface needs evidence and a result; a not-applicable surface has null result, empty evidence, and a concrete `not_applicable_reason`. Every design allowance/reuse/obsolete/exception ID and every footprint structural candidate must be assessed exactly once.

`classification_assessment.evidence_paths` must exactly cover every current non-managed implementation path relative to the frozen implementation base. A command can support the assessment but cannot replace a missing path entry.

The command ledger is machine-written and bound to the active evaluation ID. Hand-written command strings, exit codes or timestamps cannot substitute for it: `--finish` and `--recheck` require `evaluation.commands` to equal the ledger exactly. Every terminal attempt is sealed once under `harness/evaluations/<evaluation_id>.json` as an envelope containing the terminal baseline, the terminal evaluation (or null for an aborted attempt), and canonical payload SHA-256 digests. Managed flows never overwrite a terminal envelope; this is a Harness contract, not an operating-system anti-tamper boundary.

Evaluation attempts form a bounded-monotonic terminal chain. A new attempt starts strictly after the unique latest terminal envelope; the five-minute rollback tolerance never permits an ambiguous or farther-future predecessor. Finding carry-forward chooses the complete envelope with the greatest unique terminal time, not the report's wall-clock `evaluated_at`; duplicate terminal times, orphan history or a stale predecessor fail before new state is written.

Within the same Evaluation attempt, review in two ordered stages named `specification_compliance` and `code_quality`. The first covers requirements, scenarios, scope, contracts and traceability. Only after it is non-blocking may the second cover correctness, safety, regression risk, reuse, complexity, test quality and repository impact. Each executed stage must cover the complete delta/task/review-path universe and cite independent command evidence. A blocking first stage makes the second `NotRun` with a reason.

`review_input` is generated from the frozen implementation base and binds effective, committed, staged, unstaged, untracked and dirty-gitlink layers with state fingerprints. `raw_diff_persisted` is always false: the Evaluator reads actual diffs transiently but never copies raw content into evidence. A changed index/worktree state makes an attempt stale even when the ordinary source fingerprint happens to be unchanged.

Findings use `Critical | Important | Minor` and `Open | Resolved | Deferred`. Specification findings cannot be Minor; Critical/Important cannot be Deferred. Open Critical/Important findings force their stage to Fail. A Deferred Minor must reference a real technical-debt or residual-risk ID. Prior Open/Deferred findings must remain with the same identity until explicitly resolved. Every completed v2 report is copied to immutable `harness/evaluations/<evaluation-id>.json`; the top-level Evaluation verdict remains the only final verdict.
