add_library(nexus_build_options INTERFACE)
add_library(Nexus::Config ALIAS nexus_build_options)
target_include_directories(nexus_build_options INTERFACE "${NEXUS_CONFIG_DIR}")
target_compile_features(nexus_build_options INTERFACE c_std_11)
set_target_properties(nexus_build_options PROPERTIES
    NEXUS_SOURCE_DIR "${NEXUS_SOURCE_DIR}" NEXUS_CONFIG_DIR "${NEXUS_CONFIG_DIR}")
add_library(nexus_abi_options INTERFACE)
add_library(nexus_owned_warnings INTERFACE)
# CPU facts are validated by the maintained provider and consumed explicitly.
# A new family cannot silently inherit the current Cortex-M4 ABI.
if(NEXUS_CPU_ARCH STREQUAL "native")
    if(NOT NEXUS_CPU_FPU STREQUAL "none" OR
       NOT NEXUS_FLOAT_ABI STREQUAL "native" OR
       NOT NEXUS_ENUM_ABI STREQUAL "native-int")
        message(FATAL_ERROR "Unsupported Native ABI contract")
    endif()
    set(cpu)
elseif(NEXUS_CPU_ARCH STREQUAL "cortex-m4")
    if(NOT NEXUS_CPU_FPU STREQUAL "fpv4-sp-d16" OR
       NOT NEXUS_FLOAT_ABI STREQUAL "hard" OR
       NOT NEXUS_ENUM_ABI STREQUAL "short-enums")
        message(FATAL_ERROR "Unsupported Cortex-M4 ABI contract")
    endif()
    set(cpu "-mcpu=${NEXUS_CPU_ARCH}" -mthumb
        "-mfpu=${NEXUS_CPU_FPU}" "-mfloat-abi=${NEXUS_FLOAT_ABI}" -fshort-enums)
else()
    message(FATAL_ERROR "Unsupported CPU architecture: ${NEXUS_CPU_ARCH}")
endif()
if(MSVC)
    target_compile_options(nexus_owned_warnings INTERFACE /W4 /WX)
else()
    target_compile_options(nexus_owned_warnings INTERFACE
        -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow
        "$<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes;-Wmissing-prototypes>")
endif()
string(REGEX REPLACE "-lto$" "" optimize "${NEXUS_OPTIMIZATION}")
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
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error LANGUAGES C)
    set(CMAKE_EXE_LINKER_FLAGS "${saved_exe_flags}")
    set(CMAKE_C_FLAGS "${saved_c_flags}")
    if(NOT ipo_supported)
        message(FATAL_ERROR "Selected LTO is unsupported: ${ipo_error}")
    endif()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()
if(cpu)
    if(NOT CMAKE_CROSSCOMPILING OR NOT CMAKE_C_COMPILER MATCHES "arm-none-eabi")
        message(FATAL_ERROR "The selected MCU requires cmake/toolchains/arm-gcc.cmake")
    endif()
    target_compile_options(nexus_abi_options INTERFACE ${cpu})
    target_link_options(nexus_abi_options INTERFACE ${cpu})
    target_link_libraries(nexus_build_options INTERFACE nexus_abi_options)
    target_link_options(nexus_build_options INTERFACE ${cpu}
        --specs=nano.specs --specs=nosys.specs -nostartfiles -Wl,--gc-sections)
endif()
option(NEXUS_ENABLE_SANITIZERS "Host ASan/UBSan instrumentation" OFF)
if(NEXUS_ENABLE_SANITIZERS)
    if(NOT NEXUS_CPU_ARCH STREQUAL "native" OR MSVC)
        message(FATAL_ERROR "Sanitizers require the maintained native GCC/Clang model")
    endif()
    target_compile_options(nexus_build_options INTERFACE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(nexus_build_options INTERFACE -fsanitize=address,undefined)
endif()
