include("${CMAKE_CURRENT_LIST_DIR}/TargetPolicy.cmake")
nexus_require_build_generator()
if(TARGET Nexus::Config)
    message(FATAL_ERROR "One build may contain only one resolved Nexus assembly")
endif()
add_library(nexus_build_options INTERFACE)
add_library(Nexus::Config ALIAS nexus_build_options)
target_include_directories(nexus_build_options INTERFACE "${NEXUS_CONFIG_DIR}")
target_compile_features(nexus_build_options INTERFACE c_std_11)
set_target_properties(nexus_build_options PROPERTIES
    NEXUS_SOURCE_DIR "${NEXUS_SOURCE_DIR}" NEXUS_CONFIG_DIR "${NEXUS_CONFIG_DIR}")
add_library(nexus_abi_options INTERFACE)
add_library(nexus_owned_warnings INTERFACE)
# The sole CPU resolver owns ABI combinations and optional instruction flags.
# CMake consumes the validated canonical list, never guesses an FPU or port.
if(NOT NEXUS_CPU_ARCH STREQUAL "native" AND
   NOT NEXUS_CPU_ARCH MATCHES "^cortex-m(0|0plus|3|4|7|23|33|55|85)$")
    message(FATAL_ERROR "Unsupported CPU architecture: ${NEXUS_CPU_ARCH}")
endif()
if(NOT DEFINED NEXUS_CPU_COMPILE_OPTIONS)
    message(FATAL_ERROR "Missing validated CPU compile options")
endif()
set(cpu ${NEXUS_CPU_COMPILE_OPTIONS})
if(NEXUS_CPU_ARCH STREQUAL "native")
    if(cpu OR NOT NEXUS_ENUM_ABI STREQUAL "native-int")
        message(FATAL_ERROR "Unsupported Native ABI contract")
    endif()
elseif(NOT cpu OR NOT NEXUS_ENUM_ABI STREQUAL "short-enums")
    message(FATAL_ERROR "Missing validated Cortex-M ABI contract")
endif()
if(MSVC)
    target_compile_options(nexus_owned_warnings INTERFACE /W4 /WX)
else()
    target_compile_options(nexus_owned_warnings INTERFACE
        -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow
        "$<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes;-Wmissing-prototypes>")
endif()
if(NOT NEXUS_OPTIMIZATION MATCHES "^(O0|Og|Os|O2|O3|Os-lto|O2-lto|O3-lto)$")
    message(FATAL_ERROR "Unsupported resolved optimization: ${NEXUS_OPTIMIZATION}")
endif()
string(REGEX REPLACE "-lto$" "" optimize "${NEXUS_OPTIMIZATION}")
set_property(TARGET nexus_build_options PROPERTY NEXUS_LTO_REQUIRED OFF)
if(NOT MSVC)
    target_compile_options(nexus_build_options INTERFACE "-${optimize}"
        -ffunction-sections -fdata-sections)
endif()
if(NEXUS_OPTIMIZATION MATCHES "-lto$")
    include(CheckIPOSupported)
    set(saved_exe_flags "${CMAKE_EXE_LINKER_FLAGS}")
    set(saved_c_flags "${CMAKE_C_FLAGS}")
    if(cpu)
        string(JOIN " " cpu_flags ${cpu})
        string(APPEND CMAKE_EXE_LINKER_FLAGS
            " ${cpu_flags} --specs=nano.specs --specs=nosys.specs")
        string(APPEND CMAKE_C_FLAGS
            " ${cpu_flags} --specs=nano.specs --specs=nosys.specs")
    endif()
    set(ipo_languages C)
    if(CMAKE_CXX_COMPILER_LOADED)
        list(APPEND ipo_languages CXX)
    endif()
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error
        LANGUAGES ${ipo_languages})
    set(CMAKE_EXE_LINKER_FLAGS "${saved_exe_flags}")
    set(CMAKE_C_FLAGS "${saved_c_flags}")
    if(NOT ipo_supported)
        message(FATAL_ERROR "Selected LTO is unsupported: ${ipo_error}")
    endif()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    set_property(TARGET nexus_build_options PROPERTY NEXUS_LTO_REQUIRED ON)
else()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
endif()
option(NEXUS_COMPILER_CACHE "Use strict ccache for development compilation" OFF)
set(NEXUS_COMPILER_CACHE_DIR "" CACHE PATH "Development ccache directory")
set(nexus_compiler_launcher "")
if(NEXUS_COMPILER_CACHE)
    find_program(nexus_ccache ccache REQUIRED)
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    get_filename_component(policy_dir "${NEXUS_CONFIG_DIR}/../build-policy" ABSOLUTE)
    if(NOT NEXUS_COMPILER_CACHE_DIR)
        set(NEXUS_COMPILER_CACHE_DIR "${policy_dir}/compiler-cache")
    endif()
    get_filename_component(NEXUS_COMPILER_CACHE_DIR
        "${NEXUS_COMPILER_CACHE_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${NEXUS_SOURCE_DIR}/tools/dev/cache.py"
        "${NEXUS_SOURCE_DIR}/tools/evidence/common.py"
        "${NEXUS_SOURCE_DIR}/dependencies/environment.lock.json"
        "${NEXUS_SOURCE_DIR}/dependencies/toolchains.lock.json"
        "${NEXUS_SOURCE_DIR}/dependencies/platform-compatibility.json")
    get_filename_component(cache_compiler "${CMAKE_C_COMPILER}" REALPATH)
    set(cache_arguments --compiler "${cache_compiler}"
        "--flag=${NEXUS_OPTIMIZATION}"
        --dependency "${NEXUS_CONFIG_DIR}/resolved.json"
        --dependency "${NEXUS_SOURCE_DIR}/dependencies/environment.lock.json"
        --dependency "${NEXUS_SOURCE_DIR}/dependencies/toolchains.lock.json"
        --dependency "${NEXUS_SOURCE_DIR}/dependencies/platform-compatibility.json")
    foreach(flag IN LISTS cpu)
        list(APPEND cache_arguments "--flag=${flag}")
    endforeach()
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/dev/cache.py" namespace ${cache_arguments}
        --output "${policy_dir}/compiler-cache.json"
        RESULT_VARIABLE cache_result ERROR_VARIABLE cache_error)
    if(NOT cache_result EQUAL 0)
        message(FATAL_ERROR "Compiler cache policy rejected: ${cache_error}")
    endif()
    file(READ "${policy_dir}/compiler-cache.json" cache_identity)
    string(JSON cache_namespace GET "${cache_identity}" namespace)
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B
        "${NEXUS_SOURCE_DIR}/tools/dev/cache.py" environment
        --cache "${NEXUS_COMPILER_CACHE_DIR}" --namespace "${cache_namespace}"
        --output "${policy_dir}/compiler-cache-environment.json"
        RESULT_VARIABLE cache_result ERROR_VARIABLE cache_error)
    if(NOT cache_result EQUAL 0)
        message(FATAL_ERROR "Compiler cache environment rejected: ${cache_error}")
    endif()
    file(READ "${policy_dir}/compiler-cache-environment.json" cache_environment)
    set(nexus_compiler_launcher "${CMAKE_COMMAND}" -E env)
    foreach(key CCACHE_DIR CCACHE_NAMESPACE CCACHE_COMPILERCHECK
                CCACHE_SLOPPINESS CCACHE_HASHDIR)
        string(JSON value GET "${cache_environment}" "${key}")
        list(APPEND nexus_compiler_launcher "${key}=${value}")
    endforeach()
    list(APPEND nexus_compiler_launcher "${nexus_ccache}")
endif()
set_property(TARGET nexus_build_options PROPERTY NEXUS_COMPILER_LAUNCHER
    "${nexus_compiler_launcher}")
# Directory initialization covers owned libraries; public helpers apply the
# same property explicitly to targets defined by the external consumer.
set(CMAKE_C_COMPILER_LAUNCHER "${nexus_compiler_launcher}")
set(CMAKE_CXX_COMPILER_LAUNCHER "${nexus_compiler_launcher}")
if(cpu)
    if(NOT CMAKE_CROSSCOMPILING OR NOT CMAKE_C_COMPILER MATCHES "arm-none-eabi")
        message(FATAL_ERROR "The selected MCU requires cmake/toolchains/arm-gcc.cmake")
    endif()
    target_compile_options(nexus_abi_options INTERFACE ${cpu})
    target_link_options(nexus_abi_options INTERFACE ${cpu})
    target_link_libraries(nexus_build_options INTERFACE nexus_abi_options)
    target_link_options(nexus_build_options INTERFACE
        --specs=nano.specs --specs=nosys.specs -nostartfiles -Wl,--gc-sections)
endif()
option(NEXUS_ENABLE_SANITIZERS "Host ASan/UBSan instrumentation" OFF)
option(NEXUS_ENABLE_THREAD_SANITIZER "Host ThreadSanitizer instrumentation" OFF)
if(NEXUS_ENABLE_SANITIZERS AND NEXUS_ENABLE_THREAD_SANITIZER)
    message(FATAL_ERROR "ASan/UBSan and ThreadSanitizer are mutually exclusive")
endif()
if(NEXUS_ENABLE_SANITIZERS OR NEXUS_ENABLE_THREAD_SANITIZER)
    if(NOT NEXUS_CPU_ARCH STREQUAL "native" OR
       NOT CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$")
        message(FATAL_ERROR "Sanitizers require the maintained native GCC/Clang model")
    endif()
endif()
if(NEXUS_ENABLE_SANITIZERS)
    target_compile_options(nexus_build_options INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(nexus_build_options INTERFACE -fsanitize=address,undefined)
endif()
if(NEXUS_ENABLE_THREAD_SANITIZER)
    target_compile_options(nexus_build_options INTERFACE
        -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(nexus_build_options INTERFACE -fsanitize=thread)
endif()
