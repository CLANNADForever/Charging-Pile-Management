# Testing

Use the test capability and stable command IDs recorded in `.ai-harness/project-profile.json` together with the project's actual fixtures. When recording managed evidence, select a Profile command with `task_verify.sh ... --project-command <command-id>` or `evaluator_check.sh --run ... --project-command <command-id>` so the wrapper owns execution and the ledger. OpenSpec describes what must hold; the Profile describes how this repository executes it. Cover normal, boundary and error scenarios named by the delta spec. A compile-only check is insufficient for changed runtime behavior; an incremental build is insufficient when a clean configure or representative consumer is relevant.

Separate durable regression assets from one-off verification programs. A test, compatibility fixture or representative consumer with ongoing value remains under paths classified by the relevant Profile module and is reviewed, built and versioned normally. Exploratory probes, disposable consumer sources, temporary downstream projects, executables and outputs must be created only under the workspace supplied by `scripts/verification_workspace.sh`; they never enter Git, project build/install/package inputs or the archived change.

Behavior changes default to four stages:

1. RED: write a focused test or minimal reproducer first, run it directly and confirm it fails because the approved behavior is missing. A compile error, broken fixture or unrelated environment failure is not a valid RED.
2. GREEN: add only the smallest production change that makes the focused test pass.
3. REFACTOR: remove duplication or improve names only while the focused test remains green; do not expand behavior or introduce an unapproved abstraction.
4. REGRESSION: run the relevant wider build, tests and representative behavior selected by the task and design.

Planner must put exactly one closed TDD Policy block in `design.md`, including an empty `exceptions` array when every task uses normal TDD:

````markdown
<!-- autoai:tdd-policy:v1 -->
```json
{
  "schema_version": 1,
  "default": "required",
  "exceptions": []
}
```
<!-- /autoai:tdd-policy:v1 -->
````

Each exception has exactly `id`, `category`, `task_ids`, `paths`, `reason`, `alternative_verify_kinds`, and `exit_condition`. Categories are `generated_output`, `documentation_only`, `configuration_only`, `disposable_prototype`, `unavailable_hardware`, or `unavailable_external_service`. Its alternative kinds must exactly equal each covered task's declared Verify kinds. Exceptions are frozen with the planning baseline; Generator cannot add or widen one.

Record normal behavior evidence with `task_verify.sh <task> --phase red|green|regression --cycle <id> ... {--project-command <command-id> | -- <argv>}`. Use the command-ID form for reviewed Project Profile operations and raw argv only for an approved durable repository driver; they are mutually exclusive. The wrapper records the command-ID form as `scripts/project_command.sh <command-id> --change <active-change> --json`. For a verification v3 task, bind closing consumer evidence with `--surface <id>` for role `current` or repeat `--surface-role <id>=current|old_consumer|replacement_consumer|absence_probe` exactly as assigned by its approved task obligation. RED additionally requires focused `--test-path`, nonzero `--expect-exit`, `--failure-class`, `--expected-failure`, `--match-output`, and `--observed`; it is stored only as `ExpectedFailure` or `InvalidRed`, never Pass. GREEN must reuse the same normalized argv and unchanged focused test after a real source change. REGRESSION must cover every declared Verify kind and surface role. Complete only through `task_verify.sh --complete <task>`.

A temporary program cannot be a `--test-path`, `--path`, producer/consumer path or direct argv dependency. If a repeatable command needs one, keep a durable driver in the project and invoke it through `scripts/verification_workspace.sh run <change> -- ...`; the driver recreates the program beneath `AUTOAI_VERIFY_TMPDIR` from an empty directory. Command completion, task completion, Evaluation finish and archive verify that no temporary program remains.

Managed evidence timestamps are logically monotonic and tolerate at most five minutes of host-clock rollback. Timestamps farther than five minutes in the future are rejected both when appending evidence and during task/Evaluation/archive rechecks; wall-clock timestamps do not replace command results, fingerprints or output digests.

An approved exception uses `--phase alternative --exception-id <id>` and every declared alternative kind. A disposable prototype can never complete a task. Unavailable hardware or external service remains `blocking_untested` during Evaluation even after alternative evidence. Do not weaken assertions, narrow inputs or replace behavior with mock-only checks to manufacture GREEN.

When user code already exists before a test, preserve it and establish a focused characterization or regression path with explicit review; do not delete the user's implementation merely to recreate textbook RED.
