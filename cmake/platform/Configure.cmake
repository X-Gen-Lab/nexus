# A failing configuration removes the previous owned generated bundle.
function(nexus_resolve_assembly)
    get_filename_component(assembly "${NEXUS_ASSEMBLY_FILE}" ABSOLUTE
        BASE_DIR "${CMAKE_SOURCE_DIR}")
    set(bundle "${NEXUS_BINARY_DIR}/generated")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/authored.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/ir.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/factory.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/bindings.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/providers/__init__.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/providers/common.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/providers/native/__init__.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/providers/native/bindings.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/providers/native/emission.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/providers/stm32f407/__init__.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/providers/stm32f407/bindings.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/providers/stm32f407/emission.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/providers/gd32f470/__init__.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/providers/gd32f470/bindings.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/providers/gd32f470/emission.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/emission.py")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${assembly}"
        --output "${bundle}" RESULT_VARIABLE result
        OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Configuration rejected: ${output}${error}")
    endif()
    include("${bundle}/selection.cmake")
    foreach(value NEXUS_SOC_FAMILY NEXUS_EXACT_PART NEXUS_BACKEND
                  NEXUS_CPU_ARCH NEXUS_CPU_FPU NEXUS_FLOAT_ABI NEXUS_ENUM_ABI
                  NEXUS_ARCH_HAS_DWT_CYCCNT NEXUS_IRQ_PRIORITY_BITS
                  NEXUS_FREERTOS_PORT
                  NEXUS_OPTIMIZATION NEXUS_SELECTED_KINDS
                  NEXUS_SELECTED_COMPONENTS NEXUS_CONFIG_SHA256)
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
