# Compile the exact bxCAN1 only for the selected polling mode.
# Author: Nexus Team
if("classic-poll" IN_LIST f1_can OR "classic-poll-normal" IN_LIST f1_can)
    target_sources(nexus_soc_stm32f1 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/can.c")
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOCAN)
endif()
if("classic-poll-normal" IN_LIST f1_can)
    target_compile_definitions(
        nexus_soc_stm32f1
        PRIVATE NX_STM32F1_CAN_NORMAL=1
    )
    target_sources(
        nexus_soc_stm32f1
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/normal.c"
    )
endif()
