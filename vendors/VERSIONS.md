# Vendor Submodule Versions

This file documents the versions of external vendor libraries used in the Nexus project.

## ARM CMSIS

| Submodule | Repository | Version | Commit Hash |
|-----------|------------|---------|-------------|
| CMSIS_5 | [ARM-software/CMSIS_5](https://github.com/ARM-software/CMSIS_5) | 5.9.0 | 55b19837f5703e418ca37894d5745b1dc05e4c91 |

## STMicroelectronics

| Submodule | Repository | Version | Commit Hash |
|-----------|------------|---------|-------------|
| cmsis_device_f4 | [STMicroelectronics/cmsis_device_f4](https://github.com/STMicroelectronics/cmsis_device_f4) | v2.6.11 | 3c77349ce04c8af401454cc51f85ea9a50e34fc1 |
| stm32f4xx_hal_driver | [STMicroelectronics/stm32f4xx_hal_driver](https://github.com/STMicroelectronics/stm32f4xx_hal_driver) | v1.8.5 | b5c4d27f41c0ffbde135c0560d882b8a8eecb88e |

## Update Instructions

To update a submodule to a new version:

```bash
# Navigate to the submodule directory
git -C vendors/<path/to/submodule> fetch --tags

# List available tags
git -C vendors/<path/to/submodule> tag --sort=-v:refname | head -10

# Checkout the desired version
git -C vendors/<path/to/submodule> checkout <tag>

# Update this file with the new version and commit hash
```

## Initialization

After cloning the repository, initialize all submodules:

```bash
git submodule update --init --recursive
```

## Last Updated

- Date: 2026-01-15
- Updated by: Kiro
