# Exact enhanced SPI selections reuse the maintained physical factory model graph.
foreach(sequence_family stm32 gd32)
    if(sequence_family STREQUAL "stm32")
        set(sequence_stem f407)
    else()
        set(sequence_stem gd470)
    endif()
    set(sequence_input "${NEXUS_SOURCE_DIR}/tests/contracts/io_evolution_assembly/${sequence_stem}-spi-dma-transaction.toml")
    set(sequence_bundle "${CMAKE_CURRENT_BINARY_DIR}/spi-sequence-factory-${sequence_family}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${sequence_input}"
        --output "${sequence_bundle}" RESULT_VARIABLE sequence_generated
        OUTPUT_VARIABLE sequence_output ERROR_VARIABLE sequence_error)
    if(NOT sequence_generated EQUAL 0)
        message(FATAL_ERROR "Enhanced SPI factory rejected: ${sequence_output}${sequence_error}")
    endif()
    nexus_google_configuration_inputs("${sequence_bundle}")

    set(sequence_reference "nexus_${sequence_family}_multi_controller_factory_test")
    # The STM32 clone shares the parent HAL objects only with this exact
    # family/model/profile context. Each executable supplies its own ports.
    get_property(sequence_sources TARGET ${sequence_reference} PROPERTY SOURCES)
    get_property(sequence_libraries TARGET ${sequence_reference} PROPERTY LINK_LIBRARIES)
    get_property(sequence_includes TARGET ${sequence_reference} PROPERTY INCLUDE_DIRECTORIES)
    get_property(sequence_system TARGET ${sequence_reference} PROPERTY SYSTEM_INCLUDE_DIRECTORIES)
    get_property(sequence_definitions TARGET ${sequence_reference} PROPERTY COMPILE_DEFINITIONS)
    get_property(sequence_options TARGET ${sequence_reference} PROPERTY COMPILE_OPTIONS)
    list(FILTER sequence_sources EXCLUDE REGEX "multi_controller_factory_test.cpp$")
    list(FILTER sequence_sources EXCLUDE REGEX "/multi-controller-factory-[^/]+/bindings.c$")
    set(sequence_owned_includes "${sequence_bundle}")
    set(sequence_vendor_includes)
    foreach(directory IN LISTS sequence_includes)
        if(directory MATCHES "/multi-controller-factory-[^/]+$")
            continue()
        endif()
        string(FIND "${directory}" "${NEXUS_SOURCE_DIR}/vendors/" vendor_prefix)
        if(vendor_prefix EQUAL 0)
            list(APPEND sequence_vendor_includes "${directory}")
        else()
            list(APPEND sequence_owned_includes "${directory}")
        endif()
    endforeach()
    if(sequence_family STREQUAL "gd32")
        list(APPEND sequence_sources
            "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/drivers/i2c.c")
    endif()
    list(REMOVE_DUPLICATES sequence_sources)
    set(sequence_target "nexus_${sequence_family}_spi_sequence_factory_test")
    nexus_google_test(${sequence_target}
        SOURCES spi_sequence_factory_test.cpp "${sequence_bundle}/bindings.c"
            ${sequence_sources}
        LIBRARIES ${sequence_libraries} Nexus::SPIDMATransactionOwner
        INCLUDES ${sequence_owned_includes})
    target_include_directories(${sequence_target} SYSTEM PRIVATE
        ${sequence_system} ${sequence_vendor_includes})
    target_compile_definitions(${sequence_target} PRIVATE ${sequence_definitions})
    target_compile_options(${sequence_target} PRIVATE ${sequence_options})
    if(sequence_family STREQUAL "stm32")
        target_link_options(${sequence_target} PRIVATE "-Wl,--gc-sections")
    endif()
    if(sequence_family STREQUAL "gd32")
        # The retained polling participant now uses actual SPL primitives.
        # Only its unrelated reset remains the fixture's external hardware
        # model; rename that unused SDK entry in this private object alone.
        set(sequence_i2c_vendor "${sequence_target}_i2c_vendor")
        add_library(${sequence_i2c_vendor} OBJECT
            "${NEXUS_SOURCE_DIR}/vendors/gigadevice/gd32f4xx/Firmware/GD32F4xx_standard_peripheral/Source/gd32f4xx_i2c.c")
        target_include_directories(${sequence_i2c_vendor} PRIVATE
            "${NEXUS_SOURCE_DIR}/tests/contracts/gd32_model"
            "${NEXUS_SOURCE_DIR}/soc/gd32/gd32f470/private")
        target_include_directories(${sequence_i2c_vendor} SYSTEM PRIVATE
            ${sequence_system} ${sequence_vendor_includes})
        target_compile_definitions(${sequence_i2c_vendor} PRIVATE
            GD32F470 HXTAL_VALUE=25000000
            i2c_deinit=nx_sequence_official_i2c_deinit)
        target_compile_features(${sequence_i2c_vendor} PRIVATE c_std_11)
        target_compile_options(${sequence_i2c_vendor} PRIVATE
            -Wno-int-to-pointer-cast)
        target_sources(${sequence_target} PRIVATE
            $<TARGET_OBJECTS:${sequence_i2c_vendor}>)
    endif()
endforeach()
