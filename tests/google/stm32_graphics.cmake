# Checked public graphics contracts and real F479 controller/HAL model leaves.
# Author: Nexus Team
nexus_google_test(nexus_graphics_api_test
    SOURCES stm32_graphics_api_test.cpp
    LIBRARIES Nexus::IO)

# Current modeled pixel buffers use Linux/x86-64 MAP_32BIT. Other hosts need an
# explicit equivalent 32-bit address mapping, never silent truncation or skips.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
        CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    foreach(engine ltdc dma2d)
        set(graphics_test "nexus_stm32_${engine}_test")
        set(graphics_profile
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private/${engine}_hal")
        set(graphics_model
            "${CMAKE_CURRENT_SOURCE_DIR}/stm32_${engine}_model.h")
        nexus_google_test(${graphics_test} STM32F4_LL
            SOURCES "stm32_${engine}_test.cpp"
                "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/${engine}.c"
            LIBRARIES Nexus::IO Nexus::Arch
            INCLUDES "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private"
                "${NEXUS_SOURCE_DIR}/io/private"
                "${graphics_profile}" "${CMAKE_CURRENT_SOURCE_DIR}")
        target_include_directories(${graphics_test} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
        nexus_use_cmsis_core(${graphics_test})
        target_compile_definitions(${graphics_test} PRIVATE
            STM32F479xx NEXUS_STM32_MODEL=1)
        target_compile_options(${graphics_test} PRIVATE
            "-include${graphics_model}" -ffunction-sections -fdata-sections)
        target_link_options(${graphics_test} PRIVATE -Wl,--gc-sections)

        set(graphics_hal "nexus_${engine}_test_hal")
        add_library(${graphics_hal} OBJECT
            "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Src/stm32f4xx_hal_${engine}.c")
        if(engine STREQUAL "ltdc")
            target_sources(${graphics_hal} PRIVATE
                "${NEXUS_SOURCE_DIR}/vendors/st/stm32f4_hal/Src/stm32f4xx_hal_gpio.c")
        endif()
        nexus_stm32f4_official(${graphics_hal})
        target_include_directories(${graphics_hal} PRIVATE
            "${graphics_profile}" "${CMAKE_CURRENT_SOURCE_DIR}")
        target_include_directories(${graphics_hal} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
        nexus_use_cmsis_core(${graphics_hal})
        nexus_use_cmsis_host_model(${graphics_hal})
        target_compile_definitions(${graphics_hal} PRIVATE STM32F479xx)
        target_compile_features(${graphics_hal} PRIVATE c_std_11)
        set_target_properties(${graphics_hal} PROPERTIES C_EXTENSIONS OFF)
        target_link_libraries(${graphics_hal} PRIVATE nexus_build_options
            nexus_io_contract)
        target_compile_options(${graphics_hal} PRIVATE
            "-include${graphics_model}" -ffunction-sections -fdata-sections)
        target_sources(${graphics_test} PRIVATE
            $<TARGET_OBJECTS:${graphics_hal}>)
    endforeach()
endif()
