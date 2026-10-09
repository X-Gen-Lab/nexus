# STM32 Chip Variant Configuration Files

This directory contains Kconfig files for all STM32 series chip variants.

## Architecture Overview

The STM32 Kconfig system uses a hierarchical architecture:

```
platforms/stm32/Kconfig (Top-level)
    ├── Kconfig_chip (Series selection)
    │   └── chips/Kconfig.stm32xx (Variant selection for each series)
    ├── Kconfig_clock (Clock configuration)
    ├── Kconfig_boot (Boot configuration)
    ├── Kconfig_peripherals (Peripheral enable/disable)
    ├── Kconfig_hal_modules (HAL module auto-selection)
    ├── Kconfig_hal_callbacks (HAL callback configuration)
    ├── Kconfig_peripheral_config (Peripheral detailed config)
    └── Kconfig_system (System configuration)
```

## Supported STM32 Series

### Mainstream Series
- **STM32F0**: Entry-level Cortex-M0 (Kconfig.stm32f0)
- **STM32F1**: Classic Cortex-M3 (Kconfig.stm32f1)
- **STM32F2**: High-performance Cortex-M3 (Kconfig.stm32f2)
- **STM32F3**: Mixed-signal Cortex-M4 (Kconfig.stm32f3)
- **STM32F4**: High-performance Cortex-M4 (Kconfig.stm32f4) ✅ Complete
- **STM32F7**: Very high-performance Cortex-M7 (Kconfig.stm32f7)

### High Performance Series
- **STM32H5**: Secure Cortex-M33 (Kconfig.stm32h5)
- **STM32H7**: Flagship Cortex-M7 (Kconfig.stm32h7) ✅ Complete

### Ultra-Low-Power Series
- **STM32L0**: Ultra-low-power Cortex-M0+ (Kconfig.stm32l0)
- **STM32L1**: Low-power Cortex-M3 (Kconfig.stm32l1)
- **STM32L4**: Ultra-low-power Cortex-M4 (Kconfig.stm32l4) ✅ Complete
- **STM32L5**: Secure ultra-low-power Cortex-M33 (Kconfig.stm32l5)

### Ultra-Low-Power Plus Series
- **STM32U0**: Next-gen ultra-low-power Cortex-M0+ (Kconfig.stm32u0)
- **STM32U3**: Advanced ultra-low-power Cortex-M33 (Kconfig.stm32u3)
- **STM32U5**: Flagship ultra-low-power Cortex-M33 (Kconfig.stm32u5)

### Wireless Series
- **STM32WB**: Dual-core wireless BLE/802.15.4 (Kconfig.stm32wb)
- **STM32WL**: LoRa wireless Cortex-M4 (Kconfig.stm32wl)

### General Purpose Series
- **STM32G0**: Next-gen mainstream Cortex-M0+ (Kconfig.stm32g0) ✅ Placeholder
- **STM32G4**: High-performance mixed-signal Cortex-M4 (Kconfig.stm32g4)

### Value Line Series
- **STM32C0**: Ultra-low-cost Cortex-M0+ (Kconfig.stm32c0)

## File Structure

Each series-specific Kconfig file follows this structure:

```kconfig
#-----------------------------------------------------------------------------
# STM32XX Series Chip Variant Selection
#-----------------------------------------------------------------------------

if STM32XX

choice
    prompt "STM32XX Chip Variant"
    default STM32XXxx
    help
      Select specific chip variant...

config STM32XXxxx
    bool "Variant name (Flash, SRAM)"
    help
      Detailed variant description...

# More variants...

endchoice

#-----------------------------------------------------------------------------
# Chip Name Definition
#-----------------------------------------------------------------------------

config STM32_CHIP_NAME
    string
    default "STM32XXxxx" if STM32XXxxx
    # More mappings...

#-----------------------------------------------------------------------------
# Memory Configuration
#-----------------------------------------------------------------------------

config STM32_FLASH_SIZE
    hex "Flash size (bytes)"
    default 0xXXXXX if STM32XXxxx
    # More mappings...

config STM32_SRAM_SIZE
    hex "SRAM size (bytes)"
    default 0xXXXXX if STM32XXxxx
    # More mappings...

#-----------------------------------------------------------------------------
# Peripheral Feature Flags
#-----------------------------------------------------------------------------

config STM32XX_HAS_FEATURE
    bool
    default y if STM32XXxxx
    help
      Whether the chip has specific feature...

endif # STM32XX
```

## Design Principles

### 1. Extensibility
- Easy to add new series by creating a new `Kconfig.stm32xx` file
- Easy to add new variants within a series
- Modular structure allows independent development

### 2. Clarity
- Clear hierarchy: Series → Variant → Configuration
- Descriptive help text for each option
- Consistent naming conventions

### 3. Maintainability
- One file per series for easy maintenance
- Feature flags for conditional compilation
- Memory configuration centralized

### 4. Scalability
- Supports all current and future STM32 series
- Handles series-specific features gracefully
- Allows for series-specific optimizations

## Adding a New Series

To add support for a new STM32 series:

1. Create `Kconfig.stm32xx` in this directory
2. Add series choice in `../Kconfig_chip`
3. Add source statement in `../Kconfig_chip`
4. Define chip variants and configurations
5. Add series-specific feature flags

## Adding a New Variant

To add a new chip variant to an existing series:

1. Open the corresponding `Kconfig.stm32xx` file
2. Add new config option in the choice block
3. Update `STM32_CHIP_NAME` mapping
4. Update memory configuration defaults
5. Update feature flags if needed

## Configuration Variables

### Common Variables (All Series)
- `STM32_CHIP_NAME`: Chip name for HAL driver (e.g., "STM32F407xx")
- `STM32_FLASH_SIZE`: Flash memory size in bytes (hex)
- `STM32_SRAM_SIZE`: SRAM memory size in bytes (hex)
- `STM32_SERIES`: Series identifier (e.g., "stm32f4")

### Series-Specific Feature Flags
- `STM32XX_HAS_ETHERNET`: Ethernet MAC peripheral
- `STM32XX_HAS_LTDC`: LCD-TFT display controller
- `STM32XX_HAS_DSI`: MIPI-DSI interface
- `STM32XX_HAS_CRYPTO`: Hardware crypto accelerator
- `STM32XX_HAS_QSPI`: Quad SPI interface
- `STM32XX_HAS_OCTOSPI`: Octo SPI interface
- `STM32XX_HAS_SAI`: SAI audio interface
- `STM32XX_HAS_DFSDM`: Digital filter for sigma-delta modulators
- `STM32XX_HAS_JPEG`: JPEG codec
- `STM32XX_HAS_SMPS`: Switched-mode power supply

### Global Feature Flags (Cross-Series)
- `STM32_HAS_DCACHE`: Data cache
- `STM32_HAS_ICACHE`: Instruction cache
- `STM32_HAS_MPU`: Memory protection unit
- `STM32_HAS_FPU`: Floating point unit
- `STM32_HAS_TRUSTZONE`: ARM TrustZone security

## Status

| Series | Status | Variants | Notes |
|--------|--------|----------|-------|
| STM32C0 | Placeholder | 0 | TODO: Add variants |
| STM32F0 | Placeholder | 0 | TODO: Add variants |
| STM32F1 | Placeholder | 0 | TODO: Add variants |
| STM32F2 | Placeholder | 0 | TODO: Add variants |
| STM32F3 | Placeholder | 0 | TODO: Add variants |
| STM32F4 | ✅ Complete | 23 | All major variants |
| STM32F7 | Placeholder | 0 | TODO: Add variants |
| STM32G0 | Placeholder | 0 | TODO: Add variants |
| STM32G4 | Placeholder | 0 | TODO: Add variants |
| STM32H5 | Placeholder | 0 | TODO: Add variants |
| STM32H7 | ✅ Complete | 9 | All major variants |
| STM32L0 | Placeholder | 0 | TODO: Add variants |
| STM32L1 | Placeholder | 0 | TODO: Add variants |
| STM32L4 | ✅ Complete | 5 | Major variants + L4+ |
| STM32L5 | Placeholder | 0 | TODO: Add variants |
| STM32U0 | Placeholder | 0 | TODO: Add variants |
| STM32U3 | Placeholder | 0 | TODO: Add variants |
| STM32U5 | Placeholder | 0 | TODO: Add variants |
| STM32WB | Placeholder | 0 | TODO: Add variants |
| STM32WL | Placeholder | 0 | TODO: Add variants |

## References

- [STM32 Product Selector](https://www.st.com/en/microcontrollers-microprocessors/stm32-32-bit-arm-cortex-mcus.html)
- [STM32 HAL Driver Documentation](https://www.st.com/en/embedded-software/stm32cube-mcu-mpu-packages.html)
- [Kconfig Language Documentation](https://www.kernel.org/doc/html/latest/kbuild/kconfig-language.html)
