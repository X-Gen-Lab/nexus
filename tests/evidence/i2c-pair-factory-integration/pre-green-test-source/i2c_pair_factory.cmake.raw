# Actual generated paired-controller objects; only external hardware is modeled.
file(GLOB_RECURSE i2c_pair_codegen CONFIGURE_DEPENDS
    "${NEXUS_SOURCE_DIR}/tools/configure/*.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${i2c_pair_codegen})
foreach(i2c_pair_family stm32 gd32)
    if(i2c_pair_family STREQUAL "stm32")
        set(i2c_pair_label "stm32-zg")
        set(i2c_pair_soc "stm32f407")
    else()
        set(i2c_pair_label "gd32")
        set(i2c_pair_soc "gd32f470")
    endif()
    foreach(i2c_pair_mode dma irq)
        set(i2c_pair_variant "i2c-pair")
        set(i2c_pair_target "nexus_${i2c_pair_family}_i2c_pair_factory_test")
        if(i2c_pair_mode STREQUAL "irq")
            set(i2c_pair_variant "i2c-pair-irq")
            set(i2c_pair_target "nexus_${i2c_pair_family}_i2c_pair_irq_factory_test")
        endif()
        set(i2c_pair_input
            "${NEXUS_SOURCE_DIR}/tests/contracts/multi_controller_assembly/${i2c_pair_label}-${i2c_pair_variant}.toml")
        set(i2c_pair_bundle "${CMAKE_CURRENT_BINARY_DIR}/i2c-pair-${i2c_pair_family}-${i2c_pair_mode}")
        execute_process(COMMAND "${Python3_EXECUTABLE}" -B
            "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
            --source-root "${NEXUS_SOURCE_DIR}" --assembly "${i2c_pair_input}"
            --output "${i2c_pair_bundle}" RESULT_VARIABLE i2c_pair_generated
            OUTPUT_VARIABLE i2c_pair_output ERROR_VARIABLE i2c_pair_error)
        if(NOT i2c_pair_generated EQUAL 0)
            message(FATAL_ERROR "I2C-pair factory rejected: ${i2c_pair_output}${i2c_pair_error}")
        endif()
        file(READ "${i2c_pair_bundle}/input_paths.json" i2c_pair_inputs)
        string(JSON i2c_pair_count LENGTH "${i2c_pair_inputs}")
        math(EXPR i2c_pair_last "${i2c_pair_count} - 1")
        foreach(index RANGE 0 ${i2c_pair_last})
            string(JSON i2c_pair_key MEMBER "${i2c_pair_inputs}" ${index})
            string(JSON i2c_pair_path GET "${i2c_pair_inputs}" "${i2c_pair_key}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                "${i2c_pair_path}")
        endforeach()
        nexus_google_test(${i2c_pair_target} SOURCES i2c_pair_factory_test.cpp
            "${i2c_pair_bundle}/bindings.c"
            "${NEXUS_SOURCE_DIR}/soc/common/i2c_v1_async.c"
            "${NEXUS_SOURCE_DIR}/soc/${i2c_pair_soc}/drivers/i2c_async.c"
            "${NEXUS_SOURCE_DIR}/soc/${i2c_pair_soc}/drivers/gpio.c"
            LIBRARIES Nexus::IO Nexus::Arch
            INCLUDES "${i2c_pair_bundle}"
                "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
                "${NEXUS_SOURCE_DIR}/soc/common"
                "${NEXUS_SOURCE_DIR}/soc/gd32f470"
                "${NEXUS_SOURCE_DIR}/soc/${i2c_pair_soc}/private")
        if(i2c_pair_mode STREQUAL "irq")
            target_compile_definitions(${i2c_pair_target} PRIVATE NX_I2C_PAIR_IRQ)
        endif()
        target_compile_options(${i2c_pair_target} PRIVATE
            "-include${CMAKE_CURRENT_SOURCE_DIR}/i2c_pair_factory_model.h")
        target_compile_definitions(${i2c_pair_target} PRIVATE
            _init=nx_i2c_pair_fixture_init _fini=nx_i2c_pair_fixture_fini)
        if(i2c_pair_family STREQUAL "stm32")
            target_include_directories(${i2c_pair_target} SYSTEM PRIVATE
                "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
            target_compile_definitions(${i2c_pair_target} PRIVATE
                STM32F407xx NX_I2C_PAIR_STM32)
        else()
            set(i2c_pair_gd "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware")
            target_include_directories(${i2c_pair_target} SYSTEM PRIVATE
                "${i2c_pair_gd}/CMSIS/GD/GD32F4xx/Include"
                "${i2c_pair_gd}/GD32F4xx_standard_peripheral/Include")
            target_sources(${i2c_pair_target} PRIVATE
                "${i2c_pair_gd}/GD32F4xx_standard_peripheral/Source/gd32f4xx_gpio.c")
            target_compile_definitions(${i2c_pair_target} PRIVATE
                GD32F470 HXTAL_VALUE=25000000)
            target_compile_options(${i2c_pair_target} PRIVATE
                "$<$<COMPILE_LANGUAGE:C>:-Wno-int-to-pointer-cast>")
        endif()
    endforeach()
endforeach()
