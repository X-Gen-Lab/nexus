# Execute the real generated five-controller graph with exact SDK providers.
file(GLOB_RECURSE multi_factory_codegen CONFIGURE_DEPENDS
    "${NEXUS_SOURCE_DIR}/tools/configure/*.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${multi_factory_codegen})
foreach(multi_family stm32 gd32)
    if(multi_family STREQUAL "stm32")
        set(multi_label stm32-zg)
        set(multi_soc stm32f407)
    else()
        set(multi_label gd32)
        set(multi_soc gd32f470)
    endif()
    set(multi_input "${NEXUS_SOURCE_DIR}/tests/contracts/multi_controller_assembly/${multi_label}.toml")
    set(multi_bundle "${CMAKE_CURRENT_BINARY_DIR}/multi-controller-factory-${multi_family}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${multi_input}"
        --output "${multi_bundle}" RESULT_VARIABLE multi_generated
        OUTPUT_VARIABLE multi_output ERROR_VARIABLE multi_error)
    if(NOT multi_generated EQUAL 0)
        message(FATAL_ERROR "Five-controller factory rejected: ${multi_output}${multi_error}")
    endif()
    file(READ "${multi_bundle}/input_paths.json" multi_inputs)
    string(JSON multi_count LENGTH "${multi_inputs}")
    math(EXPR multi_last "${multi_count} - 1")
    foreach(index RANGE 0 ${multi_last})
        string(JSON label MEMBER "${multi_inputs}" ${index})
        string(JSON input_path GET "${multi_inputs}" "${label}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input_path}")
    endforeach()
    set(multi_target "nexus_${multi_family}_multi_controller_factory_test")
    nexus_google_test(${multi_target} SOURCES multi_controller_factory_test.cpp
        "${multi_bundle}/bindings.c"
        "${NEXUS_SOURCE_DIR}/soc/${multi_soc}/drivers/uart.c"
        "${NEXUS_SOURCE_DIR}/soc/${multi_soc}/drivers/uart_dma.c"
        "${NEXUS_SOURCE_DIR}/soc/${multi_soc}/drivers/spi.c"
        "${NEXUS_SOURCE_DIR}/soc/${multi_soc}/drivers/spi_dma.c"
        "${NEXUS_SOURCE_DIR}/soc/${multi_soc}/drivers/i2c_async.c"
        "${NEXUS_SOURCE_DIR}/soc/common/i2c_v1_async.c"
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${multi_bundle}" "${NEXUS_SOURCE_DIR}/io/private"
            "${NEXUS_SOURCE_DIR}/soc/common"
            "${NEXUS_SOURCE_DIR}/soc/${multi_soc}"
            "${NEXUS_SOURCE_DIR}/soc/${multi_soc}/private"
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
    target_compile_options(${multi_target} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/multi_controller_factory_model.h"
        "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
    target_compile_definitions(${multi_target} PRIVATE
        _init=nx_multi_fixture_init _fini=nx_multi_fixture_fini)
    # Keep real GPIO methods; only the external cold pin/clock ports are modeled.
    set(multi_gpio "${multi_target}_gpio")
    add_library(${multi_gpio} OBJECT
        "${NEXUS_SOURCE_DIR}/soc/${multi_soc}/drivers/gpio.c")
    target_link_libraries(${multi_gpio} PRIVATE Nexus::IO Nexus::Arch)
    target_include_directories(${multi_gpio} PRIVATE
        "${multi_bundle}" "${NEXUS_SOURCE_DIR}/soc/${multi_soc}"
        "${NEXUS_SOURCE_DIR}/soc/${multi_soc}/private"
        "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model")
    target_compile_options(${multi_gpio} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/multi_controller_factory_model.h"
        "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
    if(TARGET nexus_build_options)
        target_link_libraries(${multi_gpio} PRIVATE
            nexus_build_options nexus_owned_warnings)
    endif()
    target_sources(${multi_target} PRIVATE $<TARGET_OBJECTS:${multi_gpio}>)
    set(multi_gd "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
    target_include_directories(${multi_target} SYSTEM PRIVATE
        "${multi_gd}/CMSIS/GD/GD32F4xx/Include"
        "${multi_gd}/GD32F4xx_standard_peripheral/Include")
    target_include_directories(${multi_gpio} SYSTEM PRIVATE
        "${multi_gd}/CMSIS/GD/GD32F4xx/Include"
        "${multi_gd}/GD32F4xx_standard_peripheral/Include")
    if(multi_family STREQUAL "stm32")
        target_include_directories(${multi_target} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
        target_compile_definitions(${multi_target} PRIVATE
            STM32F407xx NX_MULTI_FACTORY_STM32)
        target_include_directories(${multi_gpio} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
        target_compile_definitions(${multi_gpio} PRIVATE
            STM32F407xx NX_MULTI_FACTORY_STM32
            nx_stm32_gpio_clock_enable=nx_multi_actual_gpio_clock_enable
            nx_stm32_pin_capture=nx_multi_actual_pin_capture
            nx_stm32_pin_configure=nx_multi_actual_pin_configure
            nx_stm32_pin_restore=nx_multi_actual_pin_restore)
    else()
        target_compile_definitions(${multi_target} PRIVATE
            GD32F470 HXTAL_VALUE=25000000)
        target_compile_definitions(${multi_gpio} PRIVATE
            GD32F470 HXTAL_VALUE=25000000
            nx_gd32_gpio_clock_enable=nx_multi_actual_gpio_clock_enable
            nx_gd32_pin_capture=nx_multi_actual_pin_capture
            nx_gd32_pin_configure=nx_multi_actual_pin_configure
            nx_gd32_pin_restore=nx_multi_actual_pin_restore)
        set(multi_vendor "${multi_target}_vendor")
        add_library(${multi_vendor} OBJECT
            "${multi_gd}/GD32F4xx_standard_peripheral/Source/gd32f4xx_gpio.c"
            "${multi_gd}/GD32F4xx_standard_peripheral/Source/gd32f4xx_rcu.c"
            "${multi_gd}/GD32F4xx_standard_peripheral/Source/gd32f4xx_usart.c"
            "${multi_gd}/GD32F4xx_standard_peripheral/Source/gd32f4xx_spi.c")
        target_include_directories(${multi_vendor} PRIVATE
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
            "${NEXUS_SOURCE_DIR}/soc/gd32f470/private")
        target_include_directories(${multi_vendor} SYSTEM PRIVATE
            "${multi_gd}/CMSIS/GD/GD32F4xx/Include"
            "${multi_gd}/GD32F4xx_standard_peripheral/Include")
        target_compile_definitions(${multi_vendor} PRIVATE
            GD32F470 HXTAL_VALUE=25000000)
        target_compile_features(${multi_vendor} PRIVATE c_std_11)
        target_compile_options(${multi_vendor} PRIVATE -Wno-int-to-pointer-cast)
        target_sources(${multi_target} PRIVATE $<TARGET_OBJECTS:${multi_vendor}>)
    endif()
endforeach()
