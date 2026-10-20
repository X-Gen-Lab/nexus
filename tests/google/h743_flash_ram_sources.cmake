# Shared explicit provider sources for complete H743 RAM and legacy contracts.
# Append a production source only after it exists and its intended RED ran.
set(h743_flash_ram_production_sources
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/flash_ram.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/flash_ram_owner.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/flash_ram_transaction.c"
    "${NEXUS_SOURCE_DIR}/soc/stm32h743/drivers/flash_mpu.c")
