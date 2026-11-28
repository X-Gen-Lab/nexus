# Select the reviewed STM32F1 FPEC implementation.
nexus_controller_modes(flash_modes flash)
if("poll-halfword" IN_LIST flash_modes)
    target_sources(
        nexus_soc_stm32f1
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/flash.c"
    )
    target_link_libraries(nexus_soc_stm32f1 PRIVATE Nexus::IOFlash)
endif()
