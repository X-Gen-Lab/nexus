# Source checkout discovery uses the same API as an installed source SDK.
function(_nexus_discover_checkout)
    get_filename_component(
        NEXUS_SOURCE_DIR
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.."
        ABSOLUTE
    )
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    include("${NEXUS_SOURCE_DIR}/cmake/platform/SDK.cmake")
    get_property(existing GLOBAL PROPERTY NEXUS_PACKAGE_SOURCE)
    if(existing AND NOT existing STREQUAL NEXUS_SOURCE_DIR)
        message(
            FATAL_ERROR
            "One build may contain only one resolved Nexus source SDK"
        )
    endif()
    set_property(GLOBAL PROPERTY NEXUS_PACKAGE_SOURCE "${NEXUS_SOURCE_DIR}")
    execute_process(
        COMMAND
            "${Python3_EXECUTABLE}" -B
            "${NEXUS_SOURCE_DIR}/tools/configure/metadata.py" --version
        RESULT_VARIABLE result
        OUTPUT_VARIABLE version
        ERROR_VARIABLE error
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Nexus version policy rejected: ${error}")
    endif()
    include("${NEXUS_SOURCE_DIR}/cmake/Nexus.cmake")
    set(Nexus_SOURCE_DIR "${NEXUS_SOURCE_DIR}" PARENT_SCOPE)
    set(Nexus_VERSION "${version}" PARENT_SCOPE)
    set(Nexus_TOOLCHAIN_DIR "${NEXUS_SOURCE_DIR}/cmake/toolchains" PARENT_SCOPE)
endfunction()
_nexus_discover_checkout()
set(Nexus_FOUND TRUE)
if(Nexus_FIND_COMPONENTS)
    if(NOT Nexus_FIND_COMPONENTS STREQUAL "Runtime")
        message(
            FATAL_ERROR
            "The only optional Nexus package component is Runtime"
        )
    endif()
    set(Nexus_Runtime_FOUND TRUE)
endif()
