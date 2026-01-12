# Vendor Libraries

This directory contains official chip vendor libraries and SDKs.

## Directory Structure

```
vendors/
├── st/                     # STMicroelectronics
│   ├── cmsis_device_f4/    # CMSIS Device for STM32F4
│   ├── cmsis_device_h7/    # CMSIS Device for STM32H7
│   ├── stm32f4xx_hal/      # STM32F4 HAL Driver
│   └── stm32h7xx_hal/      # STM32H7 HAL Driver
├── nordic/                 # Nordic Semiconductor
│   └── nrfx/               # nRF SDK drivers
├── espressif/              # Espressif Systems
│   └── esp-idf/            # ESP-IDF components
└── README.md
```

## Adding Vendor Libraries

### Option 1: Git Submodule (Recommended)

```bash
# STM32F4 HAL
git submodule add https://github.com/STMicroelectronics/stm32f4xx_hal_driver.git vendors/st/stm32f4xx_hal

# CMSIS Device F4
git submodule add https://github.com/STMicroelectronics/cmsis_device_f4.git vendors/st/cmsis_device_f4

# CMSIS Core
git submodule add https://github.com/ARM-software/CMSIS_5.git ext/cmsis
```

### Option 2: Manual Download

Download from official sources and extract to appropriate directory.

## License

Each vendor library retains its original license. See individual directories for license information.

| Vendor | Library | License |
|--------|---------|---------|
| ST | STM32 HAL | BSD-3-Clause |
| ST | CMSIS Device | Apache-2.0 |
| ARM | CMSIS Core | Apache-2.0 |
| Nordic | nrfx | BSD-3-Clause |
| Espressif | ESP-IDF | Apache-2.0 |

## Notes

- These libraries are excluded from code formatting checks
- These libraries are excluded from static analysis
- Do not modify vendor code directly; use wrapper layers in `platforms/`
