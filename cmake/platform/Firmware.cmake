# Explicit application sources; startup and hardware binding are common platform.
include("${CMAKE_CURRENT_LIST_DIR}/TargetPolicy.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/Configure.cmake")

# The selected platform publishes these properties across consumer directories.
function(_nexus_firmware_properties required optional)
    set(${required}
        NEXUS_SOURCE_DIR
        NEXUS_CONFIG_DIR
        NEXUS_SOC_FAMILY
        NEXUS_CPU_ARCH
        NEXUS_BACKEND
        PARENT_SCOPE
    )
    set(${optional}
        NEXUS_COMPONENT_TARGETS
        NEXUS_STARTUP_SOURCE
        NEXUS_SYSTEM_SOURCE
        NEXUS_SYSTEM_PRIVATE_INCLUDES
        NEXUS_SYSTEM_VENDOR_INCLUDES
        NEXUS_SYSTEM_COMPILE_DEFINITIONS
        PARENT_SCOPE
    )
endfunction()

# Bind the configuration once per platform. The independent measurement CLI
# keeps replaying authoring, including when a consumer replaces the checker.
function(_nexus_firmware_checker output support)
    set(checker "${NEXUS_SOURCE_DIR}/tools/measurement/resources.py")
    cmake_path(GET CMAKE_CURRENT_FUNCTION_LIST_DIR PARENT_PATH cmake_dir)
    cmake_path(GET cmake_dir PARENT_PATH owner)
    if(
        NOT NEXUS_SOURCE_DIR STREQUAL owner
        OR EXISTS "${NEXUS_SOURCE_DIR}/.nexus-source-sdk.json"
    )
        set(${output} "${checker}" PARENT_SCOPE)
        return()
    endif()
    set(driver "${NEXUS_SOURCE_DIR}/tools/measurement/build_resources.py")
    get_property(
        anchor
        TARGET Nexus::Platform
        PROPERTY NEXUS_FIRMWARE_PROJECTION
    )
    if(NOT anchor)
        set_property(
            DIRECTORY
            APPEND
            PROPERTY
                CMAKE_CONFIGURE_DEPENDS
                    "${driver}"
                    "${checker}"
                    "${NEXUS_SOURCE_DIR}/tools/measurement/elf.py"
                    "${NEXUS_SOURCE_DIR}/tools/evidence/common.py"
                    "${NEXUS_SOURCE_DIR}/tools/evidence/identity.py"
        )
        set(options)
        if(support)
            list(APPEND options --allow-reference)
        endif()
        execute_process(
            COMMAND
                "${Python3_EXECUTABLE}" -B "${driver}" --bind
                "${NEXUS_CONFIG_DIR}/resolved.json" ${options}
            COMMAND_ERROR_IS_FATAL ANY
            OUTPUT_VARIABLE anchor
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        get_target_property(platform Nexus::Platform ALIASED_TARGET)
        set_property(
            TARGET "${platform}"
            PROPERTY NEXUS_FIRMWARE_PROJECTION "${anchor}"
        )
    endif()
    set(${output} "${driver};${anchor}" PARENT_SCOPE)
endfunction()

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
        message(
            FATAL_ERROR
            "nexus_add_firmware has unknown arguments: ${ARG_UNPARSED_ARGUMENTS}"
        )
    endif()
    if(ARG_KEYWORDS_MISSING_VALUES)
        message(
            FATAL_ERROR
            "nexus_add_firmware has missing values: ${ARG_KEYWORDS_MISSING_VALUES}"
        )
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "nexus_add_firmware requires explicit SOURCES")
    endif()
    if(NOT TARGET Nexus::Platform)
        message(
            FATAL_ERROR
            "nexus_add_firmware requires a configured Nexus::Platform"
        )
    endif()
    if(TARGET "${target}")
        message(FATAL_ERROR "Firmware target ${target} already exists")
    endif()
    nexus_require_build_generator()
    _nexus_firmware_properties(required optional)
    foreach(property IN LISTS required optional)
        get_property(${property} TARGET Nexus::Platform PROPERTY ${property})
        if(property IN_LIST required AND NOT ${property})
            message(FATAL_ERROR "Nexus::Platform is missing ${property}")
        endif()
    endforeach()
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    add_executable(${target} ${ARG_SOURCES})
    nexus_apply_target_policy(${target})
    set_target_properties(
        ${target}
        PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
    )
    target_link_libraries(
        ${target}
        PRIVATE
            Nexus::Platform
            nexus_build_options
            Nexus::Arch
            "nexus_os_${NEXUS_BACKEND}"
            ${NEXUS_COMPONENT_TARGETS}
            ${ARG_LIBRARIES}
    )
    if(NOT NEXUS_CPU_ARCH STREQUAL "native")
        # Compile startup/system in their own private include/definition scope.
        # Product source files see only typed public Nexus interfaces.
        set(startup_target "${target}_nexus_startup")
        add_library(
            ${startup_target}
            OBJECT
            "${NEXUS_STARTUP_SOURCE}"
            ${NEXUS_SYSTEM_SOURCE}
        )
        nexus_apply_target_policy(${startup_target})
        target_link_libraries(
            ${startup_target}
            PRIVATE
                Nexus::IO
                Nexus::Arch
                nexus_build_options
                nexus_owned_warnings
        )
        target_include_directories(
            ${startup_target}
            PRIVATE ${NEXUS_SYSTEM_PRIVATE_INCLUDES}
        )
        target_include_directories(
            ${startup_target}
            SYSTEM
            PRIVATE ${NEXUS_SYSTEM_VENDOR_INCLUDES}
        )
        nexus_use_cmsis_core(${startup_target})
        target_compile_definitions(
            ${startup_target}
            PRIVATE ${NEXUS_SYSTEM_COMPILE_DEFINITIONS}
        )
        target_sources(${target} PRIVATE $<TARGET_OBJECTS:${startup_target}>)
        set_target_properties(${target} PROPERTIES SUFFIX ".elf")
        set(map "${CMAKE_CURRENT_BINARY_DIR}/bin/${target}.map")
        set(binary "${CMAKE_CURRENT_BINARY_DIR}/bin/${target}.bin")
        set(resources
            "${CMAKE_CURRENT_BINARY_DIR}/bin/${target}.resources.json"
        )
        target_link_options(
            ${target}
            PRIVATE
                "-T${NEXUS_CONFIG_DIR}/memory.ld"
                "-Wl,-Map,${map}"
                "-Wl,--fatal-warnings"
        )
        # The checker reconstructs the selection with the same Python modules
        # as the resolver. Changes to these inputs need a fresh measurement.
        set(checker_catalog "${NEXUS_CONFIG_DIR}/configuration_code_paths.json")
        _nexus_read_configuration_catalog("${checker_catalog}" checker_modules checker_labels)
        # A consumer may select a relocated checker tree for artifact contracts.
        # Track both resolver bytes and corresponding selected checker modules.
        foreach(module IN LISTS checker_labels)
            if(
                NOT IS_ABSOLUTE "${module}"
                AND NOT module MATCHES "(^|/)\\.\\.(/|$)"
            )
                list(APPEND checker_modules "${NEXUS_SOURCE_DIR}/${module}")
            endif()
        endforeach()
        list(REMOVE_DUPLICATES checker_modules)
        set(stamp
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${target}.budget-inputs"
        )
        set(checker "${NEXUS_SOURCE_DIR}/tools/measurement/resources.py")
        set(image "$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_NAME:${target}>")
        set(recheck "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RecheckFirmware.cmake")
        get_property(
            test_support
            TARGET Nexus::Config
            PROPERTY NEXUS_TEST_SUPPORT
        )
        _nexus_firmware_checker(checker_command "${test_support}")
        set(check_arguments
            -B
            ${checker_command}
            --elf
            "${image}"
            --resolved
            "${NEXUS_CONFIG_DIR}/resolved.json"
            --require-cpu
            --budget
            "${NEXUS_CONFIG_DIR}/resource-budget.json"
            --report
            "${resources}"
        )
        if(test_support)
            list(APPEND check_arguments --allow-reference)
        endif()
        add_custom_command(
            OUTPUT "${stamp}"
            COMMAND
                "${CMAKE_COMMAND}" "-Dstamp=${stamp}" "-Dimage=${image}"
                "-Dcommand=${Python3_EXECUTABLE};${check_arguments}" -P
                "${recheck}"
            DEPENDS
                "${recheck}"
                "${checker}"
                "${NEXUS_CONFIG_DIR}/resolved.json"
                "${NEXUS_CONFIG_DIR}/input_paths.json"
                "${NEXUS_CONFIG_DIR}/generator-identity.json"
                "${NEXUS_CONFIG_DIR}/resource-budget.json"
                "${NEXUS_SOURCE_DIR}/tools/measurement/elf.py"
                "${NEXUS_SOURCE_DIR}/tools/evidence/common.py"
                "${NEXUS_SOURCE_DIR}/tools/evidence/identity.py"
                "${checker_catalog}"
                ${checker_modules}
            COMMENT "Checking firmware budget inputs for ${target}"
            VERBATIM
        )
        add_custom_target(${target}_nexus_budget_inputs DEPENDS "${stamp}")
        add_dependencies(${target} ${target}_nexus_budget_inputs)
        set(finalize
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/FinalizeFirmware.cmake"
        )
        set_property(
            TARGET ${target}
            APPEND
            PROPERTY
                LINK_DEPENDS
                    "${NEXUS_CONFIG_DIR}/memory.ld"
                    "${stamp}"
                    "${finalize}"
        )
        # A terminated link cannot retain its previous publishable byproducts.
        add_custom_command(
            TARGET ${target}
            PRE_LINK
            COMMAND "${CMAKE_COMMAND}" -E rm -f "${binary}" "${resources}"
            VERBATIM
        )
        add_custom_command(
            TARGET ${target}
            POST_BUILD
            BYPRODUCTS "${binary}" "${map}" "${resources}"
            COMMAND
                "${CMAKE_COMMAND}" "-Dimage=${image}" "-Dbinary=${binary}"
                "-Dmap=${map}" "-Dreport=${resources}"
                "-Dobjcopy=${CMAKE_OBJCOPY}"
                "-Dcommand=${Python3_EXECUTABLE};${check_arguments}" -P
                "${finalize}"
            VERBATIM
        )
    endif()
endfunction()
