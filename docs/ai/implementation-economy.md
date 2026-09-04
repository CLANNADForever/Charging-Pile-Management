# Implementation Economy v2 contract

quick_brief: Use this exact closed JSON shape in design.md, derive change-specific thresholds, and never copy the placeholder zeros as universal limits.

Planner puts exactly one block with the following markers in `design.md`. Derive every threshold from the reviewed change; the numbers below are placeholders, not defaults. Object keys are closed: do not add fields.

````markdown
<!-- autoai:implementation-economy:v2 -->
```json
{
  "schema_version": 2,
  "profile": "small",
  "rationale": "Explain the independently reviewable implementation boundary.",
  "classification": {
    "production": ["path/to/product/**"],
    "tests": ["path/to/test-assets/**"],
    "project_docs": ["path/to/product-docs/**"],
    "project_tooling": ["path/to/project-tooling/**"],
    "examples": ["path/to/examples/**"],
    "generated": [
      {"output": "path/to/generated/**", "inputs": ["path/to/generator-input/**"], "argv": ["./reviewed-generator"]}
    ],
    "vendor": ["path/to/vendor/**"]
  },
  "thresholds": {
    "production": {
      "added_lines": {"expected": 0, "review_at": 0, "hard_limit": 0},
      "touched_files": {"expected": 0, "review_at": 0, "hard_limit": 0},
      "new_files": {"expected": 0, "review_at": 0, "hard_limit": 0}
    },
    "tests": {
      "added_lines": {"expected": 0, "review_at": 0, "hard_limit": 0},
      "touched_files": {"expected": 0, "review_at": 0, "hard_limit": 0},
      "new_files": {"expected": 0, "review_at": 0, "hard_limit": 0}
    },
    "project_support": {
      "added_lines": {"expected": 0, "review_at": 0, "hard_limit": 0},
      "new_files": {"expected": 0, "review_at": 0, "hard_limit": 0}
    },
    "generated": {
      "files": {"expected": 0, "review_at": 0, "hard_limit": 0},
      "bytes": {"expected": 0, "review_at": 0, "hard_limit": 0}
    }
  },
  "structural_allowances": {
    "public_contracts": [{"id": "contract-001", "name": "ExactTypeOrContract", "reason": "Why it is needed"}],
    "build_targets": [],
    "build_graph_entries": [],
    "distribution_surfaces": [],
    "direct_dependencies": []
  },
  "reuse_decisions": [
    {"id": "reuse-001", "path": "src/existing.cpp", "symbol": "ExistingType", "decision": "extend", "reason": "Why reuse/extend/reject is correct"}
  ],
  "obsolete_items": [
    {"id": "obsolete-001", "path": "src/old.cpp", "symbol": "OldType", "disposition": "deprecate", "reason": "Why it remains temporarily", "exit_condition": "Exact removal condition", "task_or_debt": "2.3"}
  ],
  "exceptions": [
    {"id": "exception-001", "metric": "binary", "paths": ["fixtures/input.bin"], "reason": "Why generic line accounting cannot apply", "requirement_refs": ["specs/capability/spec.md | ADDED | Requirement"], "task_ids": ["2.1"], "verification": "Exact specialized verification"}
  ]
}
```
<!-- /autoai:implementation-economy:v2 -->
````

Allowed profiles are `micro`, `small`, `medium`, and `large`; they do not supply implicit limits. Every path is derived from the reviewed repository and Project Profile—placeholder paths are illustrative and must be replaced. Classification patterns are safe repository-relative glob patterns. Generated entries require output, non-empty inputs, and a secret-free argv array. Structural entries use exactly `id`, `name`, and `reason`; build targets, graph entries and distribution surfaces are adapter-neutral. Reuse decisions are `reuse`, `extend`, or `reject`. Obsolete disposition is `delete`, `deprecate`, or `retain`; delete has no future-exit fields, while deprecate/retain require both. Exception IDs, paths, requirement references, task IDs, and specialized verification are mandatory. IDs are unique across all four groups.

Existing changes containing the closed v1 block and `cmake_targets` remain read-only compatible. They are never silently rewritten; only newly reviewed or explicitly replanned changes use v2.

Footprint treats NUL-containing and invalid-UTF-8 content as binary, reports binary file count, bytes and paths, and requires an explicit reviewed exception. An empty regular text file contributes zero added lines. Invalid UTF-8 in `design.md` is a contract error, not replacement-character-tolerant input.
