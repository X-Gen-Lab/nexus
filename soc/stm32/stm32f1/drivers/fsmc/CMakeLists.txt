# Link only the reviewed SRAM LL engine when its controller is selected.
if("async16" IN_LIST f1_fsmc)
    add_library(
        nexus_stm32f1_fsmc_ll
        OBJECT
        "${f1_hal}/Src/stm32f1xx_ll_fsmc.c"
    )
    target_compile_definitions(
        nexus_stm32f1_fsmc_ll
        PRIVATE "${NEXUS_VENDOR_DEVICE_DEFINE}"
    )
    target_include_directories(
        nexus_stm32f1_fsmc_ll
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/hal_config/fsmc"
    )
    target_include_directories(
        nexus_stm32f1_fsmc_ll
        SYSTEM
        PRIVATE "${f1_device}/Include" "${f1_hal}/Inc"
    )
    nexus_use_cmsis_core(nexus_stm32f1_fsmc_ll)
    target_link_libraries(nexus_stm32f1_fsmc_ll PRIVATE nexus_build_options)
    target_sources(
        nexus_soc_stm32f1
        PRIVATE fsmc.c "$<TARGET_OBJECTS:nexus_stm32f1_fsmc_ll>"
    )
    target_link_libraries(
        nexus_soc_stm32f1
        PRIVATE Nexus::IOExternalBus Nexus::IOMemoryWindow
    )
endif()
