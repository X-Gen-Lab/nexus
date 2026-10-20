# Optional pure descriptor validation and caller-owned EP0 orchestration.
add_library(nexus_usb_descriptors STATIC src/usb_descriptors.c)
target_include_directories(nexus_usb_descriptors PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_usb_descriptors PUBLIC Nexus::IO
    PRIVATE nexus_build_options nexus_owned_warnings)
add_library(Nexus::USBDescriptors ALIAS nexus_usb_descriptors)
add_library(nexus_usb_control STATIC src/usb_control.c)
target_include_directories(nexus_usb_control PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(nexus_usb_control PUBLIC Nexus::IO
    PRIVATE Nexus::Arch nexus_build_options nexus_owned_warnings)
add_library(Nexus::USBControl ALIAS nexus_usb_control)
