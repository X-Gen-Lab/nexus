# Explicit W25Q128JV single-line SPI component

`Nexus::W25` is an optional allocation-free C11 component over the existing
polling `nx_spi_endpoint_t`. It creates no bus, worker, task, lock, device registry
or product partition. Vendor controller code and pin ownership remain behind
the selected Nexus SPI endpoint.

The reviewed software profile is **W25Q128JV**, 16 MiB, JEDEC `EF4018`, using
single-line 03h read, 02h page program and 20h 4 KiB sector erase. An explicit
profile is required. EF4018 is necessary but does not uniquely prove every
ordering suffix, FV/JV revision or physical fitting. Q64, EF7018 DTR, SFDP,
four-byte addressing, quad/QPI, status/protection writes, erase suspend/resume,
power-down and reset sequences are not selected. No identity fallback is used.

The source is Winbond **W25Q128JV Revision M, 24 December 2024**, Industrial and
Industrial Plus grades:

- [Official datasheet](https://www.winbond.com/resource-files/W25Q128JV%20RevM%2012242024%20Plus.pdf)
- SHA-256: `b51bb4303e17a17750c9cbcafc9a776dea842a8971d7cdd5edc70ff738efb1fe`
- Reviewed sections: power-up/write protection; status registers 1/2/3;
  03h/02h/20h/06h/04h/9Fh and read-status instructions; page and sector geometry.

The original downloaded PDF stays in execution evidence rather than source Git.

## Transport and ownership

The caller supplies one stable, awake, mode 0 or 3 single-line SPI endpoint at
no more than the 03h read limit of 50 MHz. The exact endpoint remains responsible
for whole-CS framing, hardware drain and its own retained recovery fault.
The component never calls shared bus reset or stop.

The SPI polling contract ends CPU access to all supplied buffers on every
return. It may still retain controller/pin recovery responsibility after a
failed CS withdrawal. W25 read/program therefore retain no caller payload
pointer after their synchronous return. A flash program/erase pulse is different:
component state, endpoint and clock remain live until later status progress
establishes quiescence. A transport error or timeout does not erase that duty.

Calls are task-only and serialized. The caller excludes foreign Flash commands,
pin/clock/power changes and concurrent bus access. Endpoint, ops and opaque
contexts remain live and disjoint from mutable state and payloads. Known object
spans and the single byte at each nonnull context address are checked. Complete
opaque context extents remain the caller's responsibility; this is not a memory
sandbox for arbitrary context pointers.

## Bounded operations

| Operation | Selected bound and meaning |
| --- | --- |
| `nx_w25_init` | Read-only probe of an explicitly declared profile, region and power/clock facts; no reset/wake/unlock |
| `nx_w25_read` | 1..252 bytes, one 03h CS interval; failed/short reads leave output unchanged |
| `nx_w25_program_start` | 1..252 bytes inside one physical 256-byte page; SUCCESS means command issued, not completed |
| `nx_w25_erase_4k_start` | Exactly one aligned 4 KiB sector inside the caller-selected region |
| `nx_w25_service` | At most four short status/disarm exchanges, no busy-wait loop |
| `nx_w25_get_status` | No-I/O observation of pending duty and the last mutation outcome |
| `nx_w25_stop` | No-I/O unbind only after pending/uncertain duty is gone |

The four-byte command/address header plus a 252-byte payload fits the maintained
256-byte polling endpoint bound. It does not silently send a 260-byte full-page
transaction. The caller explicitly splits larger work at region/page boundaries.
Offsets are relative to the nonempty, erase-aligned region provided by the
product; no region is reserved or selected by the platform.

The local byte arrays are 512 bytes for read and 256 for program, plus the
actual call stack. These source bounds are not measured task stack peaks.
No maximum-object pool, heap buffer or retained payload copy is introduced.

The component admits writes only when the observed BP field, CMP and WPS are
zero. It does not change protection or QE. WREN must be observed latched before
an array command. An idle WEL after a supposedly issued array command is reported
as an error and disarmed with WRDI plus status readback. WIP0 with SUS1 is never
treated as completed; no automatic resume is attempted.

## Deadlines, failure and completion

The caller establishes the real power-up/write delay and supplies its absolute
`ready_at`. Initialization before that time returns BUSY without SPI. The clock
must already be live; a numerical zero is not clock-readiness evidence.
Clock domain/epoch labels are declarations, not runtime attestation.

Mutation deadlines include later internal Flash busy time. Service receives a
separate finite cleanup deadline; extending that probe budget never extends the
original operation deadline. BUSY retains duty, while status can already expose
a latched operation timeout. A new cleanup budget may establish later quiescence
and return the original timeout/error. Backward/nonfinite clock observations
during pending duty retain storage for external system recovery.

A failed WREN may leave the device armed, even when no array command was sent.
A failed program/erase transfer may already have changed Flash. The status
`may_have_modified` flag records that possibility; it is not a byte-count proof.
Unexpected observed activity also closes new admission until status service
proves quiescence. A timeout never releases state or cancels an irreversible
program/erase pulse.

Command completion is not byte verification, atomic replacement or power-loss
durability. The caller obeys NOR 1-to-0 programming semantics and provides erased
targets or a separately reviewed update policy. Readback/CRC, erase verification,
crash recovery, wear management and product layout remain external. A recovered
idle device cannot reconstruct the outcome of an interrupted write.

## Board and evidence boundary

The uploaded core-board schematic marks W25Q128 as unpopulated. The high-board
W25Q128 heading conflicts with its 64 Mbit note. Neither declaration selects this
JV profile automatically; actual part/fitting and voltage require confirmation.

Host protocol models and ARM/SDK links establish different software properties.
No physical Flash timing, endurance, electrical operation or power-cut behavior
is established by this component or its tests. Physical HIL is not executed.
