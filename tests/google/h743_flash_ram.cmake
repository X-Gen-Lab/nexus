# RAM pulse contract target against the actual sectioned provider leaf.
nexus_google_test(nexus_h743_flash_ram_test
    SOURCES h743_flash_ram_test.cpp h743_model.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/drivers/flash_ram.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743/private")
target_include_directories(nexus_h743_flash_ram_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_h7/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_h743_flash_ram_test)
target_compile_definitions(nexus_h743_flash_ram_test PRIVATE STM32H743xx)
target_compile_options(nexus_h743_flash_ram_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/h743_flash_ram_model.h")
