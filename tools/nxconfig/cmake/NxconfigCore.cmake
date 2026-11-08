include_guard(GLOBAL)
get_filename_component(NXCONFIG_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.."
    ABSOLUTE)

function(nxconfig_verify_parser name directory)
    set(lock "${NXCONFIG_SOURCE_ROOT}/dependencies/${name}.lock.json")
    file(READ "${lock}" identity)
    string(JSON count LENGTH "${identity}" files)
    if(name STREQUAL "tomlc99")
        set(required LICENSE toml.c toml.h)
    elseif(name STREQUAL "cjson")
        set(required LICENSE cJSON.c cJSON.h)
    else()
        message(FATAL_ERROR "Unknown parser identity: ${name}")
    endif()
    if(NOT count EQUAL 3)
        message(FATAL_ERROR "Expected exact parser input set for ${name}")
    endif()
    math(EXPR last "${count} - 1")
    set(recorded)
    foreach(index RANGE 0 ${last})
        string(JSON file MEMBER "${identity}" files ${index})
        list(APPEND recorded "${file}")
    endforeach()
    list(SORT recorded)
    list(SORT required)
    if(NOT recorded STREQUAL required)
        message(FATAL_ERROR "Expected exact parser input set for ${name}")
    endif()
    foreach(index RANGE 0 ${last})
        string(JSON file MEMBER "${identity}" files ${index})
        set(source "${directory}/${file}")
        if(NOT EXISTS "${source}" OR IS_SYMLINK "${source}")
            message(FATAL_ERROR "Missing regular locked parser input: ${source}")
        endif()
        string(JSON bytes GET "${identity}" files "${file}" bytes)
        string(JSON expected GET "${identity}" files "${file}" sha256)
        file(SIZE "${source}" actual_bytes)
        file(SHA256 "${source}" actual)
        if(NOT actual_bytes EQUAL bytes OR NOT actual STREQUAL expected)
            message(FATAL_ERROR "Locked parser bytes changed: ${source}")
        endif()
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${source}")
    endforeach()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${lock}")
endfunction()

nxconfig_verify_parser(tomlc99 "${NXCONFIG_SOURCE_ROOT}/ext/tomlc99")
nxconfig_verify_parser(cjson "${NXCONFIG_SOURCE_ROOT}/ext/cjson")
add_library(nxconfig_toml STATIC "${NXCONFIG_SOURCE_ROOT}/ext/tomlc99/toml.c")
target_include_directories(nxconfig_toml SYSTEM PUBLIC
    "${NXCONFIG_SOURCE_ROOT}/ext/tomlc99")
add_library(nxconfig_json STATIC "${NXCONFIG_SOURCE_ROOT}/ext/cjson/cJSON.c")
target_include_directories(nxconfig_json SYSTEM PUBLIC
    "${NXCONFIG_SOURCE_ROOT}/ext/cjson")

add_library(nxconfig_core STATIC
    "${NXCONFIG_SOURCE_ROOT}/tools/nxconfig/src/config.c"
    "${NXCONFIG_SOURCE_ROOT}/tools/nxconfig/src/hash.c"
    "${NXCONFIG_SOURCE_ROOT}/tools/nxconfig/src/emit.c")
target_include_directories(nxconfig_core PUBLIC
    "${NXCONFIG_SOURCE_ROOT}/tools/nxconfig/include")
target_link_libraries(nxconfig_core PRIVATE nxconfig_toml nxconfig_json)
target_compile_options(nxconfig_core PRIVATE
    -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow
    -Wstrict-prototypes -Wmissing-prototypes)

set_target_properties(nxconfig_core nxconfig_toml nxconfig_json PROPERTIES
    C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)

# Validation explicitly requires the prepared SDK compiler; bootstrap does not.
function(nxconfig_contract_settings target)
    target_link_libraries(${target} PRIVATE nxconfig_toml nxconfig_json)
    find_program(NXCONFIG_TEST_ARM_CC NAMES arm-none-eabi-gcc REQUIRED)
    target_compile_definitions(${target} PRIVATE
        "NXCONFIG_REPO_ROOT=\"${NXCONFIG_SOURCE_ROOT}\""
        "NXCONFIG_TEST_CMAKE=\"${CMAKE_COMMAND}\""
        "NXCONFIG_TEST_NINJA=\"${CMAKE_MAKE_PROGRAM}\""
        "NXCONFIG_TEST_CXX=\"${CMAKE_CXX_COMPILER}\""
        "NXCONFIG_TEST_ARM_CC_DEFAULT=\"${NXCONFIG_TEST_ARM_CC}\"")
endfunction()
