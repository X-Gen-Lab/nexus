# Optional exact-profile targets, admitted through the assembly source identity.
# Defining these helpers creates neither a vendor target nor runtime storage.
include_guard(GLOBAL)
function(nexus_add_coremqtt)
    if(NOT ARGC EQUAL 0)
        message(FATAL_ERROR "nexus_add_coremqtt accepts no source or include arguments")
    endif()
    if(TARGET Nexus::CoreMQTT)
        return()
    endif()
    if(NOT TARGET Nexus::Config OR NOT TARGET Nexus::Core)
        message(FATAL_ERROR "coreMQTT requires a configured Nexus assembly")
    endif()
    get_target_property(nexus_mqtt_root Nexus::Config NEXUS_SOURCE_DIR)
    if(NOT nexus_mqtt_root OR "${nexus_mqtt_root}" MATCHES "-NOTFOUND$")
        message(FATAL_ERROR "coreMQTT requires the assembly source identity")
    endif()
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    set(nexus_mqtt_scope)
    get_property(nexus_mqtt_sdk GLOBAL PROPERTY NEXUS_SOURCE_SDK_SOURCE)
    if(nexus_mqtt_sdk AND nexus_mqtt_sdk STREQUAL nexus_mqtt_root)
        list(APPEND nexus_mqtt_scope --source-sdk)
    endif()
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -B
            "${nexus_mqtt_root}/tools/maintenance/coremqtt_lock.py"
            --source "${nexus_mqtt_root}/ext/coremqtt"
            --lock "${nexus_mqtt_root}/dependencies/coremqtt.lock.json"
            ${nexus_mqtt_scope}
        RESULT_VARIABLE nexus_mqtt_verified
        OUTPUT_VARIABLE nexus_mqtt_output
        ERROR_VARIABLE nexus_mqtt_error)
    if(NOT nexus_mqtt_verified EQUAL 0)
        message(FATAL_ERROR
            "coreMQTT source rejected: ${nexus_mqtt_output}${nexus_mqtt_error}")
    endif()
    string(JSON nexus_mqtt_count LENGTH "${nexus_mqtt_output}" configure_inputs)
    math(EXPR nexus_mqtt_last "${nexus_mqtt_count} - 1")
    foreach(index RANGE 0 ${nexus_mqtt_last})
        string(JSON input GET "${nexus_mqtt_output}" configure_inputs ${index})
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input}")
    endforeach()
    _nexus_add_coremqtt_impl("${nexus_mqtt_root}/ext/coremqtt"
        "${nexus_mqtt_root}/components/network/include")
endfunction()

function(_nexus_add_coremqtt_impl vendor_root byte_stream_include)
    if(TARGET Nexus::CoreMQTT)
        message(FATAL_ERROR "coreMQTT is already selected in this build scope")
    endif()
    foreach(policy Nexus::Core nexus_abi_options nexus_build_options
                   nexus_owned_warnings)
        if(NOT TARGET "${policy}")
            message(FATAL_ERROR "coreMQTT requires selected policy ${policy}")
        endif()
    endforeach()
    if(NOT COMMAND nexus_apply_target_policy)
        message(FATAL_ERROR "coreMQTT requires maintained target policy")
    endif()
    if(NOT IS_ABSOLUTE "${vendor_root}" OR
       NOT IS_ABSOLUTE "${byte_stream_include}")
        message(FATAL_ERROR "coreMQTT requires explicitly admitted absolute paths")
    endif()
    set(component "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    add_library(nexus_coremqtt_vendor STATIC EXCLUDE_FROM_ALL
        "${vendor_root}/source/core_mqtt.c"
        "${vendor_root}/source/core_mqtt_serializer.c"
        "${vendor_root}/source/core_mqtt_state.c")
    target_include_directories(nexus_coremqtt_vendor PRIVATE
        "${component}/include" "${component}/port" "${byte_stream_include}")
    target_include_directories(nexus_coremqtt_vendor SYSTEM PUBLIC
        "${vendor_root}/source/include" "${vendor_root}/source/interface")
    target_link_libraries(nexus_coremqtt_vendor PUBLIC nexus_abi_options
        PRIVATE nexus_build_options Nexus::Core)
    nexus_apply_target_policy(nexus_coremqtt_vendor)
    set_target_properties(nexus_coremqtt_vendor PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)

    add_library(nexus_mqtt STATIC EXCLUDE_FROM_ALL "${component}/src/client.c")
    target_include_directories(nexus_mqtt PUBLIC
        "${component}/include" "${byte_stream_include}")
    target_include_directories(nexus_mqtt PRIVATE "${component}/port")
    target_link_libraries(nexus_mqtt PUBLIC Nexus::Core
        PRIVATE nexus_coremqtt_vendor nexus_build_options nexus_owned_warnings)
    nexus_apply_target_policy(nexus_mqtt)
    set_target_properties(nexus_mqtt PROPERTIES
        C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    add_library(Nexus::CoreMQTT ALIAS nexus_mqtt)

    # Only the composition site opts into upstream layout/includes. Generic
    # users of client.h do not inherit vendor headers through CoreMQTT.
    add_library(nexus_mqtt_storage INTERFACE)
    target_link_libraries(nexus_mqtt_storage INTERFACE
        Nexus::CoreMQTT nexus_coremqtt_vendor)
    add_library(Nexus::CoreMQTTStorage ALIAS nexus_mqtt_storage)
endfunction()
