# The build aggregate is the execution authority. Deferred collection also
# includes targets declared after this file, avoiding order-dependent omissions.
function(nexus_write_google_plan)
    get_property(google_targets TARGET nexus_google_contracts
        PROPERTY MANUALLY_ADDED_DEPENDENCIES)
    if(NOT google_targets)
        message(FATAL_ERROR "GoogleTest aggregate contains no executable targets")
    endif()
    set(google_executables)
    foreach(google_target IN LISTS google_targets)
        get_target_property(google_type "${google_target}" TYPE)
        if(NOT google_type STREQUAL "EXECUTABLE")
            message(FATAL_ERROR
                "GoogleTest aggregate requires executable targets: ${google_target}")
        endif()
        list(APPEND google_executables "$<TARGET_FILE:${google_target}>")
    endforeach()
    list(JOIN google_executables "\",\n    \"" google_executables)
    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/google-contracts.json"
        CONTENT "{\n  \"schema\": 1,\n  \"executables\": [\n    \"${google_executables}\"\n  ]\n}\n")
endfunction()
cmake_language(DEFER CALL nexus_write_google_plan)
