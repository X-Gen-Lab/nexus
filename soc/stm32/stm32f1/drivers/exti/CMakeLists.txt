# Compile the reviewed EXTI13 IRQ owner only for the selected mode.
# Author: Nexus Team
if("edge-event" IN_LIST f1_exti)
    target_sources(nexus_soc_stm32f1 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/exti.c")
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOEXTI Nexus::IOGPIO)
endif()
