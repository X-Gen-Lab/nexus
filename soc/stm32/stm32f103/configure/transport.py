"""Exact reviewed legacy transport integration spellings for stm32f103."""
from tools.configure.providers.legacy_transport import LegacyTransportProfile


PROFILE = LegacyTransportProfile(
    family="stm32f103",
    hook='NX_F103',
    dma_gate='RCC->AHBENR',
    dma_gate_bit='RCC_AHBENR_DMA1EN',
    tx='DMA1_Channel7',
    rx='DMA1_Channel6',
    flags='&DMA1->ISR',
    clear='&DMA1->IFCR',
    tx_vector='DMA1_Channel7',
    rx_vector='DMA1_Channel6',
    tx_shift=24,
    rx_shift=20,
    i2c_controller='I2C1',
    i2c_line_inputs='&GPIOB->IDR',
    i2c_line_mask=192,
    i2c_peripheral_mhz=8,
)
