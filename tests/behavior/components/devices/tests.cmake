# Explicit devices behavior and private model dependencies.
# Author: Nexus Team
include("${nexus_google_root}/components/devices/lsm6dsox.cmake")

include("${nexus_google_root}/components/devices/wm8978.cmake")

include("${nexus_google_root}/components/devices/ov2640.cmake")

include("${nexus_google_root}/components/devices/esp_at.cmake")

include("${nexus_google_root}/components/devices/common_atecc608b.cmake")

include("${nexus_google_root}/components/devices/robotics_devices.cmake")

include("${nexus_google_root}/components/devices/ili9341.cmake")

include("${nexus_google_root}/components/devices/at24c02.cmake")

include("${nexus_google_root}/components/devices/w25.cmake")

include("${nexus_google_root}/components/devices/infrared.cmake")

include("${nexus_google_root}/components/devices/infrared_exti.cmake")

include("${nexus_google_root}/components/devices/ds18b20.cmake")

nexus_google_test(nexus_bmp280_timing_test SOURCES "${nexus_google_root}/components/devices/bmp280_timing_test.cpp"
    "${nexus_google_root}/components/devices/bmp280_lifecycle_test.cpp"
    LIBRARIES Nexus::BMP280
)

nexus_google_test(nexus_tmp117_test SOURCES "${nexus_google_root}/components/devices/tmp117_test.cpp"
    LIBRARIES Nexus::TMP117I2C Nexus::BusOwner Nexus::Arch Nexus::IO
)
