# CMakeLists.txt
# Optional wait helpers and explicit per-backend storage.
# Author: Nexus Team
add_library(nexus_os_wait STATIC wait.c)
target_include_directories(nexus_os_wait PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_os_wait PUBLIC nexus_core PRIVATE nexus_owned_warnings nexus_build_options)

add_library(nexus_os_baremetal STATIC baremetal/notify.c)
target_link_libraries(nexus_os_baremetal PUBLIC nexus_os_wait PRIVATE nexus_owned_warnings nexus_build_options)

if(NEXUS_SOC_FAMILY STREQUAL "native")
    find_package(Threads REQUIRED)
    add_library(nexus_os_native STATIC native/native.c)
    target_include_directories(nexus_os_native PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/native/include")
    target_link_libraries(nexus_os_native PUBLIC nexus_os_wait Threads::Threads PRIVATE nexus_owned_warnings nexus_build_options)
endif()

if(NEXUS_BACKEND STREQUAL "freertos" AND NOT NEXUS_SOC_FAMILY STREQUAL "native")
    add_subdirectory(freertos)
endif()

add_library(Nexus::OSWait ALIAS nexus_os_wait)
add_library(Nexus::OSBaremetal ALIAS nexus_os_baremetal)
if(TARGET nexus_os_native)
    add_library(Nexus::OSNative ALIAS nexus_os_native)
endif()
if(TARGET nexus_os_freertos)
    add_library(Nexus::OSFreeRTOS ALIAS nexus_os_freertos)
endif()
