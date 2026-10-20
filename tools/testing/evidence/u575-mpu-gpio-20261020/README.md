# Exact U575 MPU-v2 scalar GPIO software evidence

This is a staged development increment over `b031911584800898f1cdc29a7d96afd0f8335b2b`,
not a clean-source release or hardware qualification. STM32U575ZIT6Q TZEN0/MSIS4,
Cortex-M33 MPU-v8/eight regions and one immutable PC7 WRITE grant are the only
selected reference. M7 remains unsupported. Firmware was linked, never run.

The original host phase executed 15 cases against the stub before production,
then exposed four actual provider-bound/startup defects in an expanded 29-case
run. The canonical 29-case baseline passed. Removing an unexposed redundant
control counter was a reviewed behavior-preserving refactor: its final 28 cases
passed both the isolated real-C model and the maintained Native CMake target.
The first maintained Native build genuinely failed for a missing Arch include;
the one-line fixture closure fix was then built and all 28 cases ran without
filters or skips. Historical source identities remain separate.

Policy 11, five-domain memory 5, selected derivation 8, existing port regression 6
and the schema regression passed. The SDK policy/selection phase passed its
complete 90-case pure-Python scope, followed by the final 27 Selection cases after
a test-only deduplication. These results do not execute a complete SDK image plan.

The persistent existing retention inspector gained a selected-profile audit.
The original 12-case run failed 46 actual subtests against the old inspector.
Independent review then found that linear BL presence did not prove reachability;
seven real early-return/vector/initialization-plan subtests failed before the
bounded startup-shape correction. The final complete four maintained modules
executed 122 cases, with zero failures, errors or skips. The final `unique_symbol`
doc and test EOF corrections are explicitly recorded as text-only successors.

Normal firmware and relocated SDK results are distinct actual executions:

| Context | Flash loaded | Flat BIN / Flash footprint | SRAM1 including 2048-byte MSP | SRAM4 |
|---|---:|---:|---:|---:|
| Historical 24 B authority: primary normal strict C11 M33 link | 15310 B | 24880 B | 6008 B | 0 B |
| Historical 24 B authority: relocated development-SDK public C11 consumer | 15606 B | 24880 B | 6008 B | 0 B |
| Final 16 B authority: primary normal strict C11 M33 link | 15306 B | 24880 B | 6000 B | 0 B |
| Final 16 B authority: relocated development-SDK public C11 consumer | 15662 B | 24880 B | 6000 B | 0 B |

The final actual target ABI is task 864 B and authority 16 B. Moving the existing
private authority epoch before its handle removes ARM padding; names, alignment
and lifecycle stay unchanged. This was a reviewed refactor, not a new RED. Fresh
maintained Native 28, normal M33 link and a new actual installed consumer passed
against header `5a4ce3b72aa49ae36ec2f77b8a8d2db0a5c09352754939424d882146681e5a9f`.
The dynamic production `.equ`/ELF audit reads 864/16 without hardcoded object sizes.
The earlier addition of GLOBAL/NOTYPE/ABS sizeof facts changed ELF metadata only:
its historical 24 B phase retained the exact flat BIN and resource metrics. A separate actual ARM object
measurement using the same minimal kernel options and `user_io=none` reports
StaticTask_t 844 B and task 848 B; selected minimal task is 864 B, a measured 16 B
metadata increase in that context. The real default syscall object has 280 B,
70 slots and only slot6/13 relocations, with no GPIO symbol. This narrow ABI/object
measurement is not a complete default normal-firmware footprint comparison.
The earlier standard-profile default object has different TCB options and is
recorded separately, without subtracting its sizes from the selected image. Reference reservations are
24 KiB privileged Flash, 256 B syscall Flash, 1 KiB user Flash, 8 KiB privileged
RAM and 2 KiB user RAM; unassigned tails are not mapped to user. These are explicit
software reference budgets, not a minimum or product partition policy. The link contexts above use different paths and are not a cross-context
isolated cost experiment. No latency or runtime isolation result is inferred.

The installed consumer used maintained source-SDK export with
`--development-fixture`, physically moved the complete prefix, verified it,
called the installed `prepare_selected_consumer`, linked through normal
`find_package`/`nexus_add_firmware`, ran installed `check_selected_image` including
the persistent retention audit, and verified the prefix again. The 5988-file
final 16 B snapshot is
`4920468cd6e7b4e36bb80b9ca7e708e379142a6af21a3feab0aacb80fe0d9f1d`;
`publishable=false`. The source and root index stayed unchanged during execution.
The complete v9 transport plan was not executed by this single-consumer check.

The ELF audit checks complete linker/LOAD extents, readonly authority pointers,
actual compiled sizes, three Nexus wrapper branches, real SVC6/13/70 veneers,
exact 71-slot table bytes, and explicitly supported GNU Os normal startup bodies
with their actual call targets and user DATA/BSS plan. It is not a general
transitive call-graph proof or runtime MPU isolation proof. Every raw phase,
source digest, independent review and final artifact is kept in the associated
immutable evidence manifest/archive; later source heads require new execution.

Runtime exception entry/exit, task switching, MPU faults, PC7 output, interrupt
latency and HIL were not executed. General IO-39/SOC-36 exits, other devices,
multiple grants/tasks and full delivery/candidate gates retain their own criteria.

The historical SDK snapshot
`79e23ad25e6ddb774544262bb3ccaf93975abbca6f1b98ac2ee35743ca044bf2` binds the
24 B authority header `21dfbecb…`. The later `28f7031d…` successor corrects only
the join return comment; the final `5a4ce3b7…` successor reorders private fields.
The new 16 B SDK chain above was executed anew, not inherited from either earlier
source phase. SDK public C files use actual `-std=c11`; the source contract adds
strict warning flags separately, so this does not claim SDK `-Werror`.

The checked-in `package.json`, `verification.json` and `delivery.json` bind one
compact `raw/92257cb843c656664141900d84007c740c443289bd4378a5e31eba150942cce2.tar.gz` copy.
Use the existing `tools/evidence/store.py` verifier/restorer with an explicit
`--archive` after checkout. Restored `raw-manifest.json` maps 558 role/path records
to 385 unique blobs; archive membership and each blob digest are verified.
This preservation does not re-execute the original checkers or establish HIL.
