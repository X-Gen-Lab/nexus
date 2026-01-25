<!-- 
平台支持 PR 模板
适用于添加新平台支持或平台特定功能
-->

## 📝 Description
<!-- 描述新平台支持或平台特定更改 -->



## 🖥️ Platform Information

### Target Platform
- **MCU Family**: [e.g., STM32H7, ESP32-S3, nRF52840]
- **Specific Model**: [e.g., STM32H743ZI]
- **Core Architecture**: [e.g., Cortex-M7, Xtensa LX7]
- **Development Board**: [e.g., NUCLEO-H743ZI]

### Supported Features
- [ ] GPIO
- [ ] UART
- [ ] SPI
- [ ] I2C
- [ ] Timer
- [ ] ADC
- [ ] DAC
- [ ] PWM
- [ ] DMA
- [ ] CAN
- [ ] USB
- [ ] Ethernet
- [ ] Other: ___________

## 🔗 Related Issues
- Fixes #
- Related to #

## 📋 Implementation Details

### Platform Files Added
- `platforms/<platform>/include/` - Platform headers
- `platforms/<platform>/src/` - Platform implementation
- `platforms/<platform>/startup/` - Startup code
- `platforms/<platform>/linker/` - Linker scripts
- `cmake/platforms/<platform>.cmake` - CMake configuration

### HAL Modules Implemented
- [ ] GPIO - Full implementation
- [ ] UART - Full implementation
- [ ] SPI - Full implementation
- [ ] I2C - Full implementation
- [ ] Timer - Full implementation
- [ ] ADC - Partial implementation
- [ ] Other modules: ___________

### Build System Integration
- [ ] CMake configuration added
- [ ] Toolchain file updated
- [ ] Platform-specific options added
- [ ] Kconfig integration

## 🧪 Testing

### Hardware Testing
- [ ] Tested on real hardware
- **Board**: [board name]
- **Firmware Version**: [version]

### Test Results
- [ ] GPIO tests pass
- [ ] UART tests pass
- [ ] SPI tests pass
- [ ] I2C tests pass
- [ ] Timer tests pass
- [ ] Example applications work

### Test Applications
- [ ] Blinky example works
- [ ] UART echo works
- [ ] SPI loopback works
- [ ] I2C scan works
- [ ] Custom test: ___________

## 📚 Documentation

### Added Documentation
- [ ] Platform README
- [ ] Porting guide updated
- [ ] Hardware setup guide
- [ ] Pin mapping documentation
- [ ] Clock configuration guide
- [ ] Example applications

### Documentation Files
- `platforms/<platform>/README.md`
- `platforms/<platform>/docs/`
- `docs/platforms/<platform>.md`

## 🔄 Compatibility

### Toolchain Requirements
- **Compiler**: [e.g., arm-none-eabi-gcc 10.3+]
- **CMake**: [e.g., 3.16+]
- **Python**: [e.g., 3.8+]

### RTOS Support
- [ ] Bare-metal
- [ ] FreeRTOS
- [ ] RT-Thread
- [ ] Zephyr
- [ ] Other: ___________

## 📦 Dependencies

### Vendor SDK
- [ ] No vendor SDK required
- [ ] Vendor SDK included
- **SDK Version**: [version]
- **License**: [license]

### External Dependencies
- [ ] CMSIS
- [ ] HAL Library
- [ ] Other: ___________

## 🚀 Getting Started

### Build Instructions
```bash
# Configure for new platform
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
  -DNEXUS_PLATFORM=<platform> \
  -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Flash (if applicable)
# <flash command>
```

### Quick Test
```bash
# Build and flash blinky example
cd applications/blinky
# <build and flash commands>
```

## ✅ Pre-Submission Checklist

### Implementation
- [ ] All planned HAL modules implemented
- [ ] Platform initialization code complete
- [ ] Clock configuration implemented
- [ ] Interrupt handling implemented
- [ ] DMA support (if applicable)

### Testing
- [ ] Tested on real hardware
- [ ] All HAL modules tested
- [ ] Example applications tested
- [ ] No regressions on other platforms

### Documentation
- [ ] Platform README complete
- [ ] Pin mapping documented
- [ ] Clock tree documented
- [ ] Examples documented
- [ ] Porting guide updated

### Code Quality
- [ ] Code follows project standards
- [ ] Comments follow Doxygen style
- [ ] No platform-specific hacks in common code
- [ ] Proper error handling

## 💬 Additional Notes
<!-- 任何平台特定的注意事项 -->



## 📸 Hardware Setup
<!-- 如果可能，添加硬件连接图或照片 -->



---

**感谢您为 Nexus 添加新平台支持！** 🎉
