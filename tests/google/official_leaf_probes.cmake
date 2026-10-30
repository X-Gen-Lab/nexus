# Actual production defaults and pinned inline bodies share maintained flags.
function(nexus_official_leaf_test name family part model)
    cmake_parse_arguments(LEAF "" "" "SOURCES;INCLUDES;DEFINITIONS" ${ARGN})
    nexus_google_test(${name}
        SOURCES ${LEAF_SOURCES}
        LIBRARIES Nexus::IO Nexus::Arch
        INCLUDES "${NEXUS_SOURCE_DIR}/tests/google" ${LEAF_INCLUDES})
    target_include_directories(${name} SYSTEM PRIVATE
        "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_${family}/Include"
        "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
    nexus_use_cmsis_core(${name})
    target_compile_definitions(${name} PRIVATE ${part} ${LEAF_DEFINITIONS})
    target_compile_options(${name} PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/${model}")
    nexus_stm32_official(${name} ${family})
endfunction()

set(h7_leaf "${NEXUS_SOURCE_DIR}/soc/stm32/stm32h743")
nexus_official_leaf_test(nexus_h743_spi_byte_test h7 STM32H743xx
    h743_spi_byte_model.h
    SOURCES h743_spi_byte_test.cpp h743_model.cpp
        "${NEXUS_SOURCE_DIR}/tests/contracts/h743_spi_byte_port.c"
        "${h7_leaf}/system.c" "${h7_leaf}/platform_cold.c"
        "${h7_leaf}/drivers/gpio.c" "${h7_leaf}/drivers/spi_wire.c"
        "${h7_leaf}/drivers/spi.c"
    INCLUDES "${h7_leaf}/private")
nexus_official_leaf_test(nexus_h743_spi_dma_byte_test h7 STM32H743xx
    h743_spi_dma_byte_model.h
    SOURCES h743_spi_dma_byte_test.cpp h743_spi_dma_model.cpp h743_model.cpp
        "${h7_leaf}/system.c" "${h7_leaf}/platform_cold.c"
        "${h7_leaf}/drivers/gpio.c" "${h7_leaf}/drivers/spi_wire.c"
        "${h7_leaf}/drivers/spi_dma.c"
    INCLUDES "${h7_leaf}/private")

set(u5_leaf "${NEXUS_SOURCE_DIR}/soc/stm32/stm32u575")
foreach(unit pwm uart rtc)
    set(sources "u575_${unit}_leaf_test.cpp"
        "${NEXUS_SOURCE_DIR}/tests/contracts/u575_${unit}_leaf_port.c"
        "${u5_leaf}/drivers/${unit}.c")
    if(unit STREQUAL "rtc")
        list(APPEND sources u575_rtc_model.cpp)
    else()
        list(APPEND sources u575_model.cpp "${u5_leaf}/drivers/gpio.c"
            "${u5_leaf}/system.c")
        if(unit STREQUAL "pwm")
            list(APPEND sources u575_devices_model.cpp)
        endif()
    endif()
    nexus_official_leaf_test(nexus_u575_${unit}_leaf_test u5 STM32U575xx
        "u575_${unit}_leaf_model.h" SOURCES ${sources}
        INCLUDES "${u5_leaf}/private" "${NEXUS_SOURCE_DIR}/io/private")
endforeach()
nexus_official_leaf_test(nexus_u575_gpio_command_test u5 STM32U575xx
    u575_gpio_command_model.h
    SOURCES u575_gpio_command_test.cpp u575_model.cpp
        "${u5_leaf}/drivers/gpio.c" "${u5_leaf}/system.c"
    INCLUDES "${u5_leaf}/private")

foreach(pair f1:F103xB f4:F407xx g4:G474xx h7:H743xx u5:U575xx)
    string(REPLACE ":" ";" fields "${pair}")
    list(GET fields 0 family)
    list(GET fields 1 part)
    if(family MATCHES "^f[14]$")
        set(engine i2c_v1_async)
        set(definitions NX_I2C_LEAF_V1)
    else()
        set(engine i2c_v2)
        set(definitions "")
    endif()
    string(TOLOWER "${part}" soc)
    string(REGEX REPLACE "(xb|xx)$" "" soc "${soc}")
    set(soc "stm32${soc}")
    set(name "nexus_${family}_i2c_official_leaf_test")
    # F4 retains its reviewed dedicated helper; the other four use profiles.
    if(family STREQUAL "f4")
        nexus_google_test(${name} STM32F4_LL
            SOURCES i2c_official_leaf_test.cpp i2c_official_leaf_probe.c
                "${NEXUS_SOURCE_DIR}/drivers/i2c/${engine}/${engine}.c"
            LIBRARIES Nexus::IO Nexus::Arch
            INCLUDES "${NEXUS_SOURCE_DIR}/tests/google"
                "${NEXUS_SOURCE_DIR}/drivers/i2c/${engine}")
        target_include_directories(${name} SYSTEM PRIVATE
            "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
            "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
        nexus_use_cmsis_core(${name})
        target_compile_definitions(${name} PRIVATE STM32${part} ${definitions})
        target_compile_options(${name} PRIVATE
            "-include${CMAKE_CURRENT_SOURCE_DIR}/i2c_official_leaf_model.h")
    else()
        nexus_official_leaf_test(${name} ${family} STM32${part}
            i2c_official_leaf_model.h
            SOURCES i2c_official_leaf_test.cpp i2c_official_leaf_probe.c
                "${NEXUS_SOURCE_DIR}/drivers/i2c/${engine}/${engine}.c"
            INCLUDES "${NEXUS_SOURCE_DIR}/drivers/i2c/${engine}"
            DEFINITIONS ${definitions})
    endif()
    target_include_directories(${name} PRIVATE
        "${NEXUS_SOURCE_DIR}/soc/stm32/${soc}/private")
    if(engine STREQUAL "i2c_v1_async")
        target_compile_definitions(${name} PRIVATE
            "NX_I2C_V1_PORT_HEADER=\"${soc}_i2c_port.h\"")
    else()
        target_compile_definitions(${name} PRIVATE
            "NX_I2C_V2_PORT_HEADER=\"${soc}_i2c_port.h\"")
    endif()
endforeach()

# Scope wrapper enums and host exclusive declarations to UART C, avoiding
# unrelated translation units whose private names precede vendor selection.
set_property(SOURCE "${u5_leaf}/drivers/uart.c"
    "${NEXUS_SOURCE_DIR}/tests/contracts/u575_uart_leaf_port.c"
    APPEND PROPERTY COMPILE_OPTIONS
    "-include${CMAKE_CURRENT_SOURCE_DIR}/u575_uart_cpu_model.h")

# Preserve every existing test consumer when its actual provider starts using
# a private official operation. This selects no different mode or test source.
get_property(leaf_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(target IN LISTS leaf_targets)
    get_target_property(sources ${target} SOURCES)
    if("${u5_leaf}/drivers/gpio.c" IN_LIST sources)
        nexus_stm32u575_gpio_hal(${target})
        nexus_use_cmsis_host_model(${target}_stm32u575_gpio_hal)
    elseif("${u5_leaf}/drivers/uart.c" IN_LIST sources)
        nexus_stm32_official(${target} u5)
    endif()
    get_target_property(definitions ${target} COMPILE_DEFINITIONS)
    foreach(binding f1:F103xB:stm32f103 f4:F407xx:stm32f407
            g4:G474xx:stm32g474 h7:H743xx:stm32h743 u5:U575xx:stm32u575)
        string(REPLACE ":" ";" fields "${binding}")
        list(GET fields 0 family)
        list(GET fields 1 part)
        list(GET fields 2 soc)
        if(NOT "STM32${part}" IN_LIST definitions)
            continue()
        endif()
        set(v1 "${NEXUS_SOURCE_DIR}/drivers/i2c/i2c_v1_async/i2c_v1_async.c")
        set(v2 "${NEXUS_SOURCE_DIR}/drivers/i2c/i2c_v2/i2c_v2.c")
        if(v1 IN_LIST sources OR v2 IN_LIST sources)
            target_include_directories(${target} PRIVATE
                "${NEXUS_SOURCE_DIR}/soc/stm32/${soc}/private")
            if(v1 IN_LIST sources)
                target_compile_definitions(${target} PRIVATE
                    "NX_I2C_V1_PORT_HEADER=\"${soc}_i2c_port.h\"")
            else()
                target_compile_definitions(${target} PRIVATE
                    "NX_I2C_V2_PORT_HEADER=\"${soc}_i2c_port.h\"")
            endif()
            if(family STREQUAL "f4")
                nexus_stm32f4_official(${target})
            else()
                nexus_stm32_official(${target} ${family})
            endif()
        endif()
    endforeach()
    if("${h7_leaf}/drivers/spi.c" IN_LIST sources OR
            "${h7_leaf}/drivers/spi_dma.c" IN_LIST sources)
        nexus_stm32_official(${target} h7)
    endif()
endforeach()
