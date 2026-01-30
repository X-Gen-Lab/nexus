# Nexus CMake Modules

This directory contains CMake modules for the Nexus build system redesign.

## Core Modules

### NexusBuild.cmake
Core build functions for creating libraries, executables, and tests.

**Functions:**
- `nexus_add_library()` - Create a Nexus library with standard settings
- `nexus_add_executable()` - Create a Nexus executable with standard settings
- `nexus_add_test()` - Create a Nexus test with Google Test integration
- `nexus_add_precompiled_header()` - Add precompiled header to target
- `nexus_generate_binary()` - Generate binary files (.bin, .hex) for embedded targets
- `nexus_generate_memory_report()` - Generate memory usage report

### NexusCache.cmake
Build cache management for local and remote caching.

**Functions:**
- `nexus_enable_cache()` - Enable and configure build cache
- `nexus_configure_cache()` - Configure cache policy
- `nexus_clean_cache()` - Clean build cache
- `nexus_cache_stats()` - Display cache statistics
- `nexus_generate_cache_key()` - Generate cache key for source file

**Features:**
- Integrates with ccache/sccache
- Content-addressable storage using SHA-256
- Configurable compression and encryption
- Automatic cache cleanup

### NexusDependency.cmake
Dependency management and resolution.

**Functions:**
- `nexus_declare_dependency()` - Declare a dependency
- `nexus_resolve_dependency()` - Resolve a dependency
- `nexus_add_vendor_library()` - Add a vendor library
- `nexus_generate_lockfile()` - Generate dependency lock file
- `nexus_load_lockfile()` - Load dependency lock file
- `nexus_check_circular_dependencies()` - Check for circular dependencies
- `nexus_get_dependency_order()` - Get dependencies in topological order

**Features:**
- SemVer version resolution
- Dependency lock files (nexus.lock)
- Circular dependency detection
- Vendor library integration

### NexusKconfig.cmake
Kconfig configuration system integration.

**Functions:**
- `nexus_load_kconfig()` - Load Kconfig configuration
- `nexus_generate_config_header()` - Generate configuration header file
- `nexus_validate_kconfig()` - Validate Kconfig configuration
- `nexus_add_kconfig_dependency()` - Add Kconfig dependency to target
- `nexus_generate_default_config()` - Generate default configuration
- `nexus_detect_config_change()` - Detect configuration changes

**Features:**
- Automatic configuration header generation
- Configuration change detection
- Configuration validation
- CMake variable mapping

### NexusPlatform.cmake
Platform detection and configuration (existing module, enhanced).

**Functions:**
- `nexus_detect_platform()` - Detect host platform
- `nexus_detect_compiler()` - Detect compiler
- `nexus_detect_generator()` - Detect CMake generator
- `nexus_configure_platform()` - Configure platform-specific settings
- `nexus_configure_compiler_flags()` - Configure compiler flags
- `nexus_set_default_build_type()` - Set default build type
- `nexus_configure_output_directories()` - Configure output directories

## Usage

Include the modules in your CMakeLists.txt:

```cmake
# Add module path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

# Include modules
include(NexusBuild)
include(NexusCache)
include(NexusDependency)
include(NexusKconfig)
include(NexusPlatform)
```

## Example

```cmake
# Enable build cache
nexus_enable_cache(
    LOCAL_DIR ${CMAKE_BINARY_DIR}/.cache
    MAX_SIZE 1024
)

# Declare dependencies
nexus_declare_dependency(
    NAME FreeRTOS
    VERSION "10.5.1"
    REQUIRED
)

# Load Kconfig configuration
nexus_load_kconfig(
    KCONFIG_FILE ${CMAKE_SOURCE_DIR}/Kconfig
    CONFIG_FILE ${CMAKE_SOURCE_DIR}/.config
    OUTPUT_HEADER ${CMAKE_SOURCE_DIR}/nexus_config.h
)

# Create a library
nexus_add_library(
    TARGET my_library
    SOURCES src/file1.c src/file2.c
    INCLUDES include/
    DEPS FreeRTOS
)

# Create an executable
nexus_add_executable(
    TARGET my_app
    SOURCES app/main.c
    DEPS my_library
    LINKER_SCRIPT linker/app.ld
)

# Create a test
nexus_add_test(
    TARGET test_my_library
    SOURCES tests/test_my_library.cpp
    DEPS my_library
    LABELS "unit"
    TIMEOUT 30
)
```

## Testing

Tests for these modules are located in `tests/build_system/`:

- C++ unit tests using Google Test
- Python property-based tests using Hypothesis

See `tests/build_system/README.md` for more information.
