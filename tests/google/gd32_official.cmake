# Execute the unchanged GD32 GPIO library through the production provider.
# This closes the vendor-code boundary omitted by the broader MMIO model.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(gd_official_root "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470")
    set(gd_official_sdk
        "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/scripts/ci/verify_vendor_source.py"
        --root "${gd_official_sdk}/.."
        RESULT_VARIABLE gd_official_verified
        OUTPUT_VARIABLE gd_official_output ERROR_VARIABLE gd_official_error)
    if(NOT gd_official_verified EQUAL 0)
        message(FATAL_ERROR
            "GD32 reference lock failed: ${gd_official_output}${gd_official_error}")
    endif()
    nexus_google_test(nexus_gd32_official_gpio_test
        SOURCES gd32_official_gpio_test.cpp
            "${gd_official_root}/drivers/gpio.c"
            "${gd_official_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_gpio.c"
            "${gd_official_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_rcu.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${gd_official_root}" "${gd_official_root}/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
    target_include_directories(nexus_gd32_official_gpio_test SYSTEM PRIVATE
        "${gd_official_sdk}/CMSIS/GD/GD32F4xx/Include"
        "${gd_official_sdk}/GD32F4xx_standard_peripheral/Include")
    target_compile_definitions(nexus_gd32_official_gpio_test PRIVATE
        GD32F470 HXTAL_VALUE=25000000)
    target_compile_options(nexus_gd32_official_gpio_test PRIVATE
        "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
    # Unused upstream GPIO reset helpers do not belong to this selected path.
    target_link_options(nexus_gd32_official_gpio_test PRIVATE -Wl,--gc-sections)
    nexus_google_test(nexus_gd32_official_spi_test
        SOURCES gd32_official_spi_test.cpp
            "${gd_official_root}/drivers/spi.c"
            "${gd_official_root}/drivers/gpio.c"
            "${gd_official_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_spi.c"
            "${gd_official_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_gpio.c"
            "${gd_official_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_rcu.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${gd_official_root}" "${gd_official_root}/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
    target_include_directories(nexus_gd32_official_spi_test SYSTEM PRIVATE
        "${gd_official_sdk}/CMSIS/GD/GD32F4xx/Include"
        "${gd_official_sdk}/GD32F4xx_standard_peripheral/Include")
    target_compile_definitions(nexus_gd32_official_spi_test PRIVATE
        GD32F470 HXTAL_VALUE=25000000)
    target_compile_options(nexus_gd32_official_spi_test PRIVATE
        "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
    target_link_options(nexus_gd32_official_spi_test PRIVATE -Wl,--gc-sections)
    # Upstream 32-bit register masks use unsigned long. LP64 host builds keep
    # the existing vendor-only conversion policy; owned provider warnings stay
    # strict. The unchanged reset leaf is also required by GPIO model links.
    set_source_files_properties(
        "${gd_official_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_gpio.c"
        "${gd_official_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_rcu.c"
        "${gd_official_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_spi.c"
        "${gd_official_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_i2c.c"
        PROPERTIES COMPILE_OPTIONS "-Wno-conversion;-Wno-sign-conversion")
endif()
