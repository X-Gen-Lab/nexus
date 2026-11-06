# Explicit GPIO I2C and SCCB

`Nexus::SoftwareI2C` provides a caller-owned, synchronous two-wire controller.
`Nexus::SoftwareSCCB` selects the same library with a distinct protocol API.
There is no task, heap, runtime discovery or long global interrupt mask.

The immutable configuration supplies two already configured open-drain GPIO
outputs, external pull-ups, actual input readback and one monotonic clock.
Writing one releases a wire; it must never drive a push-pull high. The selected
minimum half-period and finite polling bound constrain each wait. They do not
establish a measured bus frequency, voltage or electrical qualification.

Strict I2C exposes the existing `nx_i2c_endpoint_t` with an immutable addressed
child. It accepts 7-bit addresses `0x08..0x77`, up to eight messages of up to
256 bytes each, repeated START between messages and one final STOP. Payload
counts exclude address phases. NACK is reported; it is not silently ignored.
Async methods are unsupported.

SCCB uses a separate child and register API. Its write identifier is an even
8-bit value, with explicit register8/data8 transactions. Reads perform the
documented STOP-separated sub-address and read cycles, then master NA. The
master releases and ignores the target's ninth Don't-Care bit; successful
waveform completion does not prove sensor presence. SCCB never weakens the
strict I2C face or becomes a generic ignore-NACK fallback.

Every synchronous return ends CPU access to all payload spans. A failed STOP
separately retains the wire owner, configuration and pin responsibility. One
serialized task owns calls and the clock. Arbitration loss releases the lines
without forcing STOP. Before explicit recovery, the caller must reestablish
exclusive ownership and join foreign writers. Recovery attempts one bounded
STOP; it does not perform nine-clock bus clear, reset or transaction replay.
The deadline-free controller recover method is unsupported.

Call the budgeted stop API and check its result before releasing the outer GPIO
lease or configuration. A later numerical deadline cannot repair a failed or
discontinuous clock domain. No API promises multi-controller operation.

The V4.1 camera PD6/PD7 control and audio PC0/PC13 control are GPIO pairs rather
than hardware I2C routes. This mechanism makes those wires usable by explicitly
selected device adapters; it does not select or initialize an audio codec or
camera. Source review identities are in `references.json`. Physical HIL remains
`not_executed`.
