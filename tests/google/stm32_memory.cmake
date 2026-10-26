# Execute the production pre-RAM-safe initializer; Google remains host-only.
nexus_google_test(nexus_stm32_memory_test SOURCES stm32_memory_test.cpp
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/memory_init.c"
    LIBRARIES Nexus::IO
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407")
target_compile_definitions(nexus_stm32_memory_test PRIVATE
    NEXUS_STM32_MEMORY_MODEL)

foreach(memory_variant default ccm invalid)
    set(memory_target "nexus_stm32_memory_boot_${memory_variant}_test")
    set(memory_model "${CMAKE_CURRENT_BINARY_DIR}/stm32-memory-${memory_variant}")
    file(MAKE_DIRECTORY "${memory_model}")
    if(memory_variant STREQUAL "default")
        set(memory_selected 0)
    else()
        set(memory_selected 1)
    endif()
    # Reset-only controlled configuration does not establish factory support.
    file(WRITE "${memory_model}/nexus_config.h"
        "#define NEXUS_MEMORY_CCM ${memory_selected}\n")
    nexus_google_test(${memory_target} SOURCES stm32_memory_boot_test.cpp
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/system.c"
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407"
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/private"
            "${NEXUS_SOURCE_DIR}/core/include"
            "${NEXUS_SOURCE_DIR}/arch/include")
    target_include_directories(${memory_target} BEFORE PRIVATE "${memory_model}")
    target_include_directories(${memory_target} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${memory_target})
    target_compile_definitions(${memory_target} PRIVATE STM32F407xx
        NEXUS_STM32_MODEL)
    target_compile_options(${memory_target} PRIVATE -fno-pie
        -ffunction-sections -fdata-sections
        "-include${CMAKE_CURRENT_SOURCE_DIR}/stm32_memory_boot_model.h")
    # Preserve the real fixed addresses without replacing host image or brk.
    target_link_options(${memory_target} PRIVATE -no-pie -Wl,--gc-sections
        -Wl,-Ttext-segment=0x60000000)
    if(memory_selected)
        target_sources(${memory_target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f407/memory_init.c")
    endif()
    if(memory_variant STREQUAL "invalid")
        target_compile_definitions(${memory_target} PRIVATE
            NX_STM32_MEMORY_INVALID_PLAN)
    endif()
endforeach()
