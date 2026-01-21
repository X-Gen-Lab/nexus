# STMicroelectronics Libraries

This directory contains official STMicroelectronics HAL drivers and CMSIS device files for STM32 microcontroller families.

## Supported STM32 Series

The following STM32 series are supported in the Nexus framework:

### Mainstream Series

| Series | CMSIS Device | HAL Driver | Description |
|--------|--------------|------------|-------------|
| STM32C0 | cmsis_device_c0 | stm32c0xx_hal_driver | Entry-level ARM Cortex-M0+ |
| STM32F0 | cmsis_device_f0 | stm32f0xx_hal_driver | Mainstream ARM Cortex-M0 |
| STM32F1 | cmsis_device_f1 | stm32f1xx_hal_driver | Mainstream ARM Cortex-M3 |
| STM32F2 | cmsis_device_f2 | stm32f2xx_hal_driver | High-performance ARM Cortex-M3 |
| STM32F3 | cmsis_device_f3 | stm32f3xx_hal_driver | Mixed-signal ARM Cortex-M4 |
| STM32F4 | cmsis_device_f4 | stm32f4xx_hal_driver | High-performance ARM Cortex-M4 |
| STM32F7 | cmsis_device_f7 | stm32f7xx_hal_driver | Very high-performance ARM Cortex-M7 |

### High-Performance Series

| Series | CMSIS Device | HAL Driver | Description |
|--------|--------------|------------|-------------|
| STM32H5 | cmsis_device_h5 | stm32h5xx_hal_driver | High-performance ARM Cortex-M33 |
| STM32H7 | cmsis_device_h7 | stm32h7xx_hal_driver | Very high-performance ARM Cortex-M7 |

### Low-Power Series

| Series | CMSIS Device | HAL Driver | Description |
|--------|--------------|------------|-------------|
| STM32L0 | cmsis_device_l0 | stm32l0xx_hal_driver | Ultra-low-power ARM Cortex-M0+ |
| STM32L1 | cmsis_device_l1 | stm32l1xx_hal_driver | Ultra-low-power ARM Cortex-M3 |
| STM32L4 | cmsis_device_l4 | stm32l4xx_hal_driver | Ultra-low-power ARM Cortex-M4 |
| STM32L5 | cmsis_device_l5 | stm32l5xx_hal_driver | Ultra-low-power ARM Cortex-M33 |

### Mixed-Signal Series

| Series | CMSIS Device | HAL Driver | Description |
|--------|--------------|------------|-------------|
| STM32G0 | cmsis_device_g0 | stm32g0xx_hal_driver | Mainstream ARM Cortex-M0+ |
| STM32G4 | cmsis_device_g4 | stm32g4xx_hal_driver | Mainstream ARM Cortex-M4 |

### Ultra-Low-Power Series

| Series | CMSIS Device | HAL Driver | Description |
|--------|--------------|------------|-------------|
| STM32U0 | cmsis_device_u0 | stm32u0xx_hal_driver | Ultra-low-power ARM Cortex-M0+ |
| STM32U3 | cmsis_device_u3 | stm32u3xx_hal_driver | Ultra-low-power ARM Cortex-M33 |
| STM32U5 | cmsis_device_u5 | stm32u5xx_hal_driver | Ultra-low-power ARM Cortex-M33 |

## Directory Structure

```
vendors/st/
├── cmsis_device_c0/         # CMSIS Device files for STM32C0
├── cmsis_device_f0/         # CMSIS Device files for STM32F0
├── cmsis_device_f1/         # CMSIS Device files for STM32F1
├── cmsis_device_f2/         # CMSIS Device files for STM32F2
├── cmsis_device_f3/         # CMSIS Device files for STM32F3
├── cmsis_device_f4/         # CMSIS Device files for STM32F4
├── cmsis_device_f7/         # CMSIS Device files for STM32F7
├── cmsis_device_g0/         # CMSIS Device files for STM32G0
├── cmsis_device_g4/         # CMSIS Device files for STM32G4
├── cmsis_device_h5/         # CMSIS Device files for STM32H5
├── cmsis_device_h7/         # CMSIS Device files for STM32H7
├── cmsis_device_l0/         # CMSIS Device files for STM32L0
├── cmsis_device_l1/         # CMSIS Device files for STM32L1
├── cmsis_device_l4/         # CMSIS Device files for STM32L4
├── cmsis_device_l5/         # CMSIS Device files for STM32L5
├── cmsis_device_u0/         # CMSIS Device files for STM32U0
├── cmsis_device_u3/         # CMSIS Device files for STM32U3
├── cmsis_device_u5/         # CMSIS Device files for STM32U5
├── stm32c0xx_hal_driver/    # HAL driver for STM32C0
├── stm32f0xx_hal_driver/    # HAL driver for STM32F0
├── stm32f1xx_hal_driver/    # HAL driver for STM32F1
├── stm32f2xx_hal_driver/    # HAL driver for STM32F2
├── stm32f3xx_hal_driver/    # HAL driver for STM32F3
├── stm32f4xx_hal_driver/    # HAL driver for STM32F4
├── stm32f7xx_hal_driver/    # HAL driver for STM32F7
├── stm32g0xx_hal_driver/    # HAL driver for STM32G0
├── stm32g4xx_hal_driver/    # HAL driver for STM32G4
├── stm32h5xx_hal_driver/    # HAL driver for STM32H5
├── stm32h7xx_hal_driver/    # HAL driver for STM32H7
├── stm32l0xx_hal_driver/    # HAL driver for STM32L0
├── stm32l1xx_hal_driver/    # HAL driver for STM32L1
├── stm32l4xx_hal_driver/    # HAL driver for STM32L4
├── stm32l5xx_hal_driver/    # HAL driver for STM32L5
├── stm32u0xx_hal_driver/    # HAL driver for STM32U0
├── stm32u3xx_hal_driver/    # HAL driver for STM32U3
├── stm32u5xx_hal_driver/    # HAL driver for STM32U5
└── README.md                # This file
```

## Submodule Initialization

All STM32 libraries are managed as Git submodules. To initialize them:

### Initialize All Submodules

```bash
# Initialize all submodules recursively
git submodule update --init --recursive

# Or with parallel jobs for faster initialization
git submodule update --init --recursive --jobs 4
```

### Initialize Specific Series

```bash
# Initialize only STM32F4 libraries
git submodule update --init vendors/st/cmsis_device_f4
git submodule update --init vendors/st/stm32f4xx_hal_driver

# Initialize only STM32H7 libraries
git submodule update --init vendors/st/cmsis_device_h7
git submodule update --init vendors/st/stm32h7xx_hal_driver
```

### Shallow Clone (Recommended)

For faster initialization and reduced disk usage, use shallow clones:

```bash
# Shallow clone with depth 1
git submodule update --init --depth 1 vendors/st/stm32f4xx_hal_driver
```

### Update Submodules

To update all submodules to their latest versions:

```bash
# Update all submodules
git submodule update --remote --merge

# Update specific submodule
git submodule update --remote --merge vendors/st/stm32f4xx_hal_driver
```

## Usage in Nexus

These libraries are integrated into the Nexus build system through the platform-specific configurations in `platforms/stm32/`. The HAL drivers and CMSIS device files are automatically included when building for a specific STM32 target.

### Example: Building for STM32F4

```bash
# Configure for STM32F4
cmake -B build -DPLATFORM=stm32 -DSTM32_SERIES=F4

# Build
cmake --build build
```

## License

All STMicroelectronics libraries are licensed under BSD-3-Clause license. See individual submodule directories for detailed license information.

## Official Sources

All submodules reference official STMicroelectronics repositories:

- **HAL Drivers**: https://github.com/STMicroelectronics/stm32{series}xx_hal_driver
- **CMSIS Devices**: https://github.com/STMicroelectronics/cmsis_device_{series}

## Notes

- These libraries are excluded from Nexus code formatting and static analysis
- Do not modify vendor code directly; use wrapper layers in `platforms/stm32/`
- Each series requires both CMSIS device files and HAL driver for full functionality
- Refer to STMicroelectronics documentation for series-specific features and peripherals

## Support

For issues related to:
- **Nexus integration**: Open an issue in the Nexus repository
- **STM32 HAL bugs**: Report to STMicroelectronics official repositories
- **Hardware questions**: Consult STM32 datasheets and reference manuals

## References

- [STM32 Official Website](https://www.st.com/en/microcontrollers-microprocessors/stm32-32-bit-arm-cortex-mcus.html)
- [STM32 HAL Documentation](https://www.st.com/en/embedded-software/stm32cube-mcu-mpu-packages.html)
- [CMSIS Documentation](https://arm-software.github.io/CMSIS_5/)
