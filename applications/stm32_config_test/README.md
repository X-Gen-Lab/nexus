# STM32 Configuration Test Application

## Overview

This application validates that Kconfig configuration settings are properly applied to the STM32 platform. It demonstrates:

- LED blinking using SysTick timer
- Configuration display via UART/ITM
- Interrupt priority configuration
- Peripheral configuration validation

## Features

### LED Blinking
- Uses SysTick interrupt for timing
- Blinks LED every 500ms
- Validates that SysTick priority configuration is applied

### Configuration Display
- Displays platform configuration (clock, stack, heap)
- Displays interrupt configuration (NVIC, SysTick, PendSV)
- Displays peripheral configuration (UART settings)
- Outputs via printf (UART or ITM)

### Configuration Validation
- Verifies Kconfig settings are applied
- Shows actual vs configured values
- Helps debug configuration issues

## Hardware Requirements

### Supported Boards

#### STM32F407 Discovery
- **MCU**: STM32F407VGT6
- **LED**: Green LED on PD12
- **UART**: USART2 (optional, for printf output)
- **Debug**: ST-LINK/V2 (built-in)

### Pin Configuration

| Function | Pin | Description |
|----------|-----|-------------|
| LED | PD12 | Green LED (LD4) |
| UART TX | PA2 | USART2 TX (optional) |
| UART RX | PA3 | USART2 RX (optional) |

## Software Requirements

### Toolchain
- ARM GCC 10.3 or later
- CMake 3.16 or later
- OpenOCD or ST-LINK utilities

### Dependencies
- Nexus Framework
- STM32 HAL Library
- STM32F4xx CMSIS

## Building

### 1. Configure Kconfig

```bash
cd nexus
make menuconfig
```

Navigate to:
```
Platform Configuration → STM32 → Platform Settings
```

Configure the following options:
- NVIC Priority Group (0-4)
- SysTick Priority (0-15)
- PendSV Priority (0-15)
- UART1 Baudrate
- UART1 TX Buffer Size
- UART1 RX Buffer Size

### 2. Generate Configuration

```bash
python scripts/generate_config.py
```

This generates `nexus_config.h` with your settings.

### 3. Build Application

```bash
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make stm32_config_test
```

### 4. Flash to Board

Using OpenOCD:
```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
    -c "program stm32_config_test.elf verify reset exit"
```

Using ST-LINK utilities:
```bash
st-flash write stm32_config_test.bin 0x8000000
```

## Running

### Expected Behavior

1. **LED Blinking**
   - Green LED (PD12) blinks every 500ms
   - Indicates application is running

2. **UART Output** (if configured)
   - Configuration information at startup
   - Status updates every 5 seconds
   - Baudrate: As configured in Kconfig

3. **ITM Output** (if using debugger)
   - Same information as UART
   - Viewable in debugger console

### Sample Output

```
========================================
  STM32 Configuration Test Application
========================================

=== Platform Configuration ===
System Clock: 168000000 Hz
HSE Value: 8000000 Hz
Stack Size: 0x400 bytes
Heap Size: 0x200 bytes

=== Interrupt Configuration ===
NVIC Priority Group: 4
SysTick Priority: 15
PendSV Priority: 15

=== Peripheral Configuration ===
UART1 Baudrate: 115200
UART1 TX Buffer: 256 bytes
UART1 RX Buffer: 256 bytes
UART Overflow Policy: 0

========================================

[INFO] Application started
[INFO] LED will blink every 500 ms
[INFO] Press reset to restart

[INFO] System running with Kconfig settings
[INFO] SYSCLK: 168000000 Hz
[INFO] NVIC Group: 4
```

## Verification

### 1. Visual Verification
- Observe LED blinking at correct rate (2 Hz)
- Fast blinking indicates error

### 2. UART Verification
- Connect UART to PC (115200 baud, 8N1)
- Verify configuration values match Kconfig
- Check for error messages

### 3. Debugger Verification

Using GDB:
```bash
arm-none-eabi-gdb stm32_config_test.elf
(gdb) target remote localhost:3333
(gdb) monitor reset halt
(gdb) load
(gdb) break main
(gdb) continue
```

Check NVIC registers:
```
(gdb) x/1xw 0xE000ED0C  # AIRCR register (priority grouping)
(gdb) x/1xw 0xE000ED23  # SHPR3 register (SysTick priority)
```

Expected values:
- AIRCR[10:8] = 3 (for NVIC_PRIORITYGROUP_4)
- SHPR3[31:24] = 15 (for SysTick priority 15)

## Troubleshooting

### LED Not Blinking

**Possible causes:**
1. Wrong board selected in Kconfig
2. Clock configuration error
3. GPIO initialization failed

**Solutions:**
- Verify board configuration in Kconfig
- Check system clock settings
- Use debugger to step through initialization

### No UART Output

**Possible causes:**
1. UART not configured in Kconfig
2. Wrong baudrate setting
3. Wrong UART pins

**Solutions:**
- Enable UART in Kconfig
- Verify baudrate matches terminal settings
- Check pin configuration for your board

### Configuration Not Applied

**Possible causes:**
1. `nexus_config.h` not regenerated
2. Old build artifacts
3. Macro name mismatch

**Solutions:**
```bash
# Regenerate configuration
python scripts/generate_config.py

# Clean build
rm -rf build
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make stm32_config_test
```

### Fast LED Blinking (Error State)

**Possible causes:**
1. HAL initialization failed
2. Clock configuration failed
3. Assert failed

**Solutions:**
- Use debugger to find error location
- Check Error_Handler() function
- Verify HSE crystal is present and working

## Modifying Configuration

### Change LED Blink Rate

Edit `main.c`:
```c
#define LED_BLINK_PERIOD_MS     1000  /* Change to 1 second */
```

### Change LED Pin

Edit `main.c`:
```c
#define LED_PORT                GPIOD
#define LED_PIN                 GPIO_PIN_13  /* Orange LED */
```

### Add More Configuration Display

Edit `config_display.c` and add your configuration macros:
```c
#ifdef NX_CONFIG_YOUR_SETTING
    printf("Your Setting: %d\n", NX_CONFIG_YOUR_SETTING);
#endif
```

## Testing Different Configurations

### Test 1: Change NVIC Priority Group

1. In Kconfig, set `CONFIG_STM32_NVIC_PRIORITY_GROUP = 3`
2. Regenerate config: `python scripts/generate_config.py`
3. Rebuild and flash
4. Verify output shows: `NVIC Priority Group: 3`

### Test 2: Change SysTick Priority

1. In Kconfig, set `CONFIG_STM32_SYSTICK_PRIORITY = 10`
2. Regenerate config: `python scripts/generate_config.py`
3. Rebuild and flash
4. Verify output shows: `SysTick Priority: 10`
5. Use debugger to verify SHPR3 register

### Test 3: Change UART Settings

1. In Kconfig, set `CONFIG_STM32_UART1_BAUDRATE = 9600`
2. Regenerate config: `python scripts/generate_config.py`
3. Rebuild and flash
4. Verify output shows: `UART1 Baudrate: 9600`
5. Change terminal baudrate to 9600 to see output

## Integration with CI/CD

This application can be used in automated testing:

```bash
#!/bin/bash
# Build and verify configuration test

# Configure
make menuconfig_silent CONFIG_STM32_NVIC_PRIORITY_GROUP=4

# Generate config
python scripts/generate_config.py

# Build
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make stm32_config_test

# Flash and test (requires hardware)
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
    -c "program stm32_config_test.elf verify reset exit"

# Capture UART output and verify
# (implementation depends on your test setup)
```

## References

- [Nexus Framework Documentation](../../docs/README.md)
- [STM32F4 HAL Documentation](https://www.st.com/resource/en/user_manual/dm00105879.pdf)
- [Kconfig Configuration Guide](../../docs/kconfig.md)
- [STM32F407 Discovery User Manual](https://www.st.com/resource/en/user_manual/dm00039084.pdf)

## License

Copyright (c) 2026 Nexus Team

## Support

For issues or questions:
- Check the troubleshooting section above
- Review the main Nexus documentation
- Open an issue in the project repository
