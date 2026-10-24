---
id: docs-guides-developer-workflow
type: how-to
audience: consumer
source_language: en
---

(docs-guides-developer-workflow)=
# Configure a product and make I/O progress

Start with the [external application tutorial](../tutorials/source-sdk.md).
Use the same fixed SDK or checkout for the commands below. Commands run from its
source root; product TOML, tasks, buffers and recovery policy stay in the product
repository. The resolver reports supported software choices, not electrical
verification of a connected board.

## Choose and compare a complete configuration

Inspect choices before authoring a complete assembly. This query requires no
guessed backend or clock:

```sh
python tools/configure/configure.py list-bindings \
    --board stm32f407_qiming_v31 --kind uart
python tools/configure/configure.py init \
    --board stm32f407_qiming_v31 --backend baremetal --clock hse8-pll168 \
    --profile uart --binding uart1 --instance link0 \
    --output /tmp/product-uart.toml
python tools/configure/configure.py explain --assembly /tmp/product-uart.toml
```

`init` refuses an existing file. Profiles `empty`, `gpio` and `uart` write visible
TOML values and run the normal resolver; nonempty profiles require an explicit
binding. A rejected route is not replaced with another one. Read the generated
TOML before adopting its baud rate, event capacity, IRQ priority and budgets.
For an external Board use `--board-package` when querying bindings; its reviewed
package remains separate from the product assembly.

The explanation lists public faces, modes, clock, resources, memory domains,
declared budgets and execution duties. `list-bindings --json` includes
`mode_contracts`, resource claims, `fact_sources` and `limitations`. A binding is
still `configuration-required`: backend, clock and all selected instances must
resolve together. Missing physical PCB facts remain unknown. MCU queries state
`not_established_by_configuration`; Native states `not_applicable`.

After editing a separate next assembly, compare effective changes:

```sh
python tools/configure/configure.py diff \
    --assembly /tmp/product-uart.toml --against /tmp/product-uart-next.toml
python tools/configure/configure.py explain \
    --assembly /tmp/product-uart-next.toml --json
```

Both assemblies must resolve. The diff reports effective platform, route, mode,
endpoint, storage and budget changes; renaming a source file alone is not a
behavior change. JSON distinguishes absent fields from explicit `null`. Scripts
that previously consumed default JSON from `explain` must now pass `--json`.
Editor hints are assistance; the resolver remains the acceptance authority.

A conflict diagnostic identifies the authored file/key and competing resource
owners. Correct those inputs, then configure again. Never fix generated C,
weaken a DMA reservation or change a clock implicitly to make a build pass.
Configuration budgets are declarations; measure the actual linked image before
claiming resource savings. See the [assembly reference](../reference/configuration/assembly.md)
and [support matrix](../reference/platforms/support.md).

## Assign one executor and retain borrowed storage

The factory returns an existing typed face. Platform start, request admission,
progress, completion, recovery and shutdown remain separate operations. Choose
one execution recipe explicitly:

| Recipe | Progress owner | Storage and shutdown responsibility |
| --- | --- | --- |
| Main loop | The loop calls the selected service steps; fixed IRQ entries perform bounded IRQ work | Keep request and buffers alive through settlement; stop admission, cancel or drain, observe settlement, join observers, then stop |
| Exclusive task | One product task owns direct operations; declared IRQ policy permits only its selected OS interaction | Wake and service explicitly; a task timeout does not revoke a borrowed buffer or permit task-stack destruction |
| Shared bus owner | Callers submit bounded slots; one owner services the controller and endpoints | Queue capacity, callbacks and storage are explicit; stop admission, drain accepted slots and controller references, join observers, then destroy the owner |

These are application choices, not three hidden driver workers. Product examples
live in [nexus-examples](https://github.com/X-Gen-Lab/nexus-examples):
`templates/{native,qiming,sky,liangshan}`, `apps/shared_spi_runtime` and
`apps/stream_recovery`.
Use their README with the fixed examples revision associated with your platform.
The SPI Native example and FreeRTOS POSIX-host exercise are separate scopes;
Native Platform does not accept `backend = "freertos"`. A software two-CS fixture
does not establish a second physical CS on a launch board.

For finite requests, REJECTED retains no caller references. ACCEPTED borrows
request and transfer storage until acquire-observed SETTLED. Cancel and deadline
expiry request progress toward withdrawal; they do not settle the request.
QUARANTINED preserves storage and cleanup obligations. SETTLED does not imply a
successful transfer or that callbacks and observers have already joined. Read
[ownership](../explanation/ownership.md) before placing asynchronous storage on a
stack or stopping a worker.

For streams, a consumer releases each acquired block explicitly. Slow consumers
can cause loss or stop on backpressure according to the selected mode; providers
must not overwrite outstanding loans. Stop may return BUSY until loans and
hardware references drain. Restart uses a new valid lifecycle after complete
stop; clearing counters is not recovery. The H743 continuous-mode exercise is a
separate exact ARM configuration, not a claim that the three launch boards have
identical continuous-mode wiring.

## Diagnose facts without changing ownership

`nx_io_diagnose` in `nexus/io/diagnostic.h` composes copied request and stream
observations with explicitly known lifecycle, owner, service-time and observer
facts. It neither reads live MMIO nor invokes service or recovery. Its independent
finding bits can coexist; `unknown_facts` preserves missing information. No
findings does not certify health.

A request that stops advancing does not prove the service loop was omitted.
`SERVICE_OVERDUE` requires finite timestamps in the same monotonic domain and a
positive declared maximum gap; reversed time rejects with STATE. Owner admission
fields are known only after a successful owner diagnostic snapshot. BUSY alone
does not prove a full queue. Outstanding stream blocks can be producer, ready or
consumer loans; the ordinary cursor says nothing about a private preposted slot.
Application escalation still owns retry, safe outputs and reset policy.

`nx_dma_storage_validate` checks the complete explicit reservation separately
from its payload. For example, a 17-byte payload may use a caller-owned 64-byte
reservation aligned to 32-byte cache lines. The entire reservation must already
fit its declared DMA-visible domain and whole-line ownership. The helper does
not round, allocate, bounce, perform cache maintenance or replace provider mode
admission. Deadline expiry never permits reusing the reservation before drain.

The optional owner observation adds `accepting`, `queued` and `slot_available`.
Recompile consumers because its public observation/sidecar layout changed.
Default owner and provider structures do not gain these fields. Measure optional
sidecar storage for the selected ABI; it is not assumed to be free.

## Plan daily device operations separately from qualification

For flash/debug, use an identified equipment file and provenance from the actual
successful build. Monitor may run without build association:

```sh
python tools/dev/dev.py flash --target /path/equipment.json \
    --provenance /path/build-provenance.json --elf /path/application.elf
python tools/dev/dev.py debug --target /path/equipment.json \
    --provenance /path/build-provenance.json --elf /path/application.elf
python tools/dev/dev.py monitor --target /path/equipment.json \
    --provenance /path/build-provenance.json
```

The default prints a plan. Actual execution requires `--execute --output` with a
new owned directory. Flash/debug require matching board/part, image, probe serial
and pinned tool/configuration bytes; they do not require a serial console.
Monitor requires the board, stable serial by-id and station lock, but may omit
build provenance: that result states `image_linkage = unassociated`, not an
image-qualified observation. Monitor uses 115200 8N1 and finite duration; EOF
or disconnection fails. Debug binds loopback. Failed or cancelled device
operations retain their locks for explicit inspection; normal completion releases
them. A successful daily operation does not qualify HIL. Until real equipment is
connected, record physical execution as `not_executed`. The
[performance and HIL guide](performance-hil.md) owns capture and qualification.
