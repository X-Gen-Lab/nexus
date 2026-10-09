# Exact GD32F470ZGT6 implementation; SDK headers stay private to this target.
set(gd_vendor "${PROJECT_SOURCE_DIR}/vendors/gigadevice/gd32f4xx")
if(DEFINED NEXUS_SOURCE_DIR)
    set(gd_vendor "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx")
endif()
execute_process(COMMAND "${Python3_EXECUTABLE}"
    "${gd_vendor}/../../../scripts/ci/verify_vendor_source.py"
    --root "${gd_vendor}"
    RESULT_VARIABLE gd_sdk_verified
    OUTPUT_VARIABLE gd_sdk_output ERROR_VARIABLE gd_sdk_error)
if(NOT gd_sdk_verified EQUAL 0)
    message(FATAL_ERROR "GD32 SDK lock failed: ${gd_sdk_output}${gd_sdk_error}")
endif()
set(gd_firmware "${gd_vendor}/Firmware")
set(gd_device "${gd_firmware}/CMSIS/GD/GD32F4xx")
set(gd_peripheral "${gd_firmware}/GD32F4xx_standard_peripheral")
add_library(nexus_gd32_sdk STATIC)
foreach(unit rcu gpio timer usart spi i2c fmc adc exti syscfg fwdgt)
    target_sources(nexus_gd32_sdk PRIVATE
        "${gd_peripheral}/Source/gd32f4xx_${unit}.c")
endforeach()
target_include_directories(nexus_gd32_sdk PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/private")
target_include_directories(nexus_gd32_sdk SYSTEM PRIVATE "${gd_firmware}/CMSIS"
    "${gd_device}/Include" "${gd_peripheral}/Include")
target_link_libraries(nexus_gd32_sdk PRIVATE nexus_build_options)
target_compile_definitions(nexus_gd32_sdk PRIVATE GD32F470 HXTAL_VALUE=25000000)
add_library(nexus_soc_gd32f470 STATIC timebase.c drivers/gpio.c
    drivers/uart.c drivers/spi.c drivers/i2c.c drivers/flash.c
    drivers/watchdog.c drivers/exti.c drivers/timer.c drivers/adc.c)
target_link_libraries(nexus_soc_gd32f470 PUBLIC nexus_io PRIVATE nexus_gd32_sdk
    nexus_build_options nexus_owned_warnings)
target_include_directories(nexus_soc_gd32f470 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/private")
target_include_directories(nexus_soc_gd32f470 SYSTEM PRIVATE
    "${gd_firmware}/CMSIS" "${gd_device}/Include"
    "${gd_peripheral}/Include")
target_compile_definitions(nexus_soc_gd32f470 PRIVATE GD32F470
    HXTAL_VALUE=25000000)
set(NEXUS_STARTUP_SOURCE "${gd_device}/Source/GCC/startup_gd32f450_470.S"
    PARENT_SCOPE)
set(NEXUS_SYSTEM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/system.c" PARENT_SCOPE)
# system.c is a firmware source, not an archive: startup must resolve SystemInit.
set(NEXUS_SYSTEM_PRIVATE_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/private" PARENT_SCOPE)
set(NEXUS_SYSTEM_VENDOR_INCLUDES "${gd_firmware}/CMSIS"
    "${gd_device}/Include" "${gd_peripheral}/Include" PARENT_SCOPE)
set(NEXUS_SYSTEM_COMPILE_DEFINITIONS GD32F470 HXTAL_VALUE=25000000 PARENT_SCOPE)
