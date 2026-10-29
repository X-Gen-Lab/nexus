# Offline host-only framework shared by Native contracts and HAL comparison.
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/GeneratedConfiguration.cmake")
get_filename_component(_nexus_google_source
    "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# A macro keeps enable_language's compiler facts in the caller's directory scope.
macro(nexus_host_google_test)
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR "GoogleTest/GoogleMock require a host test build")
    endif()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(_nexus_google_lock
        "${_nexus_google_source}/dependencies/googletest.lock.json")
    set(_nexus_google_verifier
        "${_nexus_google_source}/tools/maintenance/googletest_lock.py")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${_nexus_google_verifier}"
        --source "${_nexus_google_source}/ext/googletest"
        --lock "${_nexus_google_lock}"
        RESULT_VARIABLE _nexus_google_result
        OUTPUT_VARIABLE _nexus_google_identity
        ERROR_VARIABLE _nexus_google_error)
    if(NOT _nexus_google_result EQUAL 0)
        message(FATAL_ERROR
            "GoogleTest/GoogleMock identity failed: ${_nexus_google_error}")
    endif()
    # Validate before enabling C++ so firmware cannot acquire the host runtime.
    enable_language(CXX)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${_nexus_google_lock}" "${_nexus_google_verifier}"
        "${_nexus_google_source}/.gitmodules"
        "${_nexus_google_source}/ext/googletest/.git"
        "${_nexus_google_source}/tools/maintenance/source_catalogue.py"
        "${_nexus_google_source}/tools/configure/metadata.py")
    string(JSON _nexus_google_count LENGTH
        "${_nexus_google_identity}" configure_inputs)
    math(EXPR _nexus_google_last "${_nexus_google_count} - 1")
    foreach(_nexus_google_index RANGE 0 ${_nexus_google_last})
        string(JSON _nexus_google_input GET
            "${_nexus_google_identity}" configure_inputs ${_nexus_google_index})
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${_nexus_google_input}")
    endforeach()
    set(INSTALL_GTEST OFF CACHE BOOL "GoogleTest is test-only" FORCE)
    set(BUILD_GMOCK ON CACHE BOOL "GoogleMock contract tests" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "Host MSVC runtime" FORCE)
    add_subdirectory("${_nexus_google_source}/ext/googletest"
        "${CMAKE_BINARY_DIR}/_deps/google-build" EXCLUDE_FROM_ALL)
endmacro()
