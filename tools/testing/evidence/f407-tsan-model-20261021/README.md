# F407 synchronous-model TSan compiler repair

The observed PR #19 head `56a66e05014d760fbbd8d3e2e3b5909e86e80da6`
failed GCC 13 TSan compilation at the model's `atomic_thread_fence` call,
with `-Werror=tsan`; its test step did not execute. The local maintained TSan
build independently reproduced that compiler RED before this repair.

Only three synchronous MMIO-model barrier hooks now use
`atomic_signal_fence`, with an explicit compiler-ordering comment. All fake
register, IRQ, DMA and timer progress executes on the caller stack. This is
not a replacement for a hardware barrier or inter-thread publication. The
production Cortex-M barriers and C11 request release/acquire code are unchanged.
No warning, sanitizer, assertion or case is disabled.

The same normal target and instrumented TSan target built successfully; both
actually executed all 35 GoogleTest/GoogleMock cases with zero failures,
errors or disabled cases. The archive contains original compiler RED, GREEN
receipts and raw streams, fresh XML, source/patch identities, provenance and
independent review. Two target executions do not establish whole-project
TSan acceptance. Full ordinary commit gates and the new remote CI head must
be assessed separately. No physical HIL was executed.

[package.json](package.json) binds the single compact raw archive and member
manifest; [verified.json](verified.json) records byte verification. The living
README and package locator are outside the immutable raw payload.
