UART Tickets and Settlement
===========================

The maintained external
`uart_echo example <https://github.com/X-Gen-Lab/nexus-examples/tree/main/apps/uart_echo>`_
uses typed received events and TX tickets. The
`log_uart example <https://github.com/X-Gen-Lab/nexus-examples/tree/main/apps/log_uart>`_
uses caller-owned bounded logging storage and an explicit pump/flush owner.
Which targets are created depends on actual provider capabilities in that source.

Submit borrows TX storage until a settled poll result or the provider's successful
cancel contract. Memory completion is not wire TC/idle. Deadline consumes the
original queue/operation/wait budget. Cancel/timeout/recover failure preserves
storage and ownership for retry. Successful submit with zero ticket is a provider
fault requiring recovery, not proof of completion.

Board-selected pins, IRQ and any optional transceiver DE are separate reviewed
resources. MCU UART DMA is unsupported. Native timestamps/byte capture and vendor
fault models do not qualify real TC/DE/electrical timing. Boards are deferred.
