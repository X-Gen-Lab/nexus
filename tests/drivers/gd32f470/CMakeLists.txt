cmake_minimum_required(VERSION 3.21)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(nexus_gd32_driver_contracts C)
    enable_testing()
endif()
get_filename_component(nexus_root "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
foreach(driver uart spi timebase typed_spi)
    set(target "test_driver_gd32_${driver}")
    add_executable(${target} "test_${driver}.c" fake/model.c)
    target_include_directories(${target} PRIVATE fake
        "${nexus_root}/hal/include" "${nexus_root}/arch/include"
        "${nexus_root}/boards/gd32f470_liangshan"
        "${nexus_root}/platforms/gd32f470/include")
    target_compile_features(${target} PRIVATE c_std_11)
    if(driver STREQUAL "typed_spi")
        target_sources(${target} PRIVATE "${nexus_root}/hal/src/nx_device.c"
            "${nexus_root}/hal/src/nx_device_spi.c")
        # Host discovery uses manual registration of the real GD descriptor.
        # Vendor, controller, platform boot and timer code remain production.
        target_compile_definitions(${target} PRIVATE NEXUS_PLATFORM_NATIVE=1)
    endif()
    if(TARGET nexus_build_options)
        target_link_libraries(${target} PRIVATE nexus_build_options)
    endif()
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    add_test(NAME "driver_gd32_${driver}" COMMAND ${target})
    set_tests_properties("driver_gd32_${driver}" PROPERTIES
        LABELS "driver;host-fake;gd32;contract" TIMEOUT 10)
endforeach()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_executable(test_driver_gd32_flash test_flash.c "${nexus_root}/soc/gd32f470zg/flash.c"
        "${nexus_root}/soc/gd32f470zg/identity.c")
    target_include_directories(test_driver_gd32_flash PRIVATE fake
        "${nexus_root}/soc/gd32f470zg" "${nexus_root}/arch/include"
        "${nexus_root}/services/storage/include" "${nexus_root}/hal/include")
    target_compile_features(test_driver_gd32_flash PRIVATE c_std_11)
    if(TARGET nexus_build_options)
        target_link_libraries(test_driver_gd32_flash PRIVATE nexus_build_options)
    endif()
    target_compile_options(test_driver_gd32_flash PRIVATE -Wall -Wextra -Werror -Wno-int-to-pointer-cast -UNDEBUG)
    target_link_options(test_driver_gd32_flash PRIVATE -no-pie)
    add_test(NAME driver_gd32_flash COMMAND test_driver_gd32_flash)
    set_tests_properties(driver_gd32_flash PROPERTIES LABELS "driver;host-fake;gd32;storage;contract" TIMEOUT 10)
endif()
