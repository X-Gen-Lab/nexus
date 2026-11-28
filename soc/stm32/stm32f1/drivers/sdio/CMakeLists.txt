# Select the exact bounded F1 SD memory block provider.
# Author: Nexus Team
if(f1_sdio)
    target_sources(nexus_soc_stm32f1 PRIVATE transport.c sd_block.c)
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOBlock)
endif()
if("sdio-rx-dma-poll" IN_LIST f1_sdio)
    target_compile_definitions(
        nexus_soc_stm32f1
        PRIVATE NX_STM32F1_SDIO_4BIT=1 NX_STM32F1_SDIO_RX_DMA=1
    )
elseif(
    "sdio-poll-4bit" IN_LIST f1_sdio
    OR "sdio-poll-4bit-write" IN_LIST f1_sdio
)
    target_compile_definitions(nexus_soc_stm32f1 PRIVATE NX_STM32F1_SDIO_4BIT=1)
endif()
