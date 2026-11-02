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
For an external Board, both `init` and `list-bindings` accept `--board-package`
instead of `--board`. The command resolves that package path from the working
directory; `init` writes it relative to the new TOML directory. Moving the whole
product tree preserves this relationship. The reviewed Board package remains
separate from the product assembly; use the [Board porting guide](board-porting.md)
for package contents and provider boundaries.

The explanation lists public faces, modes, clock, resources, memory domains,
declared budgets and execution duties. `list-bindings --json` includes
`mode_contracts`, resource claims, `fact_sources` and `limitations`. A binding is
still `configuration-required`: backend, clock and all selected instances must
resolve together. Missing physical PCB facts remain unknown. MCU queries state
`not_established_by_configuration`; Native states `not_applicable`.

Each candidate mode lists its own `resource_keys`, `occupied_resources` and
`resource_scope`. A polling mode does not claim the alternative DMA route.
Board-first queries retain known SoC and Board reservations without inventing an
OS owner before a backend is selected. Assembly queries read facts matching the
resolved input hashes. Options can add requirements, so this projection does not
replace full-assembly admission. Use the [exact support query](../delivery/capabilities.md)
to separate configuration acceptance from model, ARM, SDK and physical evidence.

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

The diff also groups `impacts` into `factory_ids`, `public_interface`,
`transfer_contract`, `execution` and `storage`. Each item identifies its effective
path, before/after value and review obligation. Changed or reordered factory IDs
may require consumer changes; a new executor or DMA mode needs its lifecycle and
storage
contract reviewed. These are declared configuration impacts, not measured timing,
memory savings or an assertion that a consumer was migrated.

A conflict diagnostic identifies the authored file/key and competing resource
owners. Correct those inputs, then configure again. Never fix generated C,
weaken a DMA reservation or change a clock implicitly to make a build pass.
Configuration budgets are declarations; measure the actual linked image before
claiming resource savings. See the [assembly reference](../reference/configuration/assembly.md)
and [support matrix](../reference/platforms/support.md).

Automation can add `--diagnostics-json` to receive one structured rejection on
standard error, with exit code 1; successful standard output keeps its existing
format. Human and JSON diagnostics use the same error record. Unknown/missing
fields preserve their context and key sets; resource conflicts identify the
resource and competing claims, including owner, mode and source. These records
locate authored inputs without silently correcting them.
CLI argument syntax errors retain argparse diagnostics and exit code 2.

## Preview the maintained software plan

The plan/artifact commands, Settings, wait selection and serial extensions in
this iteration require a development checkout containing those changes. They do
not modify the published `v0.2.1` archive; installed-SDK support follows actual
verification of the corresponding new SDK.

From the fixed development checkout, inspect the existing complete software plan
before starting expensive validation:

```sh
python tools/dev/dev.py plan \
    --output-root /tmp/nexus-planned-run-001 \
    --json /tmp/nexus-plan-001.json
```

Choose a new JSON path in an existing parent directory; the future run directory
is not created. The preview derives ordered commands, deadlines, checker and
preset metadata, and input identities from the maintained plan. It does not
execute tests, install tools, discover test cases, use equipment or access the
network. Case counts remain unknown, HIL preparation is not acquisition, and
qualification remains false. `--preset` filtering is rejected; use the existing
focused test command for development. Remove only your own preview JSON when no
longer needed. See the [development tool contract](../../tools/dev/README.md).

After a successful build, inspect its captured outputs without guessing which
ELF belongs to the selected configuration. Replace the path below with that
build's actual `quality/build-provenance.json`:

```sh
python tools/dev/dev.py artifacts \
    --provenance build/my-board/quality/build-provenance.json \
    --json /tmp/nexus-artifacts-001.json
```

The command verifies the provenance and resolved identity before listing source,
configuration, artifact roles and the original command receipt. Missing, changed,
stale or mismatched inputs reject rather than becoming a successful artifact
view. The optional JSON path must be new. Inspection neither builds nor accesses
equipment, and does not grant the current checkout, software or hardware a new
qualification.

## Use optional Settings and networking mechanisms

Select `settings` in the assembly's component list to link `Nexus::Settings`.
The caller first opens Storage, then supplies Settings metadata, a disjoint
staging buffer, entry capacity and a product-defined schema. Integer keys and
copied byte values stay bounded. Get/set/remove operate on staging; only explicit
commit writes Storage, and close discards uncommitted changes. Unknown schemas
or corrupt loads are errors, not empty writable stores. After a mutating commit
failure, reopen Storage and Settings to observe the surviving commit before
retrying. Defaults, migration, authorization and Flash regions remain product
policy. Read {ref}`settings.h <settings_8h>` and the
[component contract](../../components/settings/README.md) for detailed ownership.

The optional `Nexus::LwIP` target admits the organization-hosted `X-Gen-Lab/lwip`
2.2.1 gitlink `77dcd25a` and its fixed source closure. It is excluded from the
default build until explicitly selected. It uses `NO_SYS=1`: a product loop or
task advances one serialized owner, without an implicit RTOS stack port. It
reuses Ethernet frame loans and bounded memory. The reviewed slice covers static
IPv4/ARP/ICMP/UDP and limited TCP integration; complete TCP connection lifecycles,
DHCP, DNS, IPv6, VLAN, TLS and MQTT remain separate exits. Read the
[network integration contract](../../components/network/README.md) for source,
SDK, clock, memory and stop prerequisites. Source admission and development SDK
consumption do not establish a software candidate, full network-platform,
ARM or physical acceptance.

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

For several authoritative predicates sharing one wait port, `nx_wait_select`
checks a caller-owned source array from an explicit cyclic start index. It
returns an observed-ready index without acquiring or consuming that source.
The caller chooses fairness by advancing the next start index, wires every
publisher to the same port, and owns service/timer progress. Stop and join
publishers and waiters before reclaiming their state. There is no implicit
subscription, dispatch, worker or resource transfer; the public {ref}`wait contract <wait_8h>`
still governs arm/recheck and the absolute deadline.

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
image-qualified observation. Monitor uses the explicitly selected host-supported
standard baud rate with 8N1 and finite duration; 115200 remains the default.
Unknown rates or a TTY configuration mismatch fail rather than selecting a
different speed. Host acceptance does not prove the physical device baud rate.
EOF or disconnection fails. Debug binds loopback. Failed or cancelled device
operations retain their locks for explicit inspection; normal completion releases
them. A successful daily operation does not qualify HIL. Until real equipment is
connected, record physical execution as `not_executed`. The
[performance and HIL guide](performance-hil.md) owns capture and qualification.

For one bounded Linux batch, add `--client` to `debug`. This uses the official
GDB/OpenOCD pipe transport and rejects `--port`; the default debug server keeps
its loopback TCP workflow. The target JSON must explicitly identify the GDB
executable in `gdb_client.tool` with `path`, `size` and `sha256`. Optional
`gdb_client.commands` uses the same file identity fields and replaces the default
`info threads` command. Review that file as executable operator input. GDB starts
with automatic scripts disabled, loads the exact provenance-bound ELF, attaches,
runs the selected commands and detaches. Tool/configuration/ELF identities are
checked before and after execution. The finite timeout covers the owned process
chain; failure or cancellation withdraws it and retains station quarantine.
No executable or target is auto-discovered. See the
[development tool contract](../../tools/dev/README.md) for the equipment schema
and command example. Host pipe fixtures do not establish physical probe support.

Development monitor remains Linux/POSIX. The session-bound terminal phases are
`acquiring`, `owned`, `releasing` and `released`. After confirmed helper
withdrawal, the parent may restore its originally held descriptor only when
`owned` proves this session's exclusive ownership. An already `released` helper
does not authorize another restore or exclusive-release operation: another
process may now own the terminal. Unknown, corrupt or intermediate phases,
unconfirmed withdrawal and identity failures retain quarantine without touching
another owner's terminal. The parent never opens a replacement by-id device.

Primary I/O failure and cleanup failure remain separately visible, including
their exception cause/context chains; a cleanup exception must not hide an
earlier EOF or read failure. Independent cleanup errors remain in the operation
record. External SIGKILL of the parent cannot guarantee restoration. The
baud-rate extension does not expand formal HIL console admission or establish
Windows/macOS support. See the [serial ownership regressions](../../tools/testing/test_dev_serial_ownership.py)
and [rate/cancellation regressions](../../tools/testing/test_dev_serial_rates.py).
