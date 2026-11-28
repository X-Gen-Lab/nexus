cmake_minimum_required(VERSION 3.31)
project(nexus_stm32f100rb_cec_fixture LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NEXUS_SOURCE_ROOT)
    set(Nexus_DIR "${NEXUS_SOURCE_ROOT}/cmake")
endif()
find_package(Nexus CONFIG REQUIRED)
nexus_add_platform(ASSEMBLY "${CMAKE_CURRENT_LIST_DIR}/f100rb-cec.toml")
get_target_property(platform_source Nexus::Config NEXUS_SOURCE_DIR)
set(f1 "${platform_source}/soc/stm32/stm32f1")

nexus_add_firmware(
    nexus_stm32f100rb_cec
    SOURCES main.c "${f1}/drivers/cec/cec.c" "${f1}/drivers/gpio/gpio.c"
)
target_include_directories(
    nexus_stm32f100rb_cec
    PRIVATE "${f1}" "${platform_source}/io/private"
)
target_include_directories(
    nexus_stm32f100rb_cec
    SYSTEM
    PRIVATE
        "${platform_source}/vendors/st/cmsis_device_f1/Include"
        "${platform_source}/vendors/st/stm32f1_hal/Inc"
)
target_compile_definitions(nexus_stm32f100rb_cec PRIVATE STM32F100xB)
include("${platform_source}/cmake/platform/CMSISCore.cmake")
nexus_use_cmsis_core(nexus_stm32f100rb_cec)
nexus_target_warnings(nexus_stm32f100rb_cec STRICT)
