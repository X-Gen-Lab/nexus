# Explicit watchdog behavior and private model dependencies.
# Author: Nexus Team
include("${nexus_google_root}/soc/stm32/stm32f4/watchdog/legacy_watchdog.cmake")

include(
    "${nexus_google_root}/soc/stm32/stm32f4/watchdog/stm32_window_watchdog.cmake"
)
