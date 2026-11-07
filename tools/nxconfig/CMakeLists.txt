cmake_minimum_required(VERSION 3.31)
project(nexus_nxconfig VERSION 0.1.0 LANGUAGES C)

get_filename_component(NXCONFIG_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.."
    ABSOLUTE)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/NxconfigCore.cmake")
add_executable(nxconfig src/main.c)
target_link_libraries(nxconfig PRIVATE nxconfig_core)
target_compile_options(nxconfig PRIVATE -Wall -Wextra -Werror -Wpedantic)

option(NXCONFIG_BUILD_TESTS "Build standalone Host C11 configuration contracts" OFF)
if(NXCONFIG_BUILD_TESTS)
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    add_subdirectory("${NXCONFIG_SOURCE_ROOT}/ext/googletest"
        "${CMAKE_CURRENT_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)
    enable_testing()
    add_executable(nxconfig_test tests/config_test.cpp)
    target_link_libraries(nxconfig_test PRIVATE nxconfig_core GTest::gmock_main)
    nxconfig_contract_settings(nxconfig_test)
    target_compile_options(nxconfig_test PRIVATE
        -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow)
    add_test(NAME nxconfig_contracts COMMAND nxconfig_test)
endif()
