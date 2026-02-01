# Nexus CMake Modules

This directory contains the optimized CMake module structure for the Nexus build system.

## Module Architecture

The Nexus build system is organized into **7 core modules**, each with a clear responsibility:

```
cmake/modules/
├── NexusCore.cmake          # Platform detection, toolchain abstraction, compiler configuration
├── NexusBuild.cmake          # Library, executable, and test creation
├── NexusConfig.cmake         # Kconfig integration and dependency management
├── NexusOptimization.cmake   # Cache, incremental builds, performance, resource management
├── NexusQuality.cmake        # Testing, diagnostics, reproducible builds
├── NexusExtension.cmake      # Plugin system and build isolation
└── NexusHelpers.cmake        # Error handling, logging, input validation
```

## Module Overview

### 1. NexusCore.cmake
**Core platform and toolchain functionality**

**Key Functions:**
- `nexus_configure_platform()` - Auto-detect and configure platform
- `nexus_detect_compiler()` - Detect compiler type and version
- `nexus_configure_compiler_flags()` - Set compiler flags
- `nexus_set_default_build_type()` - Configure build type
- `nexus_generate_bin()` - Generate binary files (toolchain-agnostic)
- `nexus_generate_hex()` - Generate hex files (toolchain-agnostic)
- `nexus_set_linker_script()` - Set linker script (toolchain-agnostic)

**Features:**
- Automatic platform detection (Windows/Linux/macOS, native/embedded)
- Compiler detection (GCC, Clang, MSVC, ARM GCC, ARM Clang, IAR)
- Toolchain abstraction layer for ARM toolchains
- Cross-compilation support

---

### 2. NexusBuild.cmake
**Build target creation**

**Key Functions:**
- `nexus_add_library()` - Create library with standard settings
- `nexus_add_executable()` - Create executable with linker script support
- `nexus_add_test()` - Create test with Google Test integration
- `nexus_add_vendor_library()` - Add vendor/third-party library
- `nexus_generate_binary()` - Generate .bin and .hex files
- `nexus_generate_memory_report()` - Display memory usage
- `nexus_add_precompiled_header()` - Add PCH support

**Features:**
- Unified API for all target types
- Automatic binary generation for embedded targets
- Memory usage reporting
- Vendor library integration

---

### 3. NexusConfig.cmake
**Configuration and dependency management**

**Key Functions:**
- `nexus_load_kconfig()` - Load Kconfig and generate header
- `load_kconfig()` - Parse .config file
- `apply_kconfig_to_cmake()` - Apply config to CMake variables
- `nexus_validate_kconfig()` - Validate configuration
- `nexus_declare_dependency()` - Declare a dependency
- `nexus_resolve_dependency()` - Resolve dependency with SemVer
- `nexus_check_circular_dependencies()` - Detect circular deps

**Features:**
- Kconfig integration with Python generator
- Configuration validation
- SemVer version resolution
- Dependency lock files (nexus.lock)
- Circular dependency detection

---

### 4. NexusOptimization.cmake
**Build performance and resource optimization**

**Key Functions:**
- `nexus_enable_cache()` - Enable ccache/sccache
- `nexus_cache_stats()` - Display cache statistics
- `nexus_enable_incremental_build()` - Enable incremental compilation
- `nexus_enable_incremental_linking()` - Enable incremental linking
- `nexus_configure_parallel_build()` - Configure parallel jobs
- `nexus_resource_init_adaptive_parallelism()` - Adaptive parallelism
- `nexus_resource_cleanup_temp_files()` - Clean temporary files

**Features:**
- Build cache management (ccache/sccache)
- Incremental build with dependency tracking
- Hash-based change detection
- Adaptive parallelism based on system resources
- Memory monitoring and adjustment
- Temporary file cleanup

---

### 5. NexusQuality.cmake
**Testing, diagnostics, and reproducibility**

**Key Functions:**
- `nexus_test_init()` - Initialize test framework
- `nexus_test_init_gtest()` - Setup Google Test
- `nexus_test_discover()` - Discover and register tests
- `nexus_test_configure_coverage()` - Enable code coverage
- `nexus_log()` - Structured logging
- `nexus_generate_build_report()` - Generate build report
- `nexus_enable_reproducible_build()` - Enable reproducible builds
- `nexus_set_deterministic_flags()` - Set deterministic flags

**Features:**
- Google Test integration with auto-fetch
- Parallel test execution
- Code coverage analysis (gcov/lcov)
- Structured logging system
- Build reports (text/HTML/JSON)
- Reproducible builds with deterministic flags

---

### 6. NexusExtension.cmake
**Plugin system and build isolation**

**Key Functions:**
- `nexus_register_plugin()` - Register a plugin
- `nexus_load_plugin()` - Load plugin with signature verification
- `nexus_register_hook()` - Register lifecycle hook
- `nexus_invoke_hooks()` - Invoke hooks at lifecycle points
- `nexus_isolation_init()` - Initialize build isolation
- `nexus_isolation_init_environment()` - Isolate environment variables
- `nexus_isolation_audit_log()` - Audit logging

**Features:**
- Plugin registration and loading
- Lifecycle hooks (pre/post configure, build, test)
- Plugin signature verification
- Environment variable isolation
- File system access control
- Audit logging for security compliance

---

### 7. NexusHelpers.cmake
**Helper utilities**

**Key Functions:**
- `nexus_fatal_error()` - Fatal error with formatted message
- `nexus_warning()` - Warning with formatted message
- `nexus_require_file()` - Require file to exist
- `nexus_require_directory()` - Require directory to exist
- `nexus_log()` - Log message with level control
- `nexus_print_configuration_summary()` - Print enhanced config summary
- `nexus_require_variable()` - Validate variable is defined
- `nexus_validate_enum()` - Validate enum value
- `nexus_validate_range()` - Validate numeric range

**Features:**
- Unified error handling
- Structured logging with levels (VERBOSE/STATUS/WARNING/ERROR)
- Enhanced configuration summary with box drawing
- Input validation helpers
- Better user experience

---

## Quick Start

### Basic Usage

```cmake
# Add module path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

# Include core modules
include(NexusCore)
include(NexusBuild)
include(NexusConfig)

# Configure platform
nexus_configure_platform()

# Load Kconfig
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
)

# Create an executable
nexus_add_executable(
    TARGET my_app
    SOURCES app/main.c
    DEPS my_library
    LINKER_SCRIPT linker/app.ld
)
```

### With Optimization

```cmake
include(NexusOptimization)

# Enable build cache
nexus_enable_cache(
    LOCAL_DIR ${CMAKE_BINARY_DIR}/.cache
    MAX_SIZE 1024
)

# Enable incremental build
nexus_enable_incremental_build(my_library)
```

### With Testing

```cmake
include(NexusQuality)

# Initialize test framework
nexus_test_init()

# Create a test
nexus_add_test(
    TARGET test_my_library
    SOURCES tests/test_my_library.cpp
    DEPS my_library
    LABELS "unit"
    TIMEOUT 30
)
```

### With Plugins

```cmake
include(NexusExtension)

# Register a plugin
nexus_register_plugin(
    NAME my_plugin
    VERSION "1.0.0"
    SCRIPT ${CMAKE_SOURCE_DIR}/plugins/my_plugin.cmake
    HOOKS pre_build post_build
)

# Load plugin
nexus_load_plugin(my_plugin)
```

## Migration from Old Structure

The old 16-module structure has been consolidated into 6 modules:

| Old Modules | New Module |
|-------------|------------|
| NexusPlatform.cmake<br>NexusToolchain.cmake<br>NexusHelpers.cmake (partial) | **NexusCore.cmake** |
| NexusBuild.cmake<br>NexusHelpers.cmake (partial) | **NexusBuild.cmake** |
| LoadKconfig.cmake<br>NexusKconfig.cmake<br>NexusDependency.cmake | **NexusConfig.cmake** |
| NexusCache.cmake<br>NexusIncremental.cmake<br>NexusPerformance.cmake<br>NexusResource.cmake | **NexusOptimization.cmake** |
| NexusTest.cmake<br>NexusDiagnostics.cmake<br>NexusReproducible.cmake | **NexusQuality.cmake** |
| NexusPlugin.cmake<br>NexusIsolation.cmake | **NexusExtension.cmake** |

### Backward Compatibility

All modules are now finalized. The old 16-module structure has been fully replaced by the new 7-module structure:

```cmake
# New structure (current)
include(NexusCore)         # Platform, toolchain, compiler
include(NexusBuild)        # Build targets
include(NexusConfig)       # Configuration management
include(NexusOptimization) # Performance optimization
include(NexusQuality)      # Testing and quality
include(NexusExtension)    # Plugin system
# NexusHelpers is automatically loaded by NexusCore
```

## Benefits of New Structure

✅ **62.5% fewer files** (16 → 6 modules)
✅ **Clear separation of concerns** (Core → Build → Config → Optimization → Quality → Extension)
✅ **No functional overlap** (eliminated redundancy)
✅ **Easier to maintain** (related functions grouped together)
✅ **Better discoverability** (logical module names)
✅ **Consistent API** (unified function naming)

## Testing

Tests for these modules are located in `tests/build_system/`:

- C++ unit tests using Google Test
- Python property-based tests using Hypothesis

See `tests/build_system/README.md` for more information.

## Documentation

For detailed documentation on each module, see:
- [NexusCore.cmake](NexusCore.cmake) - Platform and toolchain
- [NexusBuild.cmake](NexusBuild.cmake) - Build targets
- [NexusConfig.cmake](NexusConfig.cmake) - Configuration
- [NexusOptimization.cmake](NexusOptimization.cmake) - Performance
- [NexusQuality.cmake](NexusQuality.cmake) - Testing and quality
- [NexusExtension.cmake](NexusExtension.cmake) - Plugins and isolation
