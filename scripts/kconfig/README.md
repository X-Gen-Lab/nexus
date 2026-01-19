# Kconfig Configuration Tools

This directory contains tools for managing the Nexus platform's Kconfig-based configuration system.

## Tools Overview

### Core Configuration Tools

#### `generate_config.py`
Generates C header files from Kconfig `.config` files.

**Usage:**
```bash
python scripts/kconfig/generate_config.py --config .config --output nexus_config.h
python scripts/kconfig/generate_config.py --default --output nexus_config.h
```

**Features:**
- Converts CONFIG_ symbols to NX_CONFIG_ macros
- Handles boolean, integer, string, and hexadecimal values
- Generates properly formatted C headers with include guards
- Supports default configuration generation

---

#### `validate_kconfig.py`
Validates Kconfig file syntax, structure, and dependencies.

**Usage:**
```bash
python scripts/kconfig/validate_kconfig.py Kconfig
python scripts/kconfig/validate_kconfig.py platforms/stm32/Kconfig
```

**Checks:**
- Block structure (if/endif, menu/endmenu, choice/endchoice)
- Source file existence
- Circular dependencies
- Undefined symbol references
- Range constraint violations
- Default value consistency

---

#### `kconfig_migrate.py`
Migrates configuration files between versions.

**Usage:**
```bash
python scripts/kconfig/kconfig_migrate.py --input old.config --output new.config --version 2.0
```

**Features:**
- Symbol renaming and mapping
- Deprecated option warnings
- Version tracking
- Backward compatibility support

---

#### `kconfig_diff.py`
Compares two configuration files and shows differences.

**Usage:**
```bash
python scripts/kconfig/kconfig_diff.py .config platforms/native/.config
python scripts/kconfig/kconfig_diff.py --format json config1 config2
```

**Output Formats:**
- Text (default): Human-readable diff
- JSON: Machine-readable format for automation

---

#### `generate_config_docs.py`
Generates documentation from Kconfig files.

**Usage:**
```bash
python scripts/kconfig/generate_config_docs.py Kconfig --output docs/config_reference.md
```

**Features:**
- Extracts all configuration options
- Includes help text and default values
- Generates markdown documentation
- Organizes by platform and component

---

### Testing and Verification Tools

#### `test_kconfiglib.py`
Tests kconfiglib installation and basic functionality.

**Usage:**
```bash
python scripts/kconfig/test_kconfiglib.py
```

---

#### `verify_kconfig.py`
Quick verification script for Kconfig structure.

**Usage:**
```bash
python scripts/kconfig/verify_kconfig.py
```

---

#### `test_kconfig_structure.py`
Tests the overall Kconfig structure and organization.

**Usage:**
```bash
python scripts/kconfig/test_kconfig_structure.py
```

---

## Integration with Build System

The Kconfig tools are integrated into the CMake build system:

```cmake
# CMakeLists.txt
execute_process(
    COMMAND ${Python3_EXECUTABLE} scripts/kconfig/generate_config.py
            --config ${CONFIG_FILE}
            --output ${OUTPUT_FILE}
)
```

## Configuration Workflow

1. **Edit Configuration**: Use menuconfig or edit `.config` directly
2. **Validate**: Run `validate_kconfig.py` to check for errors
3. **Generate Headers**: Run `generate_config.py` to create C headers
4. **Build**: CMake automatically regenerates headers when config changes

## File Organization

```
scripts/kconfig/
├── __init__.py                    # Package initialization
├── README.md                      # This file
├── generate_config.py             # Config → C header generation
├── validate_kconfig.py            # Kconfig validation
├── kconfig_migrate.py             # Configuration migration
├── kconfig_diff.py                # Configuration comparison
├── generate_config_docs.py        # Documentation generation
├── test_kconfiglib.py             # kconfiglib testing
├── verify_kconfig.py              # Quick verification
└── test_kconfig_structure.py      # Structure testing
```

## Dependencies

- Python 3.7+
- kconfiglib (installed via `pip install kconfiglib`)

## See Also

- [Configuration Guide](../../docs/configuration_guide.md)
- [Kconfig Writing Guide](../../docs/kconfig_writing_guide.md)
- [Configuration Reference](../../docs/config_reference.md)
