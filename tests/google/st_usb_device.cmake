# Actual locked Device Core, with mocks only at the Nexus DCD boundary.
nexus_add_st_usb_device()
nexus_google_test(nexus_st_usb_device_stack_test
    SOURCES st_usb_device_stack_test.cpp st_usb_device_dcd_test.cpp
    LIBRARIES Nexus::STUSBDevice Nexus::NativeModel Nexus::Arch
    INCLUDES "${NEXUS_SOURCE_DIR}/components/st_usb_device/private"
        "${NEXUS_SOURCE_DIR}/tests/support/native/drivers")
target_include_directories(nexus_st_usb_device_stack_test SYSTEM PRIVATE
    "${NEXUS_SOURCE_DIR}/vendors/st/usb_device/Core/Inc")
