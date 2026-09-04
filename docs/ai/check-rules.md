# Repository check rules

Rules here must be concrete and reusable. Each addition records a Trigger, Check and Source.

## Built-in gates

- Trigger: a change adds a helper, manager, parser, service, public type, dependency, build-graph entry or distribution surface. Check: prove an approved structural allowance and show why an existing extension point cannot serve it.
- Trigger: implementation footprint exceeds an expected threshold. Check: stop at review/hard boundaries; `drift_warning` requires an explicit reason and independent evaluation.
- Trigger: an external contract changes. Check: classification, affected consumers, migration/transition, rollback and representative caller verification are complete.
- Trigger: task completion is requested. Check: `task_verify.sh --complete` proves a current RED/GREEN/REGRESSION closure or an approved alternative for every declared Verify kind; a checkbox edited by hand has no authority.
- Trigger: archive is requested. Check: tasks, strict validation, planning/TDD/source fingerprints, footprint, both review stages, immutable Evaluation Pass and UTC destination all pass.
