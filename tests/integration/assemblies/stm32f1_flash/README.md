# STM32F103RB internal Flash reference

This opt-in assembly links the F103xB page-oriented Flash driver under HSI8.
The consumer reads metadata and one vector byte and stops the platform. The
reference image issues neither program nor erase pulses.

The exact NUCLEO-F103RB Board binding records a nominal 3300 mV supply.
The driver admits the 2000–3600 mV range from DS5319 Rev 20, Table 28.
The Board value is a design assumption for programming. Full-chip geometry
is 128 KiB, with 1 KiB pages and 2-byte program units. Products declare their
own Flash regions.

The separate GoogleTest model exercises real production program and erase code,
including ES096 delayed BSY, EOP provenance, timeout settlement and retained
cleanup. The longest specified single-page pulse is 40 ms under DS5319
conditions. The HSI8 timebase has a 65.536 ms maximum single-overflow interval.
The consuming product must account for Flash stall and worst-case IRQ latency
within this bound. No HIL execution or physical timing is inferred here.
