# Compile real provider sources against deterministic register/event models.
set(stm32_root "${NEXUS_SOURCE_DIR}/soc/stm32f407")
set(stm32_device "${NEXUS_SOURCE_DIR}/vendors/st/cmsis_device_f4/Include")
set(stm32_cmsis "${NEXUS_SOURCE_DIR}/vendors/arm/CMSIS_5/CMSIS/Core/Include")
add_executable(contract_stm32_system_model
    "${stm32_root}/system.c" "../stm32_system_test.c")
add_executable(contract_stm32_io_model
    "${stm32_root}/drivers/gpio.c" "${stm32_root}/drivers/uart.c"
    "${stm32_root}/drivers/spi.c" "${stm32_root}/drivers/i2c.c"
    "${stm32_root}/drivers/flash.c" "${stm32_root}/drivers/watchdog.c"
    "${stm32_root}/drivers/exti.c" "${stm32_root}/drivers/timer.c"
    "${stm32_root}/drivers/adc.c" "../stm32_io_test.c")
execute_process(COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/generate_constructors.py"
    "${NEXUS_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/stm32_constructors.h"
    COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${NEXUS_SOURCE_DIR}/tools/configure/stm32_bindings.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/generate_constructors.py")
add_executable(contract_stm32_constructor_model
    "${stm32_root}/drivers/gpio.c" "${stm32_root}/drivers/exti.c"
    "${stm32_root}/drivers/timer.c" "${stm32_root}/drivers/spi.c"
    "../stm32_constructor_test.c")
target_include_directories(contract_stm32_constructor_model PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}")
foreach(target contract_stm32_system_model contract_stm32_io_model
               contract_stm32_constructor_model)
    target_include_directories(${target} PRIVATE
        "${stm32_root}/private" "${NEXUS_SOURCE_DIR}/arch/include")
    target_include_directories(${target} SYSTEM PRIVATE
        "${stm32_device}" "${stm32_cmsis}")
    target_compile_definitions(${target} PRIVATE NEXUS_STM32_MODEL STM32F407xx)
    target_link_libraries(${target} PRIVATE nexus_io nexus_build_options
        nexus_owned_warnings)
    target_compile_options(${target} PRIVATE -UNDEBUG)
    add_test(NAME ${target} COMMAND ${target})
endforeach()
target_compile_options(contract_stm32_system_model PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/model_poll.h")
target_compile_options(contract_stm32_io_model PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/io_poll.h")
target_compile_options(contract_stm32_constructor_model PRIVATE
    "-include${CMAKE_CURRENT_SOURCE_DIR}/io_poll.h")
