# U575 finite copy and validation development records

This increment adds one explicitly selected STM32U575ZIT6Q/TZEN0/baremetal
SRAM4 memory-to-memory copy. RTC AlarmA trigger 14 starts the first byte block;
the generated second LLI completes the finite copy. One concrete Stop2 owner
withdraws the producer, DMA, IRQ, gates and clocks before publishing SETTLED.
The original IO-32/SOC-35 exits remain open. Physical HIL is `not_executed`.

`package.json` identifies one compact archive under `raw/`. Restore and verify
it with the existing `tools/evidence/store.py`; its embedded member inventory
binds exact bytes. Logical-role manifests retain historical source identities
and real failure records. They do not turn earlier phases into final acceptance.

| Scope | Actual focused execution and limit |
| --- | --- |
| SRAM4 reset path | 14 real C/MMIO cases; selected/omitted configuration and actual CMake source/definition exports; ordinary optimized C11 object proof. Final emitted Reset → SystemInit → memory boot and failure park independently reviewed. |
| LPDMA receiver | 30 real production/model cases; alias rejection, ignored reset, cancellation, two-block count, effective clock profile, and permanently quarantined externally reset loans. |
| Same-owner compound Stop2 | 17 cases using the real receiver, retained clock, Stop2 owner and Core; RTC-before-DMA withdrawal, profile failures, residual marker, final release and recovery. |
| Retained clock and old WUT | 33 retained-clock cases; 21 WUT owner plus 9 real-clock compatibility cases. The old WUT also checks effective MSIK4. |
| Strict compiler contexts | Five GCC C11 `-Os` ASan/UBSan contexts; actual narrow four-bit conversion failures and final GREEN, with required analyzer contexts. No diagnostic suppression. |
| TSan watchdog regression | Original remote timeout and actual bounded local RED; complete 33-case TSan GREEN after a test-only child watchdog replaces deferred SIGALRM. Unexpected production return still fails the death assertion. |
| SDK tooling | 42 SDK/template cases and 26 transport cases, actual missing-private-probe RED and policy identity checks. Current v7 requires 47 roots/50 images; this count is a plan, not a completed matrix. |
| Installed public SDK | Maintained preparation, relocation, verification and `find_package(Nexus)` strict public C11/M33 full-reset link. All 27 relevant installed/source bindings match. Snapshot `1cf508277949ceeb6ef52809933640ea8f672fca4efa6c6b4c6d364938f662e7` contains 5,968 files, `publishable=false`, parent revision `48299637`; it is a development fixture. |
| H743 bounded checker repair | Actual differing-constant/unknown-join RED; 36 static cases, 15 ordinary build-gate cases and one installed public consumer. Full RAM execution proof, return protocol and IO-24 remain open. |
| Documentation | Independent review of all new EN/ZH contracts and FIG-27 arrows/labels; exact gettext and rendering checks. Whole-site and ordinary complete-index gates execute after the final freeze. |

Direct source-graph copy ELF `8c87d6f11a3cf2cd4db7708e279bf234f04f7899a93aeb7f719af2bae40262ea`
uses 15,816 B Flash, 2,384 B SRAM1 including the 2,048 B MSP, and 64 B SRAM4.
Installed SDK ELF `f4a69799aac2060f78cda8107b68aa1f4f448527f676b5425b6ad313f465af0e`
uses 15,840 B Flash with the same RAM. Its additional 24 B come from longer
`__FILE__` strings and alignment; named text function sizes match. These are
separate links, not a two-build reproducibility result or a measured sleep run.

Against the same critical MSIK-corrected baseline, the old WUT reference grows
9,416 → 9,548 B Flash, and retained-time 5,808 → 5,900 B. Their RAM remains
2,216 B and 2,152 B respectively. The ordinary default image stays exactly
byte-identical at 2,488 B Flash / 2,112 B RAM. Optional copy/LPDMA symbols and
SRAM4 startup are absent from those unselected images. The small nonzero old
opt-in Flash cost is retained explicitly; no zero-cost claim is made.

The original three remote PR failures, rejected pre-fix startup ELF, actual
compiler and configuration failures, and isolation-correction receipts are
preserved. The first copy image linked and met budget but omitted the startup
call; it is not final startup evidence. Its corrected successor proves the real
call and failure branch. Whole-SDK prefixes were archived once and independently
verified before reclaiming only owned generated copies; the compact tracked
archive avoids duplicating the complete dependency SDK.

No result establishes peripheral LPBAM, DVFS, RTOS Tick compensation, TrustZone
sleep, Standby/VBAT continuity, physical retention/current/accuracy/wake latency,
a production release or the full clean-source v7 consumer matrix. Ordinary
complete-index hooks, fresh remote CI/security and clean-source qualification
must run independently for the final commit. The earlier 6,109/2,808 gate counts
belong to their original source and are not inherited here.

`admission-package.json` retains only the later documentation admission fixes: the canceled ordinary hook, two real stale/missing-ledger RED regressions and complete 51-case GREEN, reviewed six derived EN/ZH files, and exact final eight-file bindings. The earlier rendered site is explicitly an obsolete development phase; final whole-site and ordinary gates run again. The main archive is not duplicated.

`retention-package.json` retains the actual failed R2 complete-index tool run and the public-component ABI audit correction. The unchanged parser now discovers convention-owned component headers as exact inputs; unknown layouts/providers, mutable tables and wrong linked methods still reject. Six new cases produced real RED before the minimal change; all 108 relevant cases then passed without skips. Production U575 sources and earlier ARM ELF identities are unchanged. R2 did not reach GoogleTest execution and is not acceptance.

`configuration-package.json` retains the R3 failed configuration domain and its exact repairs: the explicit copy reference catalogue, type-exact default SRAM1 plus unselected SRAM4 CI inventory, and the disposable hook fixture's real status-module dependency. Actual targeted RED/GREEN, default-environment failure and pinned retry are preserved. Complete primary configure 922, CI 485 and workflows 117 passed without skips. This is a pre-commit development phase; the ordinary whole-index gate reruns independently. Source before/final copies are referenced by hash and Git location instead of duplicated in the archive.

`final-gate-package.json` preserves actual ordinary commit R4 for clean successor
`be919138` / tree `b83e4079`:6178 Google cases across344 executables and2832
tool cases passed, with fresh execution receipts and zero skips. It also binds
the passed124-page EN/ZH development site. Earlier failed attempts are not reused
as acceptance, and these counts do not qualify subsequent source increments.
