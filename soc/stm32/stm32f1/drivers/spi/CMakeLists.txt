# Compile the SPI2 short-poll owner only when the application selects it.
# Author: Nexus Team
if("short-poll" IN_LIST f1_spi)
    target_sources(nexus_soc_stm32f1 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/spi.c")
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOSPI Nexus::IOGPIO)
endif()
