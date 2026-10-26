# Explicit application sources; startup and hardware binding are common platform.
include("${CMAKE_CURRENT_LIST_DIR}/TargetPolicy.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/H743FlashRam.cmake")

function(nexus_add_firmware target)
    foreach(keyword SOURCES LIBRARIES)
        set(keyword_count 0)
        foreach(argument IN LISTS ARGN)
            if(argument STREQUAL keyword)
                math(EXPR keyword_count "${keyword_count} + 1")
            endif()
        endforeach()
        if(keyword_count GREATER 1)
            message(FATAL_ERROR "nexus_add_firmware has repeated ${keyword}")
        endif()
    endforeach()
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBRARIES" ${ARGN})
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "nexus_add_firmware has unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    if(ARG_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR
            "nexus_add_firmware has missing values: ${ARG_KEYWORDS_MISSING_VALUES}")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "nexus_add_firmware requires explicit SOURCES")
    endif()
    if(NOT TARGET Nexus::Platform)
        message(FATAL_ERROR
            "nexus_add_firmware requires a configured Nexus::Platform")
    endif()
    if(TARGET "${target}")
        message(FATAL_ERROR "Firmware target ${target} already exists")
    endif()
    nexus_require_build_generator()
    foreach(property NEXUS_SOURCE_DIR NEXUS_CONFIG_DIR NEXUS_SOC_FAMILY
                     NEXUS_BACKEND NEXUS_COMPONENT_TARGETS NEXUS_STARTUP_SOURCE NEXUS_SYSTEM_SOURCE
                     NEXUS_SYSTEM_PRIVATE_INCLUDES NEXUS_SYSTEM_VENDOR_INCLUDES
                     NEXUS_SYSTEM_COMPILE_DEFINITIONS)
        get_target_property(${property} Nexus::Platform ${property})
        if("${${property}}" MATCHES "-NOTFOUND$")
            set(${property} "")
        endif()
    endforeach()
    foreach(property NEXUS_SOURCE_DIR NEXUS_CONFIG_DIR NEXUS_SOC_FAMILY
                     NEXUS_BACKEND)
        if(NOT ${property})
            message(FATAL_ERROR "Nexus::Platform is missing ${property}")
        endif()
    endforeach()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_executable(${target} ${ARG_SOURCES})
    nexus_apply_target_policy(${target})
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
    target_link_libraries(${target} PRIVATE Nexus::Platform nexus_build_options
        Nexus::Arch "nexus_os_${NEXUS_BACKEND}"
        ${NEXUS_COMPONENT_TARGETS} ${ARG_LIBRARIES})
    if(NOT NEXUS_SOC_FAMILY STREQUAL "native")
        # Compile startup/system in their own private include/definition scope.
        # Product source files see only typed public Nexus interfaces.
        set(startup_target "${target}_nexus_startup")
        add_library(${startup_target} OBJECT "${NEXUS_STARTUP_SOURCE}"
            ${NEXUS_SYSTEM_SOURCE})
        nexus_apply_target_policy(${startup_target})
        target_link_libraries(${startup_target} PRIVATE Nexus::IO Nexus::Arch
            nexus_build_options nexus_owned_warnings)
        target_include_directories(${startup_target} PRIVATE ${NEXUS_SYSTEM_PRIVATE_INCLUDES})
        target_include_directories(${startup_target} SYSTEM PRIVATE ${NEXUS_SYSTEM_VENDOR_INCLUDES})
        nexus_use_cmsis_core(${startup_target})
        target_compile_definitions(${startup_target} PRIVATE ${NEXUS_SYSTEM_COMPILE_DEFINITIONS})
        target_sources(${target} PRIVATE $<TARGET_OBJECTS:${startup_target}>)
        set_target_properties(${target} PROPERTIES SUFFIX ".elf")
        set(map "${CMAKE_CURRENT_BINARY_DIR}/bin/${target}.map")
        set(binary "${CMAKE_CURRENT_BINARY_DIR}/bin/${target}.bin")
        set(resources "${CMAKE_CURRENT_BINARY_DIR}/bin/${target}.resources.json")
        target_link_options(${target} PRIVATE "-T${NEXUS_CONFIG_DIR}/memory.ld"
            "-Wl,-Map,${map}" "-Wl,--fatal-warnings")
        # A direct --target firmware must enforce its budget as well. Declaring
        # byproducts makes Ninja restore removed artifacts, while LINK_DEPENDS
        # rechecks modified inputs and checker code through that same rule.
        set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS
            "${NEXUS_CONFIG_DIR}/memory.ld"
            "${NEXUS_CONFIG_DIR}/resolved.json"
            "${NEXUS_CONFIG_DIR}/input_paths.json"
            "${NEXUS_CONFIG_DIR}/resource-budget.json"
            "${NEXUS_SOURCE_DIR}/tools/measurement/resources.py"
            "${NEXUS_SOURCE_DIR}/tools/measurement/elf.py"
            "${NEXUS_SOURCE_DIR}/tools/evidence/common.py"
            "${NEXUS_SOURCE_DIR}/tools/evidence/identity.py"
            "${NEXUS_SOURCE_DIR}/tools/configure/memory.py")
        add_custom_command(TARGET ${target} POST_BUILD
            BYPRODUCTS "${binary}" "${map}" "${resources}"
            COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:${target}>"
                "${binary}"
            COMMAND "${Python3_EXECUTABLE}" -B
                "${NEXUS_SOURCE_DIR}/tools/measurement/resources.py"
                --elf "$<TARGET_FILE:${target}>"
                --resolved "${NEXUS_CONFIG_DIR}/resolved.json"
                --require-cpu
                --budget "${NEXUS_CONFIG_DIR}/resource-budget.json"
                --report "${resources}"
            VERBATIM)
        nexus_h743_flash_ram_audit(${target} "${NEXUS_SOURCE_DIR}"
            "${NEXUS_CONFIG_DIR}" "${NEXUS_SOC_FAMILY}")
    endif()
endfunction()
