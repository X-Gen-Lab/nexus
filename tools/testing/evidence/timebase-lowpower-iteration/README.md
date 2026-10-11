# Optional timebase lowpower development evidence

This is a dirty-source software development record. It does not qualify a clean
candidate, power consumption, physical latency, board wiring or hardware HIL.
The root normal hooks and later clean-source ARM/SDK qualification remain separate.

`manifest.json` binds current source digests and unchanged raw bytes. Final
`*-clock-review-freeze.xml` reports contain 50 executed Google cases: 41 new
common/binding/integration cases and nine existing GD32 Arch cases. The earlier
43-case freeze is retained as pre-independent-review history. The helper policy executes
eight stdlib unittest cases. One common case evaluates 15 continuously advancing
clock/phase/register-latency timelines. That finite model set is not an exhaustive
proof of every MCU timing execution.

Historical behavior RED is retained before each implementation/fix: common
unsupported stubs, both missing SoC bindings, actual shared timer ISR and stop,
failed control rollback/restore, and repeated pause of the first short interval.
Fixture compiler/include-order errors are distinct from expected behavior RED.
The first final Python command used the wrong import invocation and failed;
`policy-freeze-discover-command.json` records the corrected eight-case execution.
No raw logs or failing statuses were replaced. Some later historical RED commands
lack a full source snapshot; only the initially recorded common RED source digest
is asserted, and current digests do not retroactively identify earlier source.

Prestage whole-file checks run the maintained formatter/comment/text functions on
15 complete C/H files without baseline exceptions. They are development mechanics;
the normal indexed style and TDD hook still runs during root commit integration.
The initial indexed gate rejection for untracked files is retained, not bypassed
by staging in this agent.

`raw/arm-clock-review/report.json` records four fresh actual production firmware
CMake graphs: two SoCs, each default/custom versus timebase. All 25 commands
pass; strong selected getter, actual kernel Tick rename, no undefined symbols,
actual 16/52/24 B layouts and default zero new mechanism source/symbols are
checked. The specific empty-assembly retained-kernelstart, no-LTO Os fixture
adds 1532 Flash / 16 RAM bytes on STM32 and 1516 Flash / 16 RAM bytes on GD32.
These are fixture deltas, not universal application costs. ELF/BIN/map, raw
nm/size/disassembly, source digests, generated inputs and original compile
commands are preserved. Authored/generated probe source has `.raw` storage
suffixes without byte changes; reports retain original execution paths.

The first ARM attempt lacked Ninja in PATH and failed configure. Its report is
retained. Corrected-tool and pre-clock-review successful results are retained in
`arm-green-tools`; they are historical source, not final-source qualification.
`manifest-pre-clock-review.json` preserves the earlier source/case identity.
Neither failure nor prior result was overwritten. No hardware tests have run.
