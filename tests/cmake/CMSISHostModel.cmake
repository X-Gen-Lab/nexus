# Only host register-model targets may bridge the CMSIS compiler entry point.
include_guard(GLOBAL)

function(nexus_use_cmsis_host_model target)
    if(NOT ARGC EQUAL 1 OR NOT TARGET "${target}" OR CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR "CMSIS register model requires one host-only concrete target")
    endif()
    get_target_property(type "${target}" TYPE)
    if(type STREQUAL "INTERFACE_LIBRARY")
        message(FATAL_ERROR "CMSIS register model requires one host-only concrete target")
    endif()
    get_filename_component(shim
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../cmsis_host" ABSOLUTE)
    # Evaluate the final consumer's direct dependencies, after its MCU
    # fixture has selected Core. Pure Native tests keep their usual flags.
    set(selected "$<IN_LIST:nexus_cmsis_core,$<TARGET_PROPERTY:LINK_LIBRARIES>>")
    if(NOT TARGET nexus_cmsis_host_model)
        add_library(nexus_cmsis_host_model INTERFACE)
        target_include_directories(nexus_cmsis_host_model INTERFACE
            "$<${selected}:${shim}>")
        target_compile_definitions(nexus_cmsis_host_model INTERFACE
            "$<${selected}:NEXUS_CMSIS_HOST_MODEL=1>")
    endif()
    # Model preludes may include Device/Core themselves. Declaration selection
    # must happen first, without propagating it to unrelated dependent targets.
    target_compile_options("${target}" BEFORE PRIVATE
        "$<${selected}:-include${shim}/compiler.h>")
    target_link_libraries("${target}" PRIVATE nexus_cmsis_host_model)
endfunction()
