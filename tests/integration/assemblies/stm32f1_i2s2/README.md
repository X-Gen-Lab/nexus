# STM32F103ZE I2S2 software reference

This bare-metal fixture selects Philips I2S master TX on PB12 (WS), PB13
(CK), and PB15 (SD). It uses SPI2 and DMA1 channel 5 exclusively, with
stereo 16-bit halfwords and a single finite request. The typed factory
consumer checks the negotiated storage layout and nominal clock rate.

With HSI at its nominal 8 MHz, the divider is 8 and ODD is 0. The frame
rate is exactly 15,625 Hz in the configuration arithmetic. The 16,000 Hz
request therefore requires at least 23,438 ppm of nominal tolerance.
Oscillator error, electrical timing, and codec interoperability require
physical measurement.

The register model verifies DMA completion, the serializer tail,
timeouts and cancellation while retaining the caller's frame loan until
DMA, the interrupt, and the wire are idle.
