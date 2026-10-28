"""Exact reviewed legacy transport integration spellings for gd32f303."""
from tools.configure.providers.legacy_transport import LegacyTransportProfile


PROFILE = LegacyTransportProfile(
    family="gd32f303",
    tx='&DMA_CH3CTL(DMA0)',
    rx='&DMA_CH4CTL(DMA0)',
    flags='&DMA_INTF(DMA0)',
    clear='&DMA_INTC(DMA0)',
    tx_vector='DMA0_Channel3',
    rx_vector='DMA0_Channel4',
    tx_shift=12,
    rx_shift=16,
    i2c_controller='I2C0',
)
