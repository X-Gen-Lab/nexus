# Explicit application sources; startup and hardware binding are common platform.
function(nexus_add_firmware target)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBRARIES" ${ARGN})
    if(ARG_UNPARSED_ARGUMENTS OR NOT ARG_SOURCES)
        message(FATAL_ERROR "nexus_add_firmware requires explicit SOURCES")
    endif()
    foreach(property NEXUS_SOURCE_DIR NEXUS_CONFIG_DIR NEXUS_SOC_FAMILY
                     NEXUS_BACKEND NEXUS_COMPONENT_TARGETS NEXUS_STARTUP_SOURCE NEXUS_SYSTEM_SOURCE
                     NEXUS_SYSTEM_PRIVATE_INCLUDES NEXUS_SYSTEM_VENDOR_INCLUDES
                     NEXUS_SYSTEM_COMPILE_DEFINITIONS)
        get_target_property(${property} Nexus::Platform ${property})
    endforeach()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_executable(${target} ${ARG_SOURCES})
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
    target_link_libraries(${target} PRIVATE Nexus::Platform nexus_build_options
        nexus_owned_warnings Nexus::Arch "nexus_os_${NEXUS_BACKEND}"
        ${NEXUS_COMPONENT_TARGETS} ${ARG_LIBRARIES})
    if(NOT NEXUS_SOC_FAMILY STREQUAL "native")
        # Compile startup/system in their own private include/definition scope.
        # Product source files see only typed public Nexus interfaces.
        set(startup_target "${target}_nexus_startup")
        add_library(${startup_target} OBJECT "${NEXUS_STARTUP_SOURCE}"
            "${NEXUS_SYSTEM_SOURCE}")
        target_link_libraries(${startup_target} PRIVATE Nexus::IO Nexus::Arch
            nexus_build_options nexus_owned_warnings)
        target_include_directories(${startup_target} PRIVATE ${NEXUS_SYSTEM_PRIVATE_INCLUDES})
        target_include_directories(${startup_target} SYSTEM PRIVATE ${NEXUS_SYSTEM_VENDOR_INCLUDES})
        target_compile_definitions(${startup_target} PRIVATE ${NEXUS_SYSTEM_COMPILE_DEFINITIONS})
        target_sources(${target} PRIVATE $<TARGET_OBJECTS:${startup_target}>)
        set_target_properties(${target} PROPERTIES SUFFIX ".elf")
        set(map "${CMAKE_CURRENT_BINARY_DIR}/bin/${target}.map")
        target_link_options(${target} PRIVATE "-T${NEXUS_CONFIG_DIR}/memory.ld"
            "-Wl,-Map,${map}" "-Wl,--fatal-warnings")
        set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS
            "${NEXUS_CONFIG_DIR}/memory.ld")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:${target}>"
                "$<TARGET_FILE_DIR:${target}>/${target}.bin"
            COMMAND "${Python3_EXECUTABLE}" -B
                "${NEXUS_SOURCE_DIR}/tools/measurement/resources.py"
                --elf "$<TARGET_FILE:${target}>"
                --resolved "${NEXUS_CONFIG_DIR}/resolved.json"
                --budget "${NEXUS_CONFIG_DIR}/resource-budget.json"
                --report "$<TARGET_FILE_DIR:${target}>/${target}.resources.json"
            VERBATIM)
    endif()
endfunction()
