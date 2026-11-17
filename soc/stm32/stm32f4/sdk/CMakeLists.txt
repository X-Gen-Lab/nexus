# Private SDK transports stay opt-in in each owning driver module.
foreach(
    module
    Official
    USB
    SAI
    SDIO
    QSPI
    FSMC
    SDRAM
    Capture
    Crypto
    Graphics
)
    include("${CMAKE_CURRENT_LIST_DIR}/STM32${module}.cmake")
endforeach()
