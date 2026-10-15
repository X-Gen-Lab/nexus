# HIL platform tooling iteration

This archive binds tooling regressions to their authored source and raw command
bytes. It does not qualify a software candidate or any physical station.

The maintained workload inventory has 21 protocols. The eight additions cover
UART RX DMA, direction/flow adapters, continuous ADC, CAN, raw Ethernet, USB FS
device, timer capture and advanced PWM break. Their stimulus, instruments,
electrical review and numerical limits remain station-owned. The authoring
template supplies none of those facts.

Six negative cases first failed against the production hash-only admission path.
They replace the workload with an opaque document or an unreviewed budget and
recompute all related hashes. Acquisition, metric validation and OS acceptance
now use the shared reviewed-content validator and reject those records. The
acquisition cases also confirm that no station process or lock was opened.

The final source-bound run executed all 136 cases across the nine HIL tool and
contract modules, with no failures or skips. It includes host subprocess/PTY
regressions and actual ARM linking in the existing admission fixture suite.
All 17 files owned by this change remained identical throughout that run. This
is development evidence; the final clean repository must run its normal gates.

The historical `raw/full-hil-green.log` filename does **not** indicate a pass:
that attempt had three errors in an old partial resolved-configuration fixture.
The subsequent final logs record the complete canonical fixture and actual
successful execution. Raw files, including failed attempts, remain unchanged.

The three original physical fixture bytes are unchanged. Their actual PCB
revisions, station identities and measurements remain unknown. G474 preparation
uses a mandatory software-reference scope, which physical station admission
rejects. ThreadX HIL protocols remain unsupported. Newly described bus and
waveform protocols do not automatically execute equipment or expand the existing
validators into a complete physical protocol audit.

See `manifest.json` for source identities, immutable raw navigation, exact
commands and the limits of this evidence. All physical results are not executed.
