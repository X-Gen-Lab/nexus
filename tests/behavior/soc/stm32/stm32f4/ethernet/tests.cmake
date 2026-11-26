# Explicit ethernet behavior and private model dependencies.
# Author: Nexus Team
include(
    "${nexus_google_root}/soc/stm32/stm32f4/ethernet/ethernet_factory.cmake"
)

include(
    "${nexus_google_root}/soc/stm32/stm32f4/ethernet/ethernet_raw_errata.cmake"
)

include("${nexus_google_root}/soc/stm32/stm32f4/ethernet/ethernet_frames.cmake")

include(
    "${nexus_google_root}/soc/stm32/stm32f4/ethernet/ethernet_frames_factory.cmake"
)
