SPI Parent, Child and Queue Ownership
=====================================

The external
`spi_transaction example <https://github.com/X-Gen-Lab/nexus-examples/tree/main/apps/spi_transaction>`_
uses typed controller/child references, copied device configuration and explicit
sync/queued/cancel settlement. Native runs a finite behavior model. Discovery
provides the reviewed reference SPI fixture; other board wiring needs its own
review and physical setup.

Each child holds a parent lease and generation. A single original deadline
includes queue/bus contention and hardware operation. Queued requests borrow
buffers until terminal delivery. The application explicitly calls service;
cancel acceptance is not buffer return. Close/deinit rejects active operations
or callbacks and preserves ownership. Caller callbacks run outside metadata locks.

Selected STM32 SPI DMA must drain the real stream/IRQ/request before buffer
return; CCM is not DMA-reachable. GD32 SPI4 currently uses polling with bounded
reset/drain. This does not provide arbitrary DMA or SPI slave mode. Host models
and actual ELF do not qualify CS/waveform/IRQ timing; HIL remains unexecuted.
