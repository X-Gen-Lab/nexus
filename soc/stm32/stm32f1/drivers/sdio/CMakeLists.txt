# Select the exact bounded F1 SD memory block provider.
# Author: Nexus Team
if("sdio-poll" IN_LIST f1_sdio OR "sdio-poll-write" IN_LIST f1_sdio)
    target_sources(nexus_soc_stm32f1 PRIVATE transport.c sd_block.c)
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOBlock)
endif()
