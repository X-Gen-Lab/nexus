---
id: reference.api.components.protocols.streams
type: reference
audience: consumer
source_language: en
---

(reference.api.components.protocols.streams)=

# Bounded protocol and user-interface sessions

These optional C11 mechanisms consume explicit bytes, existing byte/CAN faces
or selected display/input callbacks. The application supplies storage, a
serialized executor, protocol policy and finite deadlines. They create no
UART/socket owner, worker, device scan or automatic command replay.

| Target | Header under `nexus/components/` | Selected profile |
| --- | --- | --- |
| `Nexus::Framing` | `framing.h` | COBS encoding/decoding and bounded delimiter accumulation |
| `Nexus::WallClock` | `wall_clock.h` | Explicit UTC quality, age/drift uncertainty and 48-byte SNTP v3/v4 server-reply parsing |
| `Nexus::XModem` | `xmodem.h` | SOH/128-byte/CRC16-XMODEM receive session |
| `Nexus::ATClient` | `at_client.h` | One command over a caller-supplied `nx_byte_stream_t`, text/URC and explicit binary lengths |
| `Nexus::UARTByteStream` | `uart_stream.h` | Copied bytes over one existing UART request/RX owner, with explicit service and final TX drain |
| `Nexus::CANopen` | `canopen.h` | Fixed classic-CAN NMT slave, one heartbeat peer, expedited SDO and asynchronous RPDO/TPDO |
| `Nexus::CiA402` | `cia402.h`, `cia402_position.h` | Selected Profile Velocity and absolute immediate single-point Profile Position over the real CANopen node |
| `Nexus::LVGL` | `lvgl.h` | Single-owner RGB565 software GUI with caller arena/tile and settled display/input callbacks |

## Retain frame and response boundaries

COBS supplies message boundaries, without integrity or authentication. Its
incremental decoder discards overflow until a delimiter. Once ready, its frame
view borrows the decoder buffer until release; feed returns BUSY without
consuming a byte. Preserve that byte or stop reading upstream. If integrity is
required, validate the product-selected CRC before dispatching the payload.

ATClient copies an admitted command into caller TX storage, then service
advances partial stream writes and reads under the original absolute deadline
and explicit call budget. Response, URC and binary callback views expire when
the callback returns. Binary lengths come from reviewed module-specific
responses; there is no assumed module or prompt grammar. A timeout, malformed
line or stream failure can desynchronize the session after real command effects.
Recover software parsing only after the product reestablishes the actual module
boundary and joins transport users; never blindly replay the command.

Selected binary framing is decided before the next byte is consumed, including
when the header and binary body share one transport read. The exact
[ESP-AT device](../../../../../components/devices/esp_at/README.md) supplies its
reviewed grammar; the generic parser does not guess a modem personality.

## Copy UART bytes without replacing its owner

`Nexus::UARTByteStream` returns an existing `nx_byte_stream_t` over the selected
UART. A successful write copies a bounded prefix into caller-provided staging
and admits one inner request; it proves neither wire completion nor peer
delivery. No new write reuses staging before acquire-observed SETTLED. RX loss
or a short/failed TX closes admission. EMPTY UART RX means BUSY, never EOF.

The task explicitly advances UART-stream service and then the consuming AT
module. ESP-AT configure, join, connect, send and close preserve each original
finite deadline. `SEND OK` is module-stack admission. Remote CLOSED permits
draining copied RX to EOF but is terminal for that session: actual module/UART
reset and synchronization, joined users and explicit recovery precede another
configure. Stop continues cancel/service even after a latched fault and retains
the accepted UART request until SETTLED; it does not stop the outer UART/RX owner.

## Keep CANopen staging separate from product commands

Explicit `nexus_add_canopen()` creates `Nexus::CANopen` from the locked real
CANopenNode protocol objects. One task owns the existing started CAN port,
bounded RX/TX slots, private workspace and product OD values. Node ID, peer,
filters, heartbeat periods and maximum step gap are explicit. The profile has
one expedited 1/2/4-byte SDO server and one asynchronous PDO per direction,
each mapping 1..8 bytes; it does not select CiA402, CAN FD or authentication.

RX loss, mailbox overrun, wrong PDO length, bus-off, time discontinuity and TX
exhaustion close admission. Queued frames retain their first finite deadline.
Reset is reported and closes admission before SDO/PDO staging writes; no MCU
reset is executed. Product validation still checks complete command units,
sequence, epoch and validity before use. NMT Operational is not actuator enable.
Stop discards unsent copies and drains the original accepted CAN request,
including service after cancel failure. Reclaim workspace and OD only after
successful stop and joined callers; the outer CAN owner remains separate.

## Select a drive profile without granting actuator authority

Explicit `nexus_add_cia402()` adds the selected real standard OD bridge. Profile
Velocity and Profile Position own separate state and object arrays; ordinary
CANopen retains its generic application-object restrictions. The selected
asynchronous RPDO and TPDO each use seven bytes, with declared user-unit scaling.
No universal radian wire unit, SYNC, CSP/CSV, homing, relative point or queued
position sequence is implied.

Requested mode, actual mode, action requests and observed axis conditions remain
different facts. The application supplies current feedback and validates command
authority before applying a copied action. Network Operational and a successful
SDO/PDO do not enable a power stage. Heartbeat and repeated target values do not
by themselves renew a motion-command lease.

The selected position profile uses a rising controlword bit 4 for one new point.
Its acknowledgment bit 12 follows genuine matching axis admission, not reception.
Target-reached bit 10 requires the measured position and velocity windows for the
declared observation dwell; trajectory completion is not used as arrival. The
adapter's local generation is not a standard CANopen field and cannot prove
simultaneous multi-axis adoption.

Fault/reset, feedback loss, command validity and actual hardware withdrawal have
their own responsibilities. Fault reset requests recovery and never automatically
resumes an old point. Release only after the bound protocol has stopped and its
users have joined; it does not stop or deenergize the actuator. The
[selected profile contract](../../../../../components/protocols/canopen/README.md)
owns the exact objects, handshake and restrictions.

## Render only in the selected GUI executor

Explicit `nexus_add_lvgl()` creates `Nexus::LVGL`. One task owns GUI calls,
callbacks and the caller workspace, including the fixed 32 KiB TLSF pool;
there is no libc heap or worker. Only one live GUI owner is permitted.
Product UI implementation alone links `Nexus::LVGLProfile` to use selected
official label/button APIs; ordinary business code keeps Nexus/application types.

The caller supplies an aligned RGB565 tile, explicit elapsed milliseconds and
a finite service deadline. Settled flush callbacks release every tile reference
before returning, including errors. Deadline checks cannot preempt rendering
or widget callbacks. Errors close GUI progress but do not erase the panel's
recovery duties. Join all widget/event users before stop; successful stop
invalidates widget pointers and releases arena/tile references. Display, touch
and bus shutdown remain external. See the [selected GUI contract](../../../../../components/protocols/lvgl/README.md)
for pool-exhaustion fail-stop and complete scope.

## Separate local transfer from product update policy

XMODEM validates sequence, complement and CRC before each bounded 128-byte write
callback. A repeated previous block is acknowledged without writing it again.
The caller copies emitted control bytes into its own TX state, supplies
same-domain time to feed/poll, and enforces the finite session and packet
budgets. BUSY with a pending control byte does not consume new input.

Every callback receives 128 bytes including sender padding. Product length,
manifest, authorization, staging and signature/version checks determine usable
content and whether an update may proceed. Accepted blocks are not rolled back
by cancellation or failure. This profile has no checksum fallback, 1 KiB blocks,
YMODEM filename handling, Flash partition selection or reboot policy.

## Keep UTC quality separate from monotonic deadlines

WallClock does not modify Core time or scheduler deadlines. It combines an
explicit UTC sample with its monotonic observation, validity interval and stated
oscillator-drift bound. Ordinary SNTP results remain MEASURED; the product must
review a source before asserting TRUSTED. A parse result cannot bypass TLS's
trusted-time requirement.

The product validates the response source IP/port, original unpredictable request
token and unique outstanding request, and explicitly selects NTP era 0 or 1.
Uncertainty includes local RTT and server delay/dispersion, without proving
network symmetry, source honesty or physical timestamp accuracy. Invalid,
expired or discontinuous time must not become a fabricated usable UTC value.

Join callbacks and release borrowed views before resetting or reclaiming session
storage, then close the lower transport under its own contract. Software
protocol results do not establish physical serial/network timing or device
qualification; hardware HIL remains `not_executed`.

## Public declarations

### framing.h

```{doxygenfile} components/protocols/framing/include/nexus/components/framing.h
:project: nexus
```

### wall_clock.h

```{doxygenfile} components/protocols/wall_clock/include/nexus/components/wall_clock.h
:project: nexus
```

### xmodem.h

```{doxygenfile} components/protocols/xmodem/include/nexus/components/xmodem.h
:project: nexus
```

### at_client.h

```{doxygenfile} components/protocols/at_client/include/nexus/components/at_client.h
:project: nexus
```

### uart_stream.h

```{doxygenfile} components/runtime/uart_stream/include/nexus/components/uart_stream.h
:project: nexus
```

### canopen.h

```{doxygenfile} components/protocols/canopen/include/nexus/components/canopen.h
:project: nexus
```

### cia402.h

```{doxygenfile} components/protocols/canopen/include/nexus/components/cia402.h
:project: nexus
```

### cia402_position.h

```{doxygenfile} components/protocols/canopen/include/nexus/components/cia402_position.h
:project: nexus
```

### lvgl.h

```{doxygenfile} components/protocols/lvgl/include/nexus/components/lvgl.h
:project: nexus
```
