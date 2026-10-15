# Execute the production GD32 timebase against mocked CPU and SDK boundaries.
nexus_google_test(nexus_gd32_arch_test SOURCES gd32_arch_test.cpp
    "${NEXUS_SOURCE_DIR}/soc/gd32f470/timebase.c"
    LIBRARIES Nexus::Core
    INCLUDES "${NEXUS_SOURCE_DIR}/arch/include"
        "${NEXUS_SOURCE_DIR}/soc/gd32f470"
        "${NEXUS_SOURCE_DIR}/soc/gd32f470/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
set(gd_arch_vendor
    "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
target_include_directories(nexus_gd32_arch_test SYSTEM PRIVATE
    "${gd_arch_vendor}/CMSIS/GD/GD32F4xx/Include"
    "${gd_arch_vendor}/GD32F4xx_standard_peripheral/Include")
target_compile_definitions(nexus_gd32_arch_test PRIVATE GD32F470
    HXTAL_VALUE=25000000)
target_compile_options(nexus_gd32_arch_test PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
