# USB FS interrupt-IN development evidence

This collection preserves one compact development-evidence increment for
selected full-speed interrupt IN on the maintained STM32F407/GD32F470 packet
providers. It is not a clean software candidate, complete-index gate result,
formal SDK/reproducibility qualification, physical HIL result or full IO-35 exit.

The root's corrected strict bilingual documentation build passed, and its final
source-freeze receipt verifies the 593 original copied inputs and all 34 current
source files. `source/final-source-freeze.json` binds the exact copy-list hash.
The initial collection phase in `collection-inputs.json` deliberately remains
marked unsealed; it describes the copy stage, not the later portable package.
The final export and verification receipts accompany this collection separately.

The existing `tools/evidence/store.py export` and `verify` provide portable
byte/member verification; no alternative archive format, evidence framework or
hardlink workflow is used. The bounded payload remains below 25 MiB and excludes
build trees, objects, static archives, tool/vendor bodies, official PDF/text
bodies and duplicate whole source after-images.

## Execution scopes kept separate

- Normal focused development GREEN: ten USB GoogleTest programs, 186 actual
  cases, no failures/errors/disabled/skips. These include existing control,
  ACM, clocks and factory coverage; 186 is not an interrupt-only test count.
- Configuration tools: final rerun R3 has 51 passing unittest cases. Earlier
  nonexistent-module/bare-module setup failures remain original failed attempts.
- Actual TDD RED: initial configuration run (13 cases, 7 errors), descriptor
  runtime RED (9 cases, 3 failures), provider RED (Native 58/2 and two SDK 8/1
  results), and inactive old-BULK RED (Native 59/3). Original failed cast
  compilation and bad-binary-path attempts are preserved separately from real
  runtime RED; they are not counted as contract execution.
- Sanitizers: the initial ten-target ASan build failed with an actual OOM
  failure (`cc1plus` killed, build return code 1). The root-reported OOM/kill
  counters 62/11 are resource context, not test counts. Only `asan-selected-r2`
  executed and passed four focused provider/
  descriptor programs, 84 ASan/UBSan cases. It is not complete Native ASan or
  complete-index evidence. The retired ASan build's storage proof is a metadata
  reference only and does not create extra test qualification.
- ARM development: six selected bulk/interrupt fixture images plus two
  unselected images retain exact ELF/BIN/map, reported generated bundles,
  resource/retention results, source/dependency/tool identities and raw command
  records. The source-before/source-after observations are stable for that ARM
  run. The original GIT-environment source-guard rejection is kept as failure.
  Linked selection deltas are whole-image comparisons, not isolated driver cost.
- Documentation: original strict bilingual-site failure before the four API
  catalog repairs remains retained. Nine final reviewed PO files and the public
  API/design/support inputs are captured once. The corrected full build's
  final receipt records return code 0: 127 source pages, 254 translation records
  and four actual Doxygen/gettext/English HTML/Chinese HTML commands passed.
  Their original receipts, stdout/stderr and wrapper logs are included together
  with five corresponding POT inputs. HTML, API XML and doctrees are excluded.

## Source and review closure

The final changed tracked source files and three explicit new authored TOMLs
are captured once from the primary working tree after documentation PASS.
Their exact hashes, Git baseline and tracked patch are bound in
`source/source-manifest.json`; all 34 source copies were verified by the root.
`collection-inputs.json` maps original paths to archive members and is the exact
frozen copy-list byte record referenced by the source-freeze hash. Original
command/report locators remain unchanged. Root's
accepted-evidence addendum is excluded from this USB source snapshot. Unchanged
source, vendor and tool inputs are identified through the existing source-input,
compiler-dependency and tool identity records rather than copied wholesale.

Official register facts, independent control/DCD/public/config reviews, original
test-only patches, the old-behavior scaffold, reviewed provider/guard proposal,
formatting successor hashes and documentation/catalog/API review records retain
separate provenance. Earlier proposed after-images are represented by deltas and
identity records; the final source bytes appear once.

The bounded behavior adds EP1..3 IN interrupt selection with MPS1..64, preserves
single-packet PKTCNT1 and MCNT0, and extends DATA0 command/readback classification.
An observed inactive nonzero endpoint rejects STATE before borrowing or FIFO
writes. No reset/clear-halt/submit path activates it automatically. Explicit
SetConfiguration activation remains a follow-up; no periodic scheduler, queue,
worker, CDC notification policy, PCB route or physical timing claim is added.

## Accepted 430 gate is a reference, not archive content

The separate accepted 430 source gate has 702 commands, 348 Google executables,
6288 Google cases and 2951 tool cases. Its archive SHA256 is
`0e00e1db76221e47ead52ff0952fd5134d4997d0ddd44676baa72c317c37a797`
(1,551,911 bytes), tracked once under
`tools/testing/evidence/h743-canonical-input-20261021/raw/`.
Only its original/relocated metadata and exact archive identity enter this USB
collection. Its archive body is never nested, copied or recompressed here.
Neither the older accepted `ed4`/2946 predecessor nor the unrelated old
`56a66e`/`6785` archive is mixed with this 430 gate.

The independent closure audit accompanies this packet. The portable package's
verifier establishes byte/member closure only; inner reports keep these exact
development and physical qualification limits. Source fingerprints belong to
the executions that produced them. Later documentation/catalog fixes and the
separate accepted-430 metadata integration do not retroactively change an
older runtime or ARM source observation.

## Original planning correction

The preserved original `dcd-review/facts.md` contains superseded rejection text
about interval0/>255 and a nonzero bulk interval. That passage is historical
planning, not the final contract. The final plan, production, tests and current
documentation use no authored/profile/IR interval field: only an interrupt-IN
endpoint descriptor has bInterval1..255 linear FS frames. Existing bulk interval
handling remains unchanged. ST's proved bus-reset USBAEP-clear side effect is
not inferred for GD merely from shared register layout.

## Ordinary R1 compile failure

The first ordinary commit attempt passed all 2,955 fresh tool cases, then
`cc1plus` was killed while compiling the existing STM32 CAN factory test at
Ninja step 47/440. It executed no whole-Google suite and accepted no USB commit.
`ordinary-r1-package.json` references the separate failed-only raw archive; its
49 files were verified and actually restored against all 64 inventory entries.
The original 48-path index and 34-source freeze identify R1. A later evidence
addendum changes the R2 index, which must run the ordinary gate independently.
Generated documentation copies were subsequently archived, actually restored
and compared before retirement; the sealed development packet remains unchanged.

## Accepted interrupt-IN ordinary R2 successor

Commit `6b10c814c21d94b069f5690300152f904ea14d22`, tree
`a7109d16e669e0d015eb4f79978ca65cfbacce05`, passed the unchanged ordinary
and commit-message gates: 348 Google executables / 6,296 cases and 2,955 tool
cases, with zero failures/errors/skips. All 52 frozen paths and the original
34-source subset match the commit. `accepted-6b-package.json` references the
single successor archive containing 702 fresh commands and original raw
reports. Historical R1 attempts remain explicitly excluded. The exact prior
catalogue recovered as a reused input matches the fresh IO-plan hash; it is not
a reused test result. Verification and actual full restoration matched all
5,323 inventory entries. This is development host acceptance, separate from
later activation changes, formal clean-main qualification and physical HIL.
