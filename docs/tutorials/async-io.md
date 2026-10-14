---
id: docs-tutorials-async-io
type: tutorial
audience: consumer
source_language: en
---

(docs-tutorials-async-io)=
# Observe rejection, borrow, cancellation and drain

Run this host tutorial to observe the real public SPI API against a Native provider.
It demonstrates software ownership and failure paths, not physical DMA timing.
The complete executable fixture is the maintained GoogleTest target; its model
controls stand in for external IRQ and wire progress only. GoogleTest/GoogleMock
and C++17 are host-only; production firmware remains C11.

## Build and run the complete fixtures

From the repository root, with host tools and pinned dependencies available:

```sh
python tools/dev/dev.py configure --preset native-debug
python tools/dev/dev.py build --preset native-debug --parallel 4     --target nexus_native_async_test nexus_native_factory_test nexus_uart_owner_mock_test
```

Run each binary without a filter and retain fresh XML in a new directory:

```sh
mkdir -p /tmp/nexus-io-tutorial
build/native-debug/bin/nexus_native_async_test     --gtest_output=xml:/tmp/nexus-io-tutorial/async.xml
build/native-debug/bin/nexus_native_factory_test     --gtest_output=xml:/tmp/nexus-io-tutorial/factory.xml
build/native-debug/bin/nexus_uart_owner_mock_test     --gtest_output=xml:/tmp/nexus-io-tutorial/owner.xml
```

All three commands must exit zero and report passing, nonzero test counts. These
complete target executions are a tutorial scope, not the all-target commit TDD
qualification. Remove the tutorial directory only after retaining needed reports.

## Follow the normal and rejected admission path

```{literalinclude} ../../tests/google/native_async_test.cpp
:language: cpp
:start-at: TEST_F(NativeAsync, SPIBorrowWaitsForWireCompletionAndOtherEndpointRejects)
:end-before: TEST_F(NativeAsync, FixedDummyAndDiscardOnlyTouchTheirExplicitCells)
:caption: Complete maintained test: same-controller rejection and wire completion.
```

The first endpoint accepts its request. The caller keeps the request and buffers
alive. The second endpoint returns `NX_ERROR_BUSY` while its request stays READY;
that rejection retains no borrow. Stepping payload transfer does not publish
SETTLED before the simulated wire-completion step. Service and IRQ progress remain
explicit; a waiting client cannot assume a hidden worker.

## Follow cancellation and failed drain

```{literalinclude} ../../tests/google/native_async_test.cpp
:language: cpp
:start-at: TEST_F(NativeAsync, SPIQuarantineRetainsBorrowAndStopUntilDrain)
:end-before: TEST_F(NativeAsync, UARTIDLEBlocksStayImmutableAndLossPrecedesLaterData)
:caption: Complete maintained cancellation fixture; held drain retains the borrow.
```

Cancel requests termination. With drain deliberately held, the request becomes
QUARANTINED, stop returns BUSY and prepare rejects reuse. The model then supplies
actual drain progress; service publishes SETTLED and result observation exposes
the cancelled result and transferred prefix. Removing the model hold is a test
control, not a generic production recovery API.

The generated factory fixture also tests platform stop with a quarantined UART
borrow. The UART owner fixture checks rejected submissions and retained payload
against GoogleMock dependency ports. Read [ownership](../explanation/ownership.md)
before copying request storage into an application.

![Request admission, drain, quarantine and final settlement](../assets/diagrams/request-lifecycle.svg)

FIG-06 expresses current Core helper transitions with provider proof obligations.
The [I/O lifecycle matrix](../design/io-lifecycle-matrix.md) limits
what these models establish. IRQ hints, stable slot epochs, observer detach and
join require the [OS shutdown guide](../guides/os-shutdown.md).
