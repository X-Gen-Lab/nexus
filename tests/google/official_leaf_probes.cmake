# Actual F4 LL data access executes with the selected production default.
set(f4_i2c_engine
    "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/drivers/ip/i2c_v1_async")
nexus_google_test(nexus_f4_i2c_official_leaf_test STM32F4_LL
    SOURCES i2c_official_leaf_test.cpp i2c_official_leaf_probe.c
        "${f4_i2c_engine}/i2c_v1_async.c"
    LIBRARIES Nexus::IO Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/tests/google" "${f4_i2c_engine}"
        "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
target_include_directories(nexus_f4_i2c_official_leaf_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include"
    "${NEXUS_CMSIS_CORE_INCLUDE_DIR}")
nexus_use_cmsis_core(nexus_f4_i2c_official_leaf_test)
target_compile_definitions(nexus_f4_i2c_official_leaf_test PRIVATE
    STM32F407xx NX_I2C_LEAF_V1
    "NX_I2C_V1_PORT_HEADER=\"stm32f407_i2c_port.h\"")
target_compile_options(nexus_f4_i2c_official_leaf_test PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/i2c_official_leaf_model.h")

# Existing F4 targets selecting the same engine use its reviewed LL port.
get_property(f4_leaf_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(target IN LISTS f4_leaf_targets)
    get_target_property(sources ${target} SOURCES)
    get_target_property(definitions ${target} COMPILE_DEFINITIONS)
    if("${f4_i2c_engine}/i2c_v1_async.c" IN_LIST sources AND
            "STM32F407xx" IN_LIST definitions)
        target_include_directories(${target} PRIVATE
            "${NEXUS_SOURCE_DIR}/soc/stm32/stm32f4/private")
        target_compile_definitions(${target} PRIVATE
            "NX_I2C_V1_PORT_HEADER=\"stm32f407_i2c_port.h\"")
        nexus_stm32f4_official(${target})
    endif()
endforeach()
