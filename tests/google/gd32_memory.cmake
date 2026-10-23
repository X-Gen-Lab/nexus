# Actual production reset initialization; framework remains host-only.
nexus_google_test(nexus_gd32_memory_test SOURCES gd32_memory_test.cpp
    "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/memory_init.c"
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470")

# These reset-only model headers select a controlled test variant. They are not
# factory/configuration authority or source-SDK qualification.
set(memory_gd_firmware
    "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
foreach(memory_variant default both)
    set(memory_target "nexus_gd32_memory_boot_${memory_variant}_test")
    set(memory_model "${CMAKE_CURRENT_BINARY_DIR}/gd-memory-${memory_variant}")
    file(MAKE_DIRECTORY "${memory_model}")
    if(memory_variant STREQUAL "both")
        set(memory_selected 1)
    else()
        set(memory_selected 0)
    endif()
    file(WRITE "${memory_model}/nexus_config.h"
        "#define NEXUS_MEMORY_ADDSRAM ${memory_selected}\n#define NEXUS_MEMORY_TCM ${memory_selected}\n")
    nexus_google_test(${memory_target} SOURCES gd32_memory_boot_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/system.c"
        INCLUDES "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
            "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470"
            "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/private"
            "${NEXUS_SOURCE_DIR}/core/include"
            "${NEXUS_SOURCE_DIR}/arch/include")
    target_include_directories(${memory_target} BEFORE PRIVATE "${memory_model}")
    target_include_directories(${memory_target} SYSTEM PRIVATE
        "${memory_gd_firmware}/CMSIS/GD/GD32F4xx/Include"
        "${memory_gd_firmware}/GD32F4xx_standard_peripheral/Include")
    target_compile_definitions(${memory_target} PRIVATE GD32F470
        HXTAL_VALUE=25000000 NX_GD32_CLOCK_MODEL)
    target_compile_options(${memory_target} PRIVATE -fno-pie
        -ffunction-sections -fdata-sections -Wno-int-to-pointer-cast)
    # A low non-PIE randomized brk can overlap an MCU address window. Keep the
    # host image and its brk above all modeled hardware; never replace a mapping.
    target_link_options(${memory_target} PRIVATE -no-pie -Wl,--gc-sections
        -Wl,-Ttext-segment=0x60000000)
    if(memory_selected)
        target_sources(${memory_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/memory_init.c")
    endif()
endforeach()
