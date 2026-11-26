# Explicit shared behavior and private model dependencies.
# Author: Nexus Team
target_compile_options(
    nexus_stm32_spi_dma_test
    PRIVATE
        "-include${nexus_google_root}/soc/stm32/stm32f4/shared/stm32_dma_model.h"
)
