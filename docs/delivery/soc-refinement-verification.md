# SoC refinement execution record

This is the historical `3728b334` baseline. Later functionality and execution
status are recorded separately in [SoC extensions](soc-extensions-verification.md).
The hashes and counts below do not qualify those later changes.

This record covers the [55-item SoC ledger](../design/soc-refinement-execution.csv).
It distinguishes implemented mechanisms from the exact source and execution
scope of each result. Subsequent source changes do not inherit earlier results.
All physical tests remain `not_executed`.

## Committed software gates

The following commits passed ordinary staged hooks, including the unchanged-index
TDD gate and commit-message checks. No test filter, skipped-only report or hook
bypass establishes acceptance. Counts describe their individual execution scopes;
they must not be added together as a final-platform test total.

| Commit | Change | Recorded software execution |
|---|---|---|
| `e7deeecdb2dfc85d40f2f557cd655cfa34e1492d` | Exact-package GPIO inventory and bounded G474 UART | Normal commit gates; source-specific RED/GREEN retained |
| `7e48b879b327df19c6d7dd7f672a5898585bdb2c` | Nineteen SoC HIL case recipes and captured-evidence validation | Normal tool gates; plans remain prepared and unbound |
| `1f5ff587f00ce91882ffd455982f50191bc7907f` | Startup, clock, memory and interrupt cleanup ownership | 156 GoogleTest executables / 2,819 cases; 480 tool and 385 configuration tests |
| `ae27e999f157fecd027db99bdfa99ce38582a940` | Reviewed original H7/U5 CMSIS dependency closures | 156 GoogleTest executables / 2,819 cases; 527 tool and 385 configuration tests |
| `ee33f38d8120405b5bf6f383710f81d05b12371b` | Immutable descriptors, cold admission, EXTI and timer start | 157 GoogleTest executables / 2,847 cases; 576 tool and 397 configuration tests |
| `5899c230c8063eb8832bb1dc7f171958d83227d5` | Runtime timer ACK isolation, retained cleanup and narrow W1C models | 157 GoogleTest executables / 2,862 cases; 576 tool and 397 configuration tests |
| `c3c4a62ebdbd3661bb561392651e80c23dd5b1e3` | H743/U575 static GPIO/UART references, SDK closure and HIL guards | 163 GoogleTest executables / 2,982 cases; 585 tool and 415 configuration tests |
| `23cde7e57a4781db8ffe2e75849c02f976c4304d` | Selected GPIO leases and terminal time-domain failures across five MCU families | 165 GoogleTest executables / 3,040 cases; 585 tool and 423 configuration tests |
| `3728b334b02eb2eb70bcf5d4caafbb9746837dd3` | Complete private pin-helper dependencies and aligned startup DATA loads | 165 GoogleTest executables / 3,040 cases; 585 tool and 428 configuration tests |

All numerical suites above completed without failures or skips. Their staged
source-tree identities and raw hook outputs are retained in the external
`nexus-soc-20261012/evidence/batch-*/` and
`nexus-soc-qualification-20261012/batch-*-hook-passed/` records. An earlier failed hook remains a
failed attempt; its output is not rewritten as a successful run.

The dependency-only commit `ae27e999f157fecd027db99bdfa99ce38582a940` additionally
passed 47 actual source-SDK boundary tests with zero failures or skips. Source
and dependency state were clean before and after execution. Relocated public
consumers exercised Native C/C++, the existing platform ARM profiles and ThreadX
within their declared scopes. This run did **not** qualify H743/U575 firmware.
Its raw command receipts and consumer artifacts are recorded in
`nexus-soc-qualification-20261012/sdk-dependency-boundaries-v2/`.

## Baseline source qualification

The implementation qualification source is
`3728b334b02eb2eb70bcf5d4caafbb9746837dd3`, tree
`48ba820e5c2d4c372bf7eb6e909c49850cc1fe76`. Subsequent documentation commits
describe that exact code source; they do not replace its qualification identity.
The [portable result index](soc-refinement-results.json) records exact report
hashes and archive-relative paths without copying raw logs into Git.

Normal hooks passed on that source. The same complete model execution binds
203 reviewed I/O properties to assertion anchors and current production inputs;
this is bounded executed evidence, not a formal or exhaustive proof. All five
maintained ARM gates passed, covering sixteen memory/reference images and
87 selected-provider images. The same code source passed all 58 installed SDK
boundary tests. Its 236 actual commands include expected nonzero negative-path
results; the test suite has zero failures, errors or skips. Earlier results,
including 58 SDK tests and 22 legacy contract programs on `23cde7e`, are retained
as source-specific history and are not promoted to the new source.

| Scope | Maintained execution entry | Final-source status |
|---|---|---|
| Production C, generated factory and failure paths | `scripts/ci/tdd_gate.py --all` | 165 executables / 3,040 cases; zero failures or skips on the code source above |
| Configuration, provider descriptors and tools | `tools/testing/run_tool_tests.py` | 585 tool / 428 configuration cases; zero failures or skips on the code source above |
| F407 default/CCM and GD main/additional/TCM RAM | `scripts/ci/stm32_memory.py`, `scripts/ci/gd32_memory.py` | Eight actual ARM images passed resources and selected retention |
| H743 empty/selected, baremetal/FreeRTOS | `scripts/ci/stm32h743.py` | Four actual ARM images passed |
| U575 empty/selected, baremetal/FreeRTOS | `scripts/ci/stm32u575.py` | Four actual ARM images passed |
| Every maintained authored controller assembly | `scripts/ci/provider_assemblies.py` | All 87 actual fixtures passed; clean source and unchanged inputs |
| Installed SDK and actual public consumers | `tests/contracts/test_source_sdk.py`, `test_device_sdk_payload.py` | 58 actual tests passed; clean source before/after; relocated Native, selected MCU and ThreadX consumers |
| Hot paths and image/resource costs | `tools/measurement/soc_costs.py` | Eleven representative final profiles observed: eight H743/U575 plus F407 EXTI, GD EXTI and G474 LPUART |
| Derived capability projection | `tools/maintenance/capabilities.py --check` | 87 rows checked; actual exit code zero |
| External examples on the same platform code | `X-Gen-Lab/nexus-examples` | Eight CTest programs, three GoogleTest programs / 43 cases, 20 ARM images and budgets; ten HIL plans remain prepared and unbound |
| Offline reproducibility and candidate sealing | `tools/evidence/reproduce.py`, `candidate.py`, `proof.py` | Not established by this iteration record |
| Physical SoC cases 37–55 | `tools/hil/soc_acceptance.py` | Tooling implemented; physical execution absent |

The H743 ELF gate inspects the actual reset-to-power-entry closure before RAM is
usable, in addition to vector bytes, selected retention and resources. The U575
gate preserves the declared TZEN=0 single-state boot boundary. Its vector/count
checks do not automatically prove the complete Reset/SystemInit/data/BSS/MSP
call closure. Independent review has now checked all eight actual H743/U575
images, including the complete linked startup path and four-byte DATA load
alignment; 2,297 identified inputs stayed unchanged before and after capture.
The gate's `source_and_call_closure_review_required` boundary is closed by that
separate source-bound report, not by relabelling the vector test. Neither ARM
gate executes a scheduler on the MCU or establishes a measured electrical result.

The actual selected-reference costs are below. Flash is the sum of LOAD file
bytes, not the largest occupied address span. RAM includes reserved MSP exactly
once; every row reserves 2,048 MSP bytes and zero heap. Static reservations are
not measured stack high water or product application budgets.

| Family | Backend | Assembly | Flash LOAD bytes | RAM reserved bytes |
|---|---|---|---:|---:|
| H743 | baremetal | empty | 2,437 | 2,104 |
| H743 | baremetal | GPIO + UART | 8,908 | 2,364 |
| H743 | FreeRTOS | empty | 7,564 | 3,716 |
| H743 | FreeRTOS | GPIO + UART | 14,108 | 3,976 |
| U575 | baremetal | empty | 2,225 | 2,112 |
| U575 | baremetal | GPIO + UART | 8,712 | 2,372 |
| U575 | FreeRTOS | empty | 7,300 | 3,724 |
| U575 | FreeRTOS | GPIO + UART | 13,868 | 3,984 |

The selected H743 and U575 images expose 60 and 68 bytes respectively of
GPIO lease/gate objects. These are not total Platform storage. An inlined or
unretained helper without a separate function symbol is not assigned zero cost.
These observations describe exact linked selections; different backends,
capacities or routes are not comparable optimization percentages. No cycles,
interrupt blackout time or WCET was measured.

The same observer also inspected three existing images from the completed
87-fixture matrix without recompiling them:

| Assembly | Selected storage | Flash LOAD bytes | RAM reserved bytes |
|---|---|---:|---:|
| F407 VE EXTI | Two lines, four event slots each | 6,152 | 2,344 |
| GD32 EXTI | One line, eight event slots | 6,956 | 2,260 |
| G474 GPIO/LPUART | 64 RX bytes | 9,012 | 2,316 |

These RAM figures also include the 2,048-byte MSP reservation and zero heap.
The supplementary report records exact symbol sizes, decoded instructions and
static branch sites. Different routes and capacities make these separate
baselines. The eleven measured profiles do not represent every maintained mode
or establish cycle counts, blackout duration or a cross-chip optimization rate.

External examples are qualified at code commit
`f02b91867bb64abfa31e3c1d4b6004bd1a38b091`, pinned to the platform code above.
The actual 46-command clean execution and its source record are indexed
separately. The ten HIL preparations do not mean ten physical station executions.

## Interpreting the ledger

`implemented` records a mechanism in source. `contract_implemented` and
`documentation_updated` record narrower contract/documentation work.
`qualification_record_authority` delegates acceptance to the actual external
record for the exact source/configuration/artifact tuple; it is not a source
file's self-reported pass. `final_source_pending` means that execution is still
required for the completed source scope.

`selected_profiles_observed` records actual static cost measurements only for
the profiles named above and in the index. `tooling_software_validated` qualifies
the HIL software contracts; the corresponding physical test remains unexecuted.
The partial-reference implementation states preserve their limited GPIO/UART
scope even after host, ARM or installed-SDK checks pass.

SOC-05 records versioned datasheet, manual and errata provenance. Actual silicon
revision and full per-mode errata applicability remain unqualified. SOC-23 and
SOC-27 require observed actual ELF costs; an observer's unit tests alone do not
complete measurement. A changed capacity, backend, Board route or memory layout
must be reported as a distinct selection rather than a like-for-like saving.

SOC-28, SOC-30 and SOC-31 contain only the exact H743/U575 GPIO/UART/time reference
subset. SOC-29 and SOC-32–36 retain explicit missing features. Their original
exit conditions remain unchanged. A rejected unsupported mode is not a completed
mode implementation. SOC-37–55 contain executable software preparation,
admission, capture and validation tools; every physical exit remains open until
real station records are captured and validated.

No result here establishes MCU cycles, interrupt latency, Flash power-loss
behavior, cache/DMA coherence, TrustZone isolation, product safety, certification
or a product release. Exact support remains defined by the
[support matrix](support.md) and [SoC contracts](../design/soc-refinement.md).
