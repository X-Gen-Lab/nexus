# Concrete selected construction; mock only external SDK/CPU/register ports.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(gd_control_root "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470")
    set(gd_control_sdk "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
    nexus_google_test(nexus_gd32_control_cold_test
        SOURCES "${NEXUS_SOURCE_DIR}/tests/google/gd32_control_cold_test.cpp"
            "${gd_control_root}/drivers/timer.c"
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model/model.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${gd_control_root}" "${gd_control_root}/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
    target_include_directories(nexus_gd32_control_cold_test SYSTEM PRIVATE
        "${gd_control_sdk}/CMSIS/GD/GD32F4xx/Include"
        "${gd_control_sdk}/GD32F4xx_standard_peripheral/Include")
    target_compile_definitions(nexus_gd32_control_cold_test PRIVATE
        GD32F470 HXTAL_VALUE=25000000)
    target_compile_options(nexus_gd32_control_cold_test PRIVATE
        "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
endif()
