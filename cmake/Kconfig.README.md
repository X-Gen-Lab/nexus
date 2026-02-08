# CMake Kconfig Architecture

This document describes the Kconfig architecture for the CMake build system configuration.

## Overview

The CMake Kconfig system is organized into modular files for better maintainability and clarity:

```
cmake/
├── Kconfig                 # Main entry point
├── Kconfig.build          # Build type and options
├── Kconfig.toolchain      # Toolchain selection and ARM architecture
├── Kconfig.compiler       # Compiler-specific options
└── Kconfig.linker         # Linker configuration
```

## File Structure

### Kconfig (Main Entry)

The main entry point that sources all other configuration files.

**Purpose**: Organize build system configuration into logical modules

**Contents**:
- Sources Kconfig.build
- Sources Kconfig.toolchain
- Sources Kconfig.compiler
- Sources Kconfig.linker

### Kconfig.build

Build type selection and build-related options.

**Configuration Options**:
- **Build Type**: Debug, Release, MinSizeRel, RelWithDebInfo
- **Build Targets**: Tests, Examples, Benchmarks, Documentation
- **Code Quality**: Coverage, Sanitizers, Static Analysis, Format Checks
- **Build Performance**: ccache, Parallel Build
- **Verbose Output**: Detailed build commands

**Key Features**:
- Clear descriptions of each build type
- Platform-specific options (e.g., tests only on native)
- Quality and analysis tools integration

### Kconfig.toolchain

Toolchain selection and ARM architecture configuration.

**Configuration Options**:
- **Native Toolchains**: GCC, Clang, MSVC
- **ARM Toolchains**: ARM GCC, ARM Clang, IAR
- **ARM Architecture**: CPU selection (Cortex-M0/M0+/M3/M4/M7/M33)
- **FPU Configuration**: FPU type and Float ABI
- **Toolchain Files**: Automatic selection based on toolchain

**Key Features**:
- Detailed toolchain descriptions with pros/cons
- Automatic CPU selection based on platform (STM32)
- Comprehensive FPU and Float ABI configuration
- Clear explanations of each option

### Kconfig.compiler

Compiler-specific options and flags.

**Configuration Options**:
- **Character Types**: unsigned char, short enums, short wchar_t
- **Code Generation**: builtin functions, function sections, common blocks
- **Warning Levels**: None, Basic, Standard, Extra, Pedantic
- **Debug Format**: DWARF 2/3/4/5
- **Optimization**: LTO, size optimization
- **Standard Library**: microlib, newlib-nano, nosys
- **ARM-Specific**: APCS interwork, ROPI, RWPI, LDM/STM splitting
- **Custom Flags**: User-defined compiler flags

**Key Features**:
- Five warning levels with clear descriptions
- Detailed explanations of each option
- Toolchain-specific options (ARM Clang, ARM GCC, IAR)
- Benefits and drawbacks for each option

### Kconfig.linker

Linker script and memory configuration (embedded targets only).

**Configuration Options**:
- **Linker Script**: Custom linker script path
- **Memory Layout**: Stack size, Heap size
- **Linker Optimization**: Garbage collection, LTO
- **Linker Output**: Map file, Cross-reference, Memory usage
- **Custom Flags**: User-defined linker flags

**Key Features**:
- Only available for embedded targets (not native)
- Hexadecimal memory size configuration
- Detailed memory size recommendations
- Clear explanations of linker optimizations

## Configuration Flow

```
User runs menuconfig
    ↓
Kconfig (main entry)
    ↓
├─→ Kconfig.build
│   ├─ Select build type
│   ├─ Enable/disable tests, examples
│   └─ Configure quality tools
│
├─→ Kconfig.toolchain
│   ├─ Select toolchain (GCC/Clang/MSVC/ARM GCC/ARM Clang/IAR)
│   └─ [If ARM] Configure CPU, FPU, Float ABI
│
├─→ Kconfig.compiler
│   ├─ Configure character types
│   ├─ Set warning level
│   ├─ Configure debug format
│   ├─ Enable optimizations
│   └─ Configure standard library
│
└─→ Kconfig.linker
    ├─ [If embedded] Set linker script
    ├─ Configure stack/heap sizes
    └─ Enable linker optimizations
```

## Usage

### Basic Configuration

1. Run menuconfig:
   ```bash
   cmake -B build
   cd build
   make menuconfig
   ```

2. Navigate through menus:
   - **Build Configuration**: Select build type and options
   - **Toolchain Configuration**: Select toolchain and architecture
   - **Compiler Configuration**: Configure compiler options
   - **Linker Configuration**: Configure linker (embedded only)

3. Save configuration and build:
   ```bash
   # Configuration saved to .config
   make
   ```

### Common Scenarios

#### Development Build (Native)
```
Build Configuration:
  - Build Type: Debug
  - Build Tests: Yes
  - Build Examples: Yes
  - Enable Sanitizers: Yes

Toolchain Configuration:
  - Toolchain: GCC or Clang

Compiler Configuration:
  - Warning Level: Standard
  - Warnings as Errors: No
```

#### Production Build (Embedded)
```
Build Configuration:
  - Build Type: Release
  - Build Tests: No
  - Build Examples: No

Toolchain Configuration:
  - Toolchain: ARM GCC or ARM Clang
  - CPU: Cortex-M4
  - FPU: FPv4-SP
  - Float ABI: Hard

Compiler Configuration:
  - Warning Level: Standard
  - Warnings as Errors: Yes
  - LTO: Yes
  - Use nano specs: Yes (ARM GCC)

Linker Configuration:
  - Stack Size: 0x1000 (4KB)
  - Heap Size: 0x4000 (16KB)
  - GC Sections: Yes
  - LTO: Yes
```

#### Size-Optimized Build
```
Build Configuration:
  - Build Type: MinSizeRel

Compiler Configuration:
  - LTO: Yes
  - Optimize for Size: Yes
  - Use microlib: Yes (ARM Clang)
  - Use nano specs: Yes (ARM GCC)

Linker Configuration:
  - GC Sections: Yes
  - LTO: Yes
```

## Design Principles

### 1. Modularity
Each configuration aspect is in its own file for easy maintenance and navigation.

### 2. Clear Documentation
Every option includes:
- Brief description
- Detailed help text
- Use cases and recommendations
- Benefits and drawbacks
- Toolchain-specific flags

### 3. Logical Organization
Options are grouped by functionality:
- Build options together
- Toolchain options together
- Compiler options together
- Linker options together

### 4. Platform Awareness
Options are automatically enabled/disabled based on:
- Selected platform (native vs embedded)
- Selected toolchain
- Selected CPU architecture

### 5. Sensible Defaults
Default values are chosen for:
- Common use cases
- Best practices
- Safety and reliability

## Extending the Configuration

### Adding New Options

1. Choose the appropriate file:
   - Build-related → `Kconfig.build`
   - Toolchain-related → `Kconfig.toolchain`
   - Compiler-related → `Kconfig.compiler`
   - Linker-related → `Kconfig.linker`

2. Add the option with:
   - Clear name (CONFIG_XXX)
   - Descriptive prompt
   - Comprehensive help text
   - Appropriate default value
   - Dependencies (if any)

3. Document the option in this README

### Adding New Toolchains

1. Add toolchain choice in `Kconfig.toolchain`
2. Add toolchain name mapping
3. Add toolchain file path
4. Create toolchain file in `cmake/toolchains/`
5. Test configuration and build

## Best Practices

### For Users

1. **Start with defaults**: Default values are sensible for most cases
2. **Read help text**: Press '?' in menuconfig to see detailed help
3. **Save configurations**: Save different configurations for different scenarios
4. **Test thoroughly**: Test configuration changes before production

### For Developers

1. **Keep options focused**: Each option should do one thing
2. **Provide clear help**: Help text should explain what, why, and when
3. **Use dependencies**: Disable irrelevant options automatically
4. **Test all combinations**: Ensure options work together correctly
5. **Document changes**: Update this README when adding options

## Troubleshooting

### Option Not Visible

**Cause**: Dependencies not met

**Solution**: Check dependencies with '?' in menuconfig

### Build Fails After Configuration

**Cause**: Incompatible option combination

**Solution**: Review configuration, check toolchain support

### Custom Flags Not Applied

**Cause**: Flags may be overridden by other options

**Solution**: Check flag order in generated build files

## References

- [Kconfig Language](https://www.kernel.org/doc/html/latest/kbuild/kconfig-language.html)
- [CMake Documentation](https://cmake.org/documentation/)
- [ARM Compiler Documentation](https://developer.arm.com/documentation/)
- [GCC ARM Embedded](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain)
