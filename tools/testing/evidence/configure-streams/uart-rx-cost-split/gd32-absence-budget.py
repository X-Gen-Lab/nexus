from pathlib import Path
names=Path('/workspace/nexus-config-iteration-evidence/uart-rx-cost-split/gd32-nm.log').read_text()
assert "nx_gd32_uart_dma_ops" not in names, "RX-only face retains TX-DMA operations table"
