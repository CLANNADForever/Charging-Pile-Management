# RCA

For every unexpected test, build or runtime failure, read the complete error and obtain a stable reproducer before changing code. For a complex or repeated issue, record this sequence:

1. Evidence and boundary: inputs, exact reproducer, actual output, recent changes and the boundary where behavior first diverges.
2. Pattern comparison: a working repository example and the concrete differences from the failing path.
3. One hypothesis and one variable: state a single causal hypothesis and run the smallest experiment that can disprove it. A failed experiment creates a new hypothesis; it does not justify another patch on top.
4. Direct fix and regression: preserve the reproducer, fix the root cause, run focused and related regression commands, and add a reusable Trigger/Check only when evidence supports it.

Track validated fix attempts for the same problem. After three unsuccessful direct fixes, stop stacking patches and return to Planner and the user to recheck architecture, scope and assumptions. Any resulting contract, dependency, target, structural allowance or budget change requires revised OpenSpec planning, strict validation and renewed human approval.

Classify an Integration failure before fixing it. A planned surface whose real consumer, dispatch, entrypoint or independent behavior is missing is an implementation failure and returns to Generator. An unplanned surface, wrong requirement/task link, wrong consumer classification or expanded scope is a planning failure and returns to Planner for renewed review. Do not hide either case with a test-only call, residual risk, implementation-detail label or another speculative interface.

RCA stays change-local when it belongs to one change. Do not close with “be more careful”, and do not turn a one-off incident into a global rule without evidence. Archive requires the resulting task/surface/Evaluation evidence, but it does not infer the quality of every debugging step from prose alone.
