# Exact F407 finite I2S2 slave-TX development evidence

This reference selects STM32F407VET6 or STM32F407ZGT6, baremetal,
HSE8/PLL168, external CK/WS and one finite stereo16 block. These are staged
development observations over commit `6032222cb099d7f5b4410b18f57f375126e87d7d`,
not a clean-source candidate or physical qualification. Firmware was linked,
not executed with a physical master, codec or Board.

Native executed all 35 current provider/System cases without failure or skip;
selection executed 28, coherent SDK-verification 42 and release-tool 57 cases.
The latter two total 99, not a complete v10 image-plan execution. The canonical
two-frame layout regression genuinely failed against the predecessor before
contiguous-row admission passed. Earlier 33-case history is retained separately.
Strict Wshadow and harness-setup failures are original rejected attempts, not
functional TDD RED; the test-only local-name correction is a separate phase.

Component retention executed 17 new checks within the complete 94-case scope.
The original actual CLI rejected a caller-owned face as no selected faces;
the corrected normal VE and ZG CLI audits each proved one caller face, one
read-only operations table and all four concrete methods. The audit proves
static retained ELF bytes, not runtime permissions or hardware behavior.

| Complete normal link context | Flash loaded | Flat BIN / footprint | SRAM including 2048 B MSP |
|---|---:|---:|---:|
| VE selected I2S2 | 10544 B | 10544 B | 2448 B |
| ZG selected I2S2 | 10544 B | 10544 B | 2448 B |
| Omitted default, no I2S symbols | 2979 B | 2980 B | 2112 B |
| One relocated development-SDK VE consumer | 10596 B | 10596 B | 2448 B |

These are complete linked configurations, not isolated driver-cost differences,
latency, stack high-water measurements or audio-rate characterization. The
consumer uses the public C11 interface; this does not claim SDK `-Werror`.

The single SDK execution used maintained export, physical prefix rename,
installed verification, the installed selected-consumer helper, public
`find_package`/normal firmware graph, selected-image audit and verification again.
The source and index observations before and after were equal. The 5999-file
snapshot is `acfc33470487eee4f5d1a118ba7b8eeed5847668291ea06e769909f1b1dab77c`,
with `publishable=false`; its consumer ELF is
`8211ecc0f6edcbca8a520bee704f34c25040681109d3ffbbf123dd9d945f7d59`.
This one VE consumer does not execute the full v10 50-root/53-image plan.
The complete SDK/build bytes were preserved, verified, independently restored
and compared before owned expanded copies were reclaimed; that preservation
is not a new compile or a portable execution claim.

## Original source-host receipts

The following absolute paths are original host locators, not checkout-relative
portable payloads. Hashes identify the retained actual receipts; final portable
qualification retains its own admission. No raw build tree is duplicated here.

| Actual receipt | SHA-256 |
|---|---|
| `/tmp/nexus-f407-i2s2-root-arm-evidence-20261020/execution.json` | `653b40d44cbf65108b037d1d444f291ef0841ffaf57772cf1d0c8d36bee277ba` |
| `/tmp/nexus-f407-i2s2-root-focused-r3-20261020/execution.json` | `a6c776371347a70441d1d7424761797fc98ee9e097a0ee9c988b5fa657b6a378` |
| `/tmp/nexus-f407-i2s2-root-arm-evidence-20261020/actual-retention-green.json` | `70ec3c9f666b859e2010ff60a016e72fa9a0c0d3aaf76c2b979fc932a91e8f41` |
| `/tmp/nexus-f407-i2s2-installed-sdk-evidence-20261020/execution.json` | `cfe70c49cc09395ca5e3207ad73ad10e1c7ea8cdbb46db8a9f7c5a211890ef90` |

API extraction/publication, the whole bilingual site and the final source commit
remain pending at this source-documentation preparation phase. Full clean-main
formal validation, the complete SDK plan, two fresh network-disabled OCI builds,
software-candidate sealing and physical HIL require their own actual evidence.
IO-41 stays `in_progress`/`not_qualified`; its original continuous-audio,
SAI/PDM and hardware exits are unchanged.


## Compact archive and later validation phases

The source-bound text above is retained byte-for-byte (4023 bytes, SHA256
`84764e76aaf64b962e082448ec282ea6f143692e535c577799b0a4dcce571e96`).
This index adds later evidence; it does not relabel earlier source snapshots.

| Archive directory | Preserved evidence | Scope |
|---|---|---|
| `author-phased` | Original260 raw phase files, functional RED/GREEN and rejected setup/mixed-source attempts | Original identities/classifications unchanged |
| `root-normal-arm`, `normal-artifacts` | VE/ZG selected and omitted/default normal public ARM graphs, exact ELF/BIN/map/resource/resolved/compile metadata | Linking and static retention, no physical execution |
| `root-strict-original`, `root-strict-r2`, `root-strict-r3` | Original Wshadow compile rejection, successful corrected build/harness setup failure, then35 Google+28 selection+42 SDK+57 release passed | Compile/setup failures are not functional RED |
| `retention-original93`, `retention-successor94` | Original93 and legal4-byte ops-reference RED→94 complete cases,17 new tests, exact patches and root integration | Both actual normal VE/ZG audits then pass |
| `installed-sdk` | Eight critical artifacts, manifest/snapshot/source bindings, actual commands/postcheck and external archive receipts | One relocated development VE SDK consumer; full v10 plan not executed |
| `mpu-static-fix` | Explicit16 sanitized remote members, equivalent bc193→cc643 fix, independent review,28 Google, fresh normal MPU ARM+retention | Earlier603 and installed-SDK source/resource phases remain separate |
| `focused-tidy-r1-harness-setup`, `focused-tidy-r2` | Missing-environment setup error and10 distinct successful current analysis contexts | Original setup failure remains rejected |
| `documentation-preliminary` | Actual first Doxygen0/gettext1 and repaired gettext0, both127-POT outputs | First multiline-link warning was not suppressed or relabeled |
| `documentation-final` | Initial18-file doc patch, supported-link successor,12 final catalogs and107 independently reviewed new pairs, FIG30 PNGs/review and source-bound README |98 contract/API/ledger pairs plus9 navigation/atlas pairs |
| `documentation-fullsite` | Completed actual Doxygen/gettext/English/Chinese commands, raw streams, source/translation reports and127 POTs | Strict maintained bilingual development site passed; ordinary full index gate pending |

The final strict site passed all127 source pages and complete bilingual
translation admission. All four maintained commands exited0 with process
withdrawal complete. The1626-file frozen-input superset remained unchanged;
it includes unused source and does not assert all1626 files render. Its48
external dependencies, the original dirty/staged source identity over6032222,
and qualification=false remain explicit. No whole HTML/XML tree is included.

The distinct MPU static-analysis successor has loaded Flash15310B (the prior603
phase recorded15306B), flat BIN/footprint24880B, SRAM6000B including2048B MSP
and heap0. Its fresh normal ELF is
`70969a20482387fce9a19d47f9325461f8d7ddfa51ac4cfcd5a5c656fb801db5`.
These later observations do not overwrite the old MPU resources or the old
F407 SDK snapshot.

Existing full SDK and build archives4696d459… and9933432… are referenced by
their original full restore/inventory verification receipts, not duplicated
with vendor payloads here. No current dirty source is substituted for an
older execution phase. The archive collector ran no compiler/configure/tests
and changed no primary source/index. Final ordinary full-gate acceptance,
formal clean main qualification, the complete v10 image matrix and physical
HIL are not established by this archive.

Portable raw archive: [raw/0a799290296991780a20b967d2cda38d64ee0f4c41e5370e6f369a97cd1e39b0.tar.gz](raw/0a799290296991780a20b967d2cda38d64ee0f4c41e5370e6f369a97cd1e39b0.tar.gz).

Archive SHA256: `0a799290296991780a20b967d2cda38d64ee0f4c41e5370e6f369a97cd1e39b0`; 4356681 compressed bytes, 1137 files, 21363564 uncompressed bytes. The maintained-store package descriptor and verification receipt accompany this archive.


## First ordinary-gate failure and test-only followup

The first ordinary gate executed 1366 tools/testing cases:1365 passed,1 failed,
0 errors and0 skips. Later stages were not executed in that attempt. The real
filesystem observer test assumed capacity admission when actual free storage
was below the unchanged default 384 MiB requirement; production storage.py
`7eca920d…` correctly refused admission and remains unchanged. The independently
reviewed test-only patch preserves real observer binding and adds deterministic
default-budget refusal coverage; the complete storage test module then passed
all 21 cases without failure or skip. This does not establish full-gate success.

Storage recovery separately archived, fully restored and inventory-compared
256 files / 135990659 B before retiring only the owned untracked normal generated
examples build. Source/Git remained untouched; observed /tmp free changed from
376504320 B to 486637568 B. External recovery archive e0c92b1b… is referenced,
not copied with build/vendor payloads. The original 1137-file 0a799290 archive
is unchanged. No documentation rebuild is inferred or performed for this
test-only successor; the 1626-input rendering superset is unchanged.

Sidecar raw archive: [raw/b287922234afd92fbfda7a583b3dd9b85e90193a154a0d189af4ce4562ccbbb3.tar.gz](raw/b287922234afd92fbfda7a583b3dd9b85e90193a154a0d189af4ce4562ccbbb3.tar.gz).
SHA256: `b287922234afd92fbfda7a583b3dd9b85e90193a154a0d189af4ce4562ccbbb3`. Ordinary complete R2 remains
**pending**; full v10 execution and physical HIL remain not executed.
