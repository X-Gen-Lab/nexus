# Execute the production cold transaction and vendor GPIO implementation.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(gd_cold_root "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470")
    set(gd_cold_sdk "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
    nexus_google_test(nexus_gd32_cold_pins_test
        SOURCES gd32_cold_pins_test.cpp "${gd_cold_root}/drivers/gpio.c"
            "${gd_cold_sdk}/GD32F4xx_standard_peripheral/Source/gd32f4xx_gpio.c"
        LIBRARIES Nexus::IO
        INCLUDES "${gd_cold_root}" "${gd_cold_root}/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
            "${NEXUS_SOURCE_DIR}/arch/include")
    target_include_directories(nexus_gd32_cold_pins_test SYSTEM PRIVATE
        "${gd_cold_sdk}/CMSIS/GD/GD32F4xx/Include"
        "${gd_cold_sdk}/GD32F4xx_standard_peripheral/Include")
    target_compile_definitions(nexus_gd32_cold_pins_test PRIVATE
        GD32F470 HXTAL_VALUE=25000000)
    target_compile_options(nexus_gd32_cold_pins_test PRIVATE
        "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>"
        "-include${CMAKE_CURRENT_SOURCE_DIR}/gd32_cold_pins_model.h")
    target_link_options(nexus_gd32_cold_pins_test PRIVATE -Wl,--gc-sections)
endif()
