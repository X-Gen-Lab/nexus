# Explicit timer behavior and private model dependencies.
# Author: Nexus Team
include("${nexus_google_root}/soc/stm32/stm32f4/timer/timer_advanced.cmake")

include("${nexus_google_root}/soc/stm32/stm32f4/timer/timer_clock.cmake")

include("${nexus_google_root}/soc/stm32/stm32f4/timer/timer_factory.cmake")

include(
    "${nexus_google_root}/soc/stm32/stm32f4/timer/f407_timer_waveform.cmake"
)
