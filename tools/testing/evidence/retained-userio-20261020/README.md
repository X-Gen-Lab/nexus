# Retained time, Stop2 and copied UART development records

This is one compact development record, not a clean software candidate or a
physical qualification. `package.json` identifies the single archive under
`raw/`; the existing `tools/evidence/store.py` verifies every member before
restoration. The archive contains 5,509 files and 27,331,810 logical bytes, stored
in 2,654,275 compressed bytes. Preserve its original phase and source identities.

| Recorded scope | Execution and limitation |
| --- | --- |
| Copied UART mediation | Actual RED/GREEN, 37 final Google cases; public M4/M33 links. Kernel identity, fault-contained copy and persistent role authority are external ports. |
| Retained clock and shared RTC decoder | 27 clock and 25 RTC cases on the final explicit-extension source; source and calendar-helper hashes are bound in `retained/evidence/integration-tidy-refresh/`. |
| Exact U575 Stop2 owner | 19 owner and 9 combined cases; real C owners and ClockDomain, external MMIO/instruction models. `stop2/range4-correction/` and the independent review retain the actual regression and final fix; earlier refactor receipts retain their earlier source scope. |
| ARM closure | Four public/source-SDK-template C11 M33 links; final retained image 5,808 B Flash / 2,152 B RAM and Stop2 image 9,400 B Flash / 2,216 B RAM, including the 2,048 B MSP reservation. These are source-graph checks, not a new relocated installed-SDK qualification. Original artifacts are retained separately from the refreshed bytes. `stop2/range4-arm/` binds the final corrected source and consumer bytes to four fresh links with unchanged budgets. |
| SDK boundaries | Actual omitted-consumer RED/GREEN, selection and template checks, and a freshly relocated Native C/C++ SDK build/run. The immutable v6 plan requires 46 roots / 49 images; that plan count is not an execution result. |
| Documentation | Independent public-contract translation reviews and diagram reviews; derived ledger substitutions retain the authoritative CSV exits. A page's final single-LF normalization and matching catalog hashes are recorded separately. |

The I3C core's 40-case RED/GREEN, public Runtime link and pinned reference facts
are tracked once in [its separate archive](../../../../tests/evidence/i3c-controller/README.md).
They are not duplicated here and do not supply a physical I3C provider.

`integration/` retains the real PR #19 SDK and static-analysis failures, the
corrected relocated consumer run, analyzer diagnostics and the complete Native
prebuild. The first six-file analyzer pass includes its original failures; fresh
clock and Stop2 records show the corrections. Focused checks never substitute
for the complete required CI analysis. The initial full documentation attempt
rendered both languages but was rejected because source metadata changed during
execution; it is retained as a failure, not admitted as a successful build.

A later independent check against RM0456 sections 10.10.4 and 10.7.8 found
that the old Stop2 predicate and model had used Range1 encoding for Range4.
The preserved actual RED rejects that implementation; the minimal predicate fix
and corrected independent fixture pass all 28 cases. The final source admits
`VOS=00`, `VOSRDY=1`, `BOOSTEN=0`; it adds no voltage writer. Earlier source
and ELF receipts are historical phases, not final-source evidence. Cancelled
and failed ordinary attempts under `integration/ordinary-attempts/` are never
acceptance reports. Final full gates execute separately.

`history/109-complete-index/` preserves the ordinary gate for commit
`109f6f8c5e87dd8a37b8ae8c9364a3d9b0fdbac7`: 5,976 Google cases and their raw
reports. This historical result does not apply to the next integrated source and
does not negate the two subsequently discovered CI failures. Final integrated
complete-index gates, full bilingual build and clean GitHub checks must execute
independently after this development freeze.

No source deadline is adopted across the stopped Core TIM2 domain. No local
quarantine flag revokes a request or buffer loan. Hardware retention, electrical
LSE/PC13 behavior, wake latency, power, kernel isolation and physical HIL remain
unverified. The complete IO-32/38/39 and SOC-35/36 exits remain open.

Example restoration, using the archive's exact original identity:

```sh
python tools/evidence/store.py restore \
  --package tools/testing/evidence/retained-userio-20261020/package.json \
  --archive tools/testing/evidence/retained-userio-20261020/raw/6abb26afb87302115818de61fdb38c3f818a4d70ddeceb1df7cb8927fbbf6835.tar.gz \
  --destination /path/to/empty-restoration \
  --report /path/to/restoration.json
```
