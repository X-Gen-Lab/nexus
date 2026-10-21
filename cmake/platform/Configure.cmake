# A failing configuration removes the previous owned generated bundle.
function(nexus_resolve_assembly)
    get_cmake_property(assembly_cache_variables CACHE_VARIABLES)
    foreach(override IN LISTS assembly_cache_variables)
        if(override MATCHES "^NEXUS_(CPU|ARCH|IRQ|OS)_")
            message(FATAL_ERROR
                "${override} is not a platform override; use NEXUS_ASSEMBLY_FILE")
        endif()
    endforeach()
    get_filename_component(assembly "${NEXUS_ASSEMBLY_FILE}" ABSOLUTE
        BASE_DIR "${CMAKE_SOURCE_DIR}")
    set(bundle "${NEXUS_BINARY_DIR}/generated")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${assembly}"
        --output "${bundle}" RESULT_VARIABLE result
        OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Configuration rejected: ${output}${error}")
    endif()
    include("${bundle}/selection.cmake")
    # Register the resolver's exact configuration-code contract. This tracks
    # Python generation inputs only; CMake still owns production source targets.
    file(READ "${bundle}/configuration_code_paths.json" configuration_code)
    string(JSON code_count LENGTH "${configuration_code}")
    math(EXPR code_last "${code_count} - 1")
    foreach(index RANGE 0 ${code_last})
        string(JSON name MEMBER "${configuration_code}" ${index})
        string(JSON path GET "${configuration_code}" "${name}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
    endforeach()
    foreach(value NEXUS_SOC_FAMILY NEXUS_EXACT_PART NEXUS_BACKEND
                  NEXUS_CPU_ARCH NEXUS_CPU_FPU NEXUS_FLOAT_ABI NEXUS_ENUM_ABI
                  NEXUS_ARCH_HAS_DWT_CYCCNT NEXUS_IRQ_PRIORITY_BITS
                  NEXUS_ARCH_MPU_VERSION NEXUS_ARCH_ICACHE_LINE_BYTES
                  NEXUS_ARCH_DCACHE_LINE_BYTES NEXUS_ARCH_SECURITY_STATE
                  NEXUS_ARCH_HAS_SAU NEXUS_CPU_HAS_BASEPRI NEXUS_CPU_HAS_FPU
                  NEXUS_CPU_HAS_MVE NEXUS_CPU_HAS_DSP NEXUS_CPU_MPU_REGIONS
                  NEXUS_CPU_SECURE_ONLY
                  NEXUS_CPU_EXTERNAL_IRQ_COUNT NEXUS_CPU_ATOMIC_BACKEND_IRQ
                  NEXUS_CPU_COMPILE_OPTIONS NEXUS_IRQ_KERNEL_POLICY
                  NEXUS_FREERTOS_PORT NEXUS_THREADX_PORT
                  NEXUS_OPTIMIZATION NEXUS_SELECTED_KINDS
                  NEXUS_MEMORY_ADDSRAM NEXUS_MEMORY_TCM NEXUS_MEMORY_CCM
                  NEXUS_MEMORY_SRAM1 NEXUS_MEMORY_SRAM4
                  NEXUS_IO_CYCLE_PROBES NEXUS_RUNTIME_POWER
                  NEXUS_F407_I2S2_SLAVE_TX_SELECTED NEXUS_F407_I2S2_PROFILE
                  NEXUS_SELECTED_COMPONENTS NEXUS_CONFIG_SHA256
                  NEXUS_OS_SELECTION_VARIABLES ${NEXUS_OS_SELECTION_VARIABLES})
        set(${value} "${${value}}" PARENT_SCOPE)
    endforeach()
    set(NEXUS_CONFIG_DIR "${bundle}" PARENT_SCOPE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${assembly}")
    # Every authored and fact input comes from the same resolver snapshot.
    # CMake consumes identities; it never parses or normalizes authored TOML.
    file(READ "${bundle}/input_paths.json" input_paths)
    string(JSON count LENGTH "${input_paths}")
    if(count GREATER 0)
        math(EXPR last "${count} - 1")
        foreach(index RANGE 0 ${last})
            string(JSON name MEMBER "${input_paths}" ${index})
            string(JSON input GET "${input_paths}" "${name}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                "${input}")
        endforeach()
    endif()
endfunction()
