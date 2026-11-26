# Explicit memory behavior and private model dependencies.
# Author: Nexus Team
include("${nexus_google_root}/soc/stm32/stm32f4/memory/f407_fsmc.cmake")

include("${nexus_google_root}/soc/stm32/stm32f4/memory/stm32_sdram.cmake")

include("${nexus_google_root}/soc/stm32/stm32f4/memory/stm32_qspi.cmake")

include("${nexus_google_root}/soc/stm32/stm32f4/memory/f407_sram_boot.cmake")

include("${nexus_google_root}/soc/stm32/stm32f4/memory/stm32_memory.cmake")
