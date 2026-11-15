# Register exact resolver inputs for a real generated host/model assembly.
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/../platform/Configure.cmake")

function(nexus_google_configuration_inputs bundle)
    _nexus_watch_configuration_inputs("${bundle}"
        input_paths configuration_code_paths)
endfunction()
