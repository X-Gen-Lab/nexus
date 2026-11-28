# Select the reviewed F1 CPU-only direct DAC implementation.
# Author: Nexus Team
if("direct" IN_LIST f1_dac)
    target_sources(nexus_soc_stm32f1 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/dac.c")
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IODAC)
endif()
