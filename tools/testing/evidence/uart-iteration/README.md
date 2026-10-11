# UART iteration development evidence

`manifest.json` selects actual production RED executions and the final fresh
build plus nine GoogleTest binaries: 120 tests, zero failures. The new receive
and RS485 binaries account for 81 cases; the remaining 39 are existing modes
and owner regressions. Original argv, return codes, raw logs and XML are copied
without rewriting historical paths. All intermediate attempts remain in `raw/`.
Five compiler logs use `.log` extensions to retain their original whitespace
bytes; their SHA256 values are unchanged. Failed builds and stale executable
results are explicitly excluded from accepted
RED/GREEN facts. A signal crash is recorded as an actual failed development
execution, without inventing missing XML or a passing case count.

This is dirty development evidence, not a clean source or hardware candidate.
The final own-source hashes describe the software freeze; they do not bind an
earlier RED to this final source. Global candidate checks must run again against
the committed graph and installed SDK. No physical board was connected.

The hardware model now implements DATA-read status clearing, W1C flag capture,
bank-address writes and disable refusal. It is still a model: it cannot prove
wire timing, DMA arbitration, latency or electrically correct CTS/RTS/DE wiring.
See `docs/design/uart-dma-rs485.md` for required ownership and timing contracts.

Intermediate actual ARM GC measurements are retained externally under
`/workspace/nexus-config-iteration-evidence/uart-rx-cost-{old,split}`. Both paths
contain the same RX-only fixture configurations, original ELF/map/nm/size output
and command/artifact identities. The split fixture removed retained TX-DMA ops:
STM32 text 10084→7774 bytes, GD32 text 12106→9844 bytes, and bss decreased by
8 bytes on each. These measurements precede the final terminal-error changes;
the configuration owner performs a separate fresh final-mode ARM matrix.

That final matrix subsequently passed all fourteen production mode fixtures.
After the terminal-error fixes, RX-only text is STM32 7866 and GD32 9932 bytes,
with no retained `nx_*_uart_dma_*` symbol. Compared with the same old fixture,
the final reductions are 2218 and 2174 text bytes; bss still decreases by 8
bytes each. The final graph records are in the separate configure-streams
evidence bundle, not inferred from the intermediate ELF.
