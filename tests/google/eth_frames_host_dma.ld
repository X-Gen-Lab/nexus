/* Test-only placement of the exact production static DMA arrays. All metadata
 * remains ordinary host storage. Does not widen any production DMA domain. */
SECTIONS
{
  /* Claim the exact inputs before the default .bss.* wildcard, then restore
   * its location so unrelated host metadata does not migrate into MCU SRAM. */
  __nx_eth_frames_host_bss = .;
  .nx_eth_frames_tx 0x20001000 (NOLOAD) : ALIGN(32)
  {
    KEEP(*(.bss.s_nx_eth_frames_tx_mac))
  }
  .nx_eth_frames_rx (NOLOAD) : ALIGN(32)
  {
    KEEP(*(.bss.s_nx_eth_frames_rx_mac))
  }
  .nx_eth_frames_storage (NOLOAD) : ALIGN(32)
  {
    KEEP(*(.bss.s_nx_eth_frames_storage_mac))
  }
  . = __nx_eth_frames_host_bss;
}
INSERT BEFORE .bss;
/* Each section contains one exact generated array. Sanitizers may append
 * redzones; actual symbol sizes independently establish 256/256/2048 bytes. */
ASSERT(SIZEOF(.nx_eth_frames_tx) >= 256,
       "The exact eight-descriptor TX array must be present")
ASSERT(SIZEOF(.nx_eth_frames_rx) >= 256,
       "The exact eight-descriptor RX array must be present")
ASSERT(SIZEOF(.nx_eth_frames_storage) >= 2048,
       "The exact eight-row RX payload array must be present")
ASSERT(ADDR(.nx_eth_frames_storage) + SIZEOF(.nx_eth_frames_storage) <= 0x20010000,
       "Generated DMA storage must not overlap caller payload or leave main SRAM")
/* The existing STM32 model declaration is the same physical GPIOA window
 * used by the shared System/pin oracle, never a second GPIO object. */
PROVIDE(g_nx_stm32_gpioa_model = 0x40020000);
