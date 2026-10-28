"""Exact reviewed legacy transport integration spellings for stm32f103."""
from tools.configure.providers.legacy_transport import LegacyTransportProfile


PROFILE = LegacyTransportProfile(
    family="stm32f103",
    tx='DMA1_Channel7',
    rx='DMA1_Channel6',
    flags='&DMA1->ISR',
    clear='&DMA1->IFCR',
    tx_vector='DMA1_Channel7',
    rx_vector='DMA1_Channel6',
    tx_shift=24,
    rx_shift=20,
    i2c_controller='I2C1',
)
