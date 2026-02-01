# Vendor Configuration System

This directory contains the unified vendor library configuration system for Nexus.

## Overview

The vendor configuration system provides a centralized way to manage all external dependencies (vendor libraries) with support for multiple sources:

1. **User-specified paths** (CMake cache variables)
2. **Environment variables**
3. **System packages** (via `find_package`)
4. **Git submodules** (default)
5. **Auto-download** (future feature)

## Files

- `VendorConfig.cmake` - Main configuration file with vendor registry
- `README.md` - This file

## Usage

### Basic Usage

```cmake
# In your CMakeLists.txt
include(VendorConfig)

# Resolve a vendor library
nexus_resolve_vendor(CMSIS_CORE
    RESULT_VAR CMSIS_PATH
    REQUIRED TRUE
)

# Use the resolved path
target_include_directories(my_target PRIVATE ${CMSIS_PATH}/Include)
```

### Initialize All Platform Vendors

```cmake
# Automatically initialize all vendors for current platform
nexus_init_platform_vendors()
```

### Query Vendor Information

```cmake
# List all vendors for a platform
nexus_list_platform_vendors(VENDOR_LIST PLATFORM stm32)
message(STATUS "STM32 vendors: ${VENDOR_LIST}")

# Check if vendor supports platform
nexus_vendor_supports_platform(CMSIS_CORE SUPPORTS PLATFORM stm32)
if(SUPPORTS)
    message(STATUS "CMSIS_CORE supports STM32")
endif()

# Print vendor registry
nexus_print_vendor_registry()
```

## Vendor Registry

The vendor registry is defined in `VendorConfig.cmake` and contains metadata for all known vendor libraries:

```cmake
nexus_register_vendor(
    NAME CMSIS_CORE
    DESCRIPTION "ARM CMSIS Core for Cortex-M"
    SUBMODULE_PATH "vendors/arm/CMSIS_5"
    INCLUDE_DIRS "CMSIS/Core/Include"
    PLATFORMS "stm32;gd32;nrf52"
    REQUIRED
)
```

### Registered Vendors

#### ARM Vendors
- `CMSIS_CORE` - ARM CMSIS Core for Cortex-M

#### STMicroelectronics Vendors
- `CMSIS_DEVICE_F4` - STM32F4 CMSIS Device Headers
- `HAL_DRIVER_F4` - STM32F4 HAL Driver
- (and other STM32 series: C0, F0, F1, F2, F3, F7, G0, G4, H5, H7, L0, L1, L4, L5, U0, U3, U5)

#### RTOS Vendors
- `FREERTOS` - FreeRTOS Real-Time Kernel

#### Test Framework Vendors
- `GTEST` - Google Test Framework

## Customization

### Override Vendor Paths

You can override vendor paths in multiple ways:

#### 1. CMake Cache Variable

```bash
cmake -DNEXUS_VENDOR_CMSIS_CORE_DIR=/path/to/cmsis ..
```

#### 2. Environment Variable

```bash
export NEXUS_CMSIS_CORE_DIR=/path/to/cmsis
cmake ..
```

#### 3. System Package

```cmake
# Enable find_package for a vendor
nexus_resolve_vendor_path(GTEST
    RESULT_VAR GTEST_PATH
    FIND_PACKAGE TRUE  # Try system package first
)
```

### Add New Vendor

To add a new vendor library:

1. Edit `VendorConfig.cmake`
2. Add a new `nexus_register_vendor()` call:

```cmake
nexus_register_vendor(
    NAME MY_VENDOR
    DESCRIPTION "My Vendor Library"
    SUBMODULE_PATH "vendors/my_vendor"
    INCLUDE_DIRS "include"
    PLATFORMS "stm32;native"  # Empty = all platforms
    REQUIRED  # Or omit for optional
)
```

3. Add the submodule (if using Git submodules):

```bash
git submodule add https://github.com/vendor/repo.git vendors/my_vendor
```

## Resolution Priority

When resolving a vendor path, the system tries sources in this order:

1. **CMake cache variable**: `NEXUS_VENDOR_<NAME>_DIR`
   - Highest priority
   - Set via `-D` flag or `set(... CACHE ...)`

2. **Environment variable**: `NEXUS_<NAME>_DIR`
   - Second priority
   - Useful for CI/CD and shared environments

3. **System package**: `find_package(<NAME>)`
   - Only if `FIND_PACKAGE TRUE` is specified
   - Useful for system-installed libraries

4. **Git submodule**: Specified in `SUBMODULE_PATH`
   - Default source
   - Auto-initialized if missing

5. **Auto-download**: From `DOWNLOAD_URL`
   - Future feature
   - Requires `NEXUS_ENABLE_AUTO_DOWNLOAD=ON`

## Examples

### Example 1: STM32F4 Project

```cmake
cmake_minimum_required(VERSION 3.21)
project(my_stm32_project)

# Load vendor configuration
include(VendorConfig)

# Initialize all STM32 vendors
nexus_init_platform_vendors(PLATFORM stm32)

# Resolve specific vendors
nexus_resolve_vendor(CMSIS_CORE RESULT_VAR CMSIS_PATH)
nexus_resolve_vendor(HAL_DRIVER_F4 RESULT_VAR HAL_PATH)

# Use in your targets
add_executable(my_app main.c)
target_include_directories(my_app PRIVATE
    ${CMSIS_PATH}/CMSIS/Core/Include
    ${HAL_PATH}/Inc
)
```

### Example 2: Native Testing

```cmake
# Load vendor configuration
include(VendorConfig)

# Try system package first, fallback to submodule
nexus_resolve_vendor_path(GTEST
    RESULT_VAR GTEST_PATH
    FIND_PACKAGE TRUE
    REQUIRED FALSE  # Optional for native builds
)

if(GTEST_PATH)
    add_subdirectory(${GTEST_PATH} gtest)
    enable_testing()
endif()
```

### Example 3: CI/CD with Custom Paths

```yaml
# .github/workflows/build.yml
- name: Setup Vendors
  run: |
    # Use cached vendor libraries
    export NEXUS_CMSIS_CORE_DIR=/opt/cache/cmsis
    export NEXUS_HAL_DRIVER_F4_DIR=/opt/cache/hal_f4
    cmake -B build

- name: Build
  run: cmake --build build
```

## Benefits

### For Developers
- ✅ Flexible dependency sources
- ✅ Easy to override paths
- ✅ Auto-initialization of submodules
- ✅ Clear error messages

### For CI/CD
- ✅ Support for cached dependencies
- ✅ Environment variable configuration
- ✅ System package integration
- ✅ Faster builds

### For Projects
- ✅ Centralized vendor management
- ✅ Platform-specific dependencies
- ✅ Easy to add new vendors
- ✅ Consistent configuration

## Troubleshooting

### Vendor Not Found

If you see "Cannot resolve vendor path", try:

1. Check if submodule is initialized:
   ```bash
   git submodule status
   ```

2. Initialize manually:
   ```bash
   ./scripts/setup_deps.sh --platform=stm32 --series=f4
   ```

3. Set custom path:
   ```bash
   cmake -DNEXUS_VENDOR_CMSIS_CORE_DIR=/path/to/cmsis ..
   ```

### Wrong Vendor Version

To use a specific version:

1. Update submodule:
   ```bash
   cd vendors/arm/CMSIS_5
   git checkout v5.9.0
   cd ../../..
   git add vendors/arm/CMSIS_5
   ```

2. Or use custom path:
   ```bash
   export NEXUS_CMSIS_CORE_DIR=/path/to/cmsis-5.9.0
   ```

### Clear Vendor Cache

To reset vendor paths:

```bash
# Remove CMake cache
rm -rf build/CMakeCache.txt

# Reconfigure
cmake -B build
```

## Future Enhancements

- [ ] Auto-download support
- [ ] Checksum verification
- [ ] Version constraints
- [ ] Dependency graph visualization
- [ ] Vendor update automation

## See Also

- [NexusVendor.cmake](../modules/NexusVendor.cmake) - Low-level vendor resolution
- [Dependency Quick Start](../../docs/DEPENDENCY_QUICK_START.md) - User guide
- [Dependency Management Solution](../../docs/dependency-management-solution.md) - Complete solution
