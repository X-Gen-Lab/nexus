# Installed secure API consumers

These two public Source SDK scenarios are part of the single versioned consumer
plan. They do not start a broker, open a socket, download data or install tools.
Only a public synthetic CA is supplied; neither a client identity nor a private
key is included. Products supply their own trust, time and provisioning policy.

The Native Linux scenario obtains words with `getrandom(GRND_NONBLOCK)` and uses
the platform's actual `CLOCK_MONOTONIC` domain. Unavailable or interrupted entropy
returns BUSY under finite deadlines; other failures return IO without a fallback.
Its explicit fixture UTC is not a product clock authority. It successfully opens
the PSA/runtime and TLS state, observes one bounded handshake WANT, initializes
and deinits MQTT, aborts TLS and stops the runtime. A software MAC retains the
normal bounded lwIP pools and exercises adapter start/service/stop; it never
accepts TX, acquires RX or brings a link up. This does not establish an
authenticated TLS connection or a broker exchange.

The F407 scenario derives from the independent repository ARM service fixture.
It uses the reviewed Qiming F407ZG `hse8-pll168` assembly and explicit RNG
component. Its public application calls normal TLS and MQTT APIs; its composition
supplies exact static storage and retains the default bounded lwIP pools. The
transport is unconnected and UTC is explicitly untrusted. Source SDK verification
only compiles and links it, checks actual CPU/ABI/C11 options, resource budgets,
configuration identity and live implementation symbols. It does not execute the
RNG, TLS or network on a device. Its budgets are 512 KiB Flash, 112 KiB static
RAM, 8 KiB main stack and zero libc heap; a build exceeding them fails.

Both scenarios compile the ordinary API caller separately from storage
composition. Only composition opts into `Nexus::TLSStorage`,
`Nexus::CoreMQTTStorage` and the lwIP layout. Ordinary callers inherit no supplier
headers or profile macros. Canonical inputs, actual compile rows, image identity
and the installed SDK prefix remain bound to execution results. Successful
software linking does not measure stack peaks, latency or physical behavior.

The independent N09 host fixture verifies authenticated TLS/mTLS and a real broker
exchange. Its results and source identity are separate; these installed consumer
scenarios do not inherit that qualification.
