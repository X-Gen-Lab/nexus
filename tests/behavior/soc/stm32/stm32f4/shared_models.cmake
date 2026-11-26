# Explicit late behavior targets and private model requirements.
# Author: Nexus Team
# One external BSRR/ODR model, shared by every maintained STM32 SPI context.
foreach(
    spi_latch_target
    IN
    ITEMS
        contract_stm32_constructor_model
        contract_stm32_io_model
        nexus_stm32_multi_controller_factory_test
        nexus_stm32_single_spi_factory_test
        nexus_stm32_spi_dma_owner_test
        nexus_stm32_spi_dma_sequence_test
        nexus_stm32_spi_dma_test
        nexus_stm32_spi_sequence_factory_test
        nexus_stm32_spi_transaction_test
        nexus_stm32_static_lifecycle_test
        nexus_stm32f4_spi_multi_controller_test
)
    target_sources(
        ${spi_latch_target}
        PRIVATE
            "${nexus_google_root}/soc/stm32/stm32f4/spi/stm32_spi_cs_model.c"
    )
    target_compile_options(
        ${spi_latch_target}
        PRIVATE
            "-include${nexus_google_root}/soc/stm32/stm32f4/spi/stm32_spi_cs_model.h"
    )
endforeach()
