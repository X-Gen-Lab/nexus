# DMA controller isolation CPU fixture

This host-only header selects the four-bit BASEPRI/ceiling-5 CPU policy used
by real STM32F407/GD32F470 UART/SPI providers in GoogleTest. It never selects
Board wiring, a product assembly or a generated factory.

The tests map the real peripheral addresses and execute the normal provider
NVIC paths. Only CPU/NVIC, DMA W1C/enable withdrawal and selected hardware reset
are modeled. Error injection happens at the real disable or W1C leaf; it never
replaces provider admission, cancellation, service or settlement.

Fresh unfiltered regular and ASan/UBSan reports establish software isolation.
Strict Cortex-M4F SDK objects establish compilation and layout costs; consumer
factory ELF tests and physical HIL establish separate scopes. No host model
establishes silicon timing, electrical behavior or Board support.
