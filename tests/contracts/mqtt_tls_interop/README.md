# Native TLS and MQTT interoperability

This Linux host test connects the C11 Nexus TLS/coreMQTT adapters to a real
Mosquitto 2.0.22 broker. POSIX nonblocking sockets are a test-only byte-stream
port. The test does not exercise lwIP, a MAC/PHY, MCU entropy or a board, and does
not grant release or physical qualification.

Build `client.c` as a Native executable linked to the selected
`Nexus::CoreMQTTStorage` and `Nexus::TLSStorage`, using the maintained ABI, build-options,
strict-warning and target-policy targets. It accepts an explicit loopback port,
case name and generated fixture directory. Neither firmware library creates a
worker: the client drives TLS handshake/service/close and MQTT process/keepalive
in one serialized executor with absolute monotonic deadlines and finite budgets.
The host entropy port explicitly uses Linux `getrandom`; UTC comes from the host
calendar. These choices establish no product entropy or trusted-time policy.

The host runner requires the managed local Docker daemon, OpenSSL 3 and the
already available image below. It never automatically pulls a host tool or adds a
firmware dependency. Keep the existing proxy/CA settings and Docker registry
configuration when obtaining the immutable image:

```sh
env -u DOCKER_HOST -u DOCKER_CONTEXT -u DOCKER_TLS \
    -u DOCKER_TLS_VERIFY -u DOCKER_CERT_PATH \
    docker --host=unix:///var/run/docker.sock pull \
    eclipse-mosquitto@sha256:09b62ff32c2e7b64d4b431b9a89236073289b68db273de2b089edeb9b0d92b5c
python -B -m tools.testing.mqtt_tls_interop \
    --client /absolute/path/to/native-client \
    --output-root /absolute/path/to/new-output-directory
```

The output parent must exist. Each run creates a new private directory and fresh
P-256 CA/server/client test credentials valid for two days. Keys remain local
host fixtures; never add the generated output to Git, a Source SDK, firmware or
manufacturing credentials. Only the broker's own CA/certificate/key and config
are mounted read-only. The container uses the caller UID/GID, drops capabilities,
has a read-only filesystem and bounded resources, and publishes a random port
only on `127.0.0.1`.

Three real-broker cases run:

- Mutual TLS 1.2 with ECDSA authentication, MQTT clean-session CONNECT/SUBACK,
  QoS1 publish/self-echo/PUBACK, UNSUBACK, DISCONNECT and locally accepted TLS
  close-notify. Local close does not prove the peer consumed the final alert.
- A real socket shutdown after the QoS1 frame drains. The client requires a sticky
  I/O failure and observes zero subsequent transport calls when traffic is tried
  again. The application still owns semantic recovery for uncertain delivery.
- Missing client identity. Success requires the TLS public diagnostic to report
  the actual peer fatal alert, rather than an arbitrary timeout or entropy/I/O
  failure. Raw broker logs retain its concrete certificate rejection reason.

The report records image digest/ID, broker binary hash/version, OpenSSL identity,
client binary hash, test credential hashes and raw command receipts. Cleanup
checks full container ID, exact name and owner label in one snapshot, then uses
only that immutable ID. Unknown ownership fails without touching the object.
Controlled repeated cancellation is deferred through bounded withdrawal; external
SIGKILL or a lost Docker daemon cannot guarantee cleanup. `owner.json` preserves
the unique recovery identity. No global stop, prune or name-only removal occurs.
Primary and cleanup failures remain separate in the report and cause failure.

The small Python unit tests cover cleanup after a withdrawn Docker create client
and repeated controlled cancellation. Their Docker model is host-tool evidence;
it contributes no broker, TLS or hardware qualification. Run them with:

```sh
python -B -m unittest tools.testing.test_mqtt_tls_interop
```

The separate [complete client-chain fixture](../lwip_mqtt_interop/README.md) uses
`--transport lwip-test-relay` and its own helper to exercise the actual
lwIP/TCP/TLS/MQTT client chain through a test-only ciphertext relay. That mode
has five cases and distinct scope; it does not change these default three POSIX
cases or turn their evidence into lwIP or hardware qualification.
