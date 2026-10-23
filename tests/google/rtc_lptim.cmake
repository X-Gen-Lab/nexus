# Native-only source contracts; no physical RTC/LPTIM claim.
nexus_google_test(nexus_rtc_lptim_test SOURCES rtc_lptim_test.cpp
    LIBRARIES Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/native/drivers")
if(UNIX AND NOT APPLE)
    nexus_google_test(nexus_rtc_lptim_final_access_test
        SOURCES rtc_lptim_final_access_test.cpp
        LIBRARIES Nexus::NativeModel
        INCLUDES "${NEXUS_SOURCE_DIR}/soc/native/drivers")
endif()

nexus_google_test(nexus_rtc_lptim_start_test SOURCES rtc_lptim_start_test.cpp
    LIBRARIES Nexus::NativeModel
    INCLUDES "${NEXUS_SOURCE_DIR}/soc/native/drivers")

# The same canonical public C scenarios are consumed by the installed SDK.
foreach(nx_time_profile pair empty)
    set(nx_time_input "${NEXUS_SOURCE_DIR}/cmake/package/verification/io_evolution/native-time-${nx_time_profile}.toml")
    set(nx_time_bundle "${CMAKE_CURRENT_BINARY_DIR}/native-time-${nx_time_profile}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/configure/configure.py"
        --source-root "${NEXUS_SOURCE_DIR}" --assembly "${nx_time_input}"
        --output "${nx_time_bundle}" RESULT_VARIABLE nx_time_generated
        OUTPUT_VARIABLE nx_time_output ERROR_VARIABLE nx_time_error)
    if(NOT nx_time_generated EQUAL 0)
        message(FATAL_ERROR "Native time fixture rejected: ${nx_time_output}${nx_time_error}")
    endif()
    file(READ "${nx_time_bundle}/input_paths.json" nx_time_inputs)
    string(JSON nx_time_count LENGTH "${nx_time_inputs}")
    math(EXPR nx_time_last "${nx_time_count} - 1")
    foreach(index RANGE 0 ${nx_time_last})
        string(JSON nx_time_key MEMBER "${nx_time_inputs}" ${index})
        string(JSON nx_time_path GET "${nx_time_inputs}" "${nx_time_key}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${nx_time_path}")
    endforeach()
    set(nx_time_consumer "${NEXUS_SOURCE_DIR}/cmake/package/verification/io_evolution/consumers/native-time-${nx_time_profile}.c")
    set_source_files_properties("${nx_time_consumer}" PROPERTIES
        COMPILE_DEFINITIONS "main=nx_native_time_consumer")
    nexus_google_test(nexus_rtc_lptim_${nx_time_profile}_generated_test
        SOURCES rtc_lptim_generated_test.cpp "${nx_time_consumer}"
            "${nx_time_bundle}/bindings.c"
        LIBRARIES Nexus::NativeModel
        INCLUDES "${nx_time_bundle}" "${NEXUS_SOURCE_DIR}/soc/native/drivers")
endforeach()
