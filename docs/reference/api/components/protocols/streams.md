---
id: reference.api.components.protocols.streams
type: reference
audience: consumer
source_language: en
---

(reference.api.components.protocols.streams)=

# Bounded framing, time and byte sessions

These optional C11 mechanisms consume explicit bytes or the existing byte-stream
face. The application supplies storage, a serialized executor, protocol policy
and any finite deadlines. They create no UART/socket owner, worker, device scan
or automatic command replay.

| Target | Header under `nexus/components/` | Selected profile |
| --- | --- | --- |
| `Nexus::Framing` | `framing.h` | COBS encoding/decoding and bounded delimiter accumulation |
| `Nexus::WallClock` | `wall_clock.h` | Explicit UTC quality, age/drift uncertainty and 48-byte SNTP v3/v4 server-reply parsing |
| `Nexus::XModem` | `xmodem.h` | SOH/128-byte/CRC16-XMODEM receive session |
| `Nexus::ATClient` | `at_client.h` | One command over a caller-supplied `nx_byte_stream_t`, text/URC and explicit binary lengths |

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
qualification; hardware HIL remains `not_executed`. CANopen and additional exact
device profiles remain `not_selected`.

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
