# A failing configuration removes the previous owned generated bundle.
function(nexus_resolve_assembly)
    get_filename_component(assembly "${NEXUS_ASSEMBLY_FILE}" ABSOLUTE
        BASE_DIR "${CMAKE_SOURCE_DIR}")
    set(bundle "${NEXUS_BINARY_DIR}/generated")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/bindings.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/stm32_bindings.py"
        "${NEXUS_SOURCE_DIR}/tools/configure/gd32_bindings.py")
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
                  NEXUS_OPTIMIZATION NEXUS_SELECTED_KINDS
                  NEXUS_SELECTED_COMPONENTS NEXUS_CONFIG_SHA256)
        set(${value} "${${value}}" PARENT_SCOPE)
    endforeach()
    set(NEXUS_CONFIG_DIR "${bundle}" PARENT_SCOPE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${assembly}")
    file(READ "${assembly}" assembly_json)
    string(JSON board_relative GET "${assembly_json}" board_package)
    get_filename_component(assembly_dir "${assembly}" DIRECTORY)
    get_filename_component(board_dir "${board_relative}" ABSOLUTE BASE_DIR "${assembly_dir}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${board_dir}/board.json")
    string(JSON layout_type TYPE "${assembly_json}" layout)
    if(NOT layout_type STREQUAL "NULL")
        string(JSON layout_relative GET "${assembly_json}" layout)
        get_filename_component(layout "${layout_relative}" ABSOLUTE BASE_DIR "${assembly_dir}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${layout}")
    endif()
    file(READ "${board_dir}/board.json" board_json)
    string(JSON declared_count ERROR_VARIABLE no_inputs LENGTH "${board_json}" inputs)
    if(NOT no_inputs AND declared_count GREATER 0)
        math(EXPR declared_last "${declared_count} - 1")
        foreach(index RANGE 0 ${declared_last})
            string(JSON declared GET "${board_json}" inputs ${index})
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${board_dir}/${declared}")
        endforeach()
    endif()
    file(READ "${bundle}/resolved.json" resolved)
    string(JSON count LENGTH "${resolved}" inputs)
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(JSON name GET "${resolved}" inputs ${index} path)
        if(NOT name MATCHES "^external/")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                "${NEXUS_SOURCE_DIR}/${name}")
        endif()
    endforeach()
endfunction()
