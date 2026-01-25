---
name: Platform Support Request
about: Request support for a new MCU platform
title: '[PLATFORM] Add support for '
labels: platform, enhancement
assignees: ''
---

## Platform Information

### MCU Details
- **MCU Family**: [e.g., STM32H7, ESP32-S3, nRF52840, GD32F4]
- **Specific Model**: [e.g., STM32H743ZI, ESP32-S3-WROOM-1, nRF52840]
- **Manufacturer**: [e.g., STMicroelectronics, Espressif, Nordic, GigaDevice]
- **Core Architecture**: [e.g., Cortex-M7, Xtensa LX7, Cortex-M4, RISC-V]
- **Core Frequency**: [e.g., 480 MHz, 240 MHz, 64 MHz]
- **Flash Size**: [e.g., 2 MB, 4 MB, 1 MB]
- **RAM Size**: [e.g., 1 MB, 512 KB, 256 KB]

### Development Board
- **Board Name**: [e.g., NUCLEO-H743ZI, ESP32-S3-DevKitC-1, nRF52840-DK]
- **Board Manufacturer**: [e.g., STMicroelectronics, Espressif, Nordic]
- **Availability**: [e.g., Widely available, Custom board, Prototype]
- **Price Range**: [e.g., $10-20, $20-50, $50+]

## Use Case
<!-- Why do you need this platform? What's your application? -->

### Application Domain
- [ ] IoT Device
- [ ] Industrial Control
- [ ] Consumer Electronics
- [ ] Medical Device
- [ ] Automotive
- [ ] Robotics
- [ ] Education/Learning
- [ ] Other: ___________

### Project Description
<!-- Describe your project and why this platform is needed -->

## Technical Requirements

### Priority Features
<!-- Which HAL modules are most important for your use case? -->

**Essential** (Must have):
- [ ] GPIO
- [ ] UART
- [ ] SPI
- [ ] I2C
- [ ] Timer
- [ ] Other: ___________

**Important** (Should have):
- [ ] ADC
- [ ] DAC
- [ ] PWM
- [ ] DMA
- [ ] CAN
- [ ] USB
- [ ] Ethernet
- [ ] Other: ___________

**Nice to have** (Could have):
- [ ] Crypto
- [ ] RTC
- [ ] Watchdog
- [ ] Flash
- [ ] SDIO
- [ ] Other: ___________

### RTOS Support
<!-- Which RTOS do you plan to use? -->

- [ ] Bare-metal
- [ ] FreeRTOS
- [ ] RT-Thread
- [ ] Zephyr
- [ ] Other: ___________

### Special Features
<!-- Does this platform have special features you need? -->

- [ ] Low Power Modes
- [ ] Hardware Crypto
- [ ] TrustZone
- [ ] Wireless (WiFi/BLE/LoRa)
- [ ] DSP Instructions
- [ ] FPU
- [ ] MPU
- [ ] Other: ___________

## Resources Available

### Documentation
- [ ] Datasheet: [link]
- [ ] Reference Manual: [link]
- [ ] HAL Library: [link]
- [ ] SDK: [link]
- [ ] Examples: [link]

### Hardware
- [ ] I have hardware available for testing
- [ ] I can provide hardware to maintainers
- [ ] Hardware is commercially available
- [ ] Hardware is custom/prototype

### Toolchain
- [ ] GCC toolchain available
- [ ] Vendor toolchain available
- [ ] LLVM/Clang support
- [ ] Toolchain: ___________

## Contribution Commitment
<!-- How can you help with this platform support? -->

- [ ] I can contribute to the implementation
- [ ] I can test on real hardware
- [ ] I can provide documentation
- [ ] I can create examples
- [ ] I can review code
- [ ] I need full implementation from maintainers

### Time Commitment
- [ ] I can dedicate significant time (> 10 hours/week)
- [ ] I can dedicate moderate time (5-10 hours/week)
- [ ] I can dedicate limited time (< 5 hours/week)
- [ ] I can only provide testing/feedback

## Implementation Plan
<!-- If you plan to contribute, outline your approach -->

### Phase 1: Basic Support
- [ ] Platform initialization
- [ ] GPIO support
- [ ] UART support
- [ ] Build system integration

### Phase 2: Core Peripherals
- [ ] SPI support
- [ ] I2C support
- [ ] Timer support
- [ ] DMA support

### Phase 3: Advanced Features
- [ ] ADC/DAC support
- [ ] PWM support
- [ ] Communication protocols (CAN/USB/Ethernet)
- [ ] Low power modes

### Phase 4: Testing & Documentation
- [ ] Unit tests
- [ ] Integration tests
- [ ] Example applications
- [ ] Documentation

## Timeline
<!-- Estimated timeline for implementation -->

- **Target Start Date**: [YYYY-MM-DD]
- **Expected Completion**: [YYYY-MM-DD]
- **Urgency**: [Low / Medium / High / Critical]

## Similar Platforms
<!-- Are there similar platforms already supported? -->

- Similar to: [e.g., STM32F4 (if requesting STM32F7)]
- Differences: [e.g., Higher clock speed, more peripherals]

## Additional Context
<!-- Add any other context, diagrams, or information -->

### Related Issues/PRs
- Related to: #
- Depends on: #

### Community Interest
<!-- Is there community interest in this platform? -->

- [ ] Multiple users have requested this
- [ ] Active community forum/discussion
- [ ] Popular in specific domain

### Vendor Support
<!-- Does the vendor provide good support? -->

- [ ] Active vendor support
- [ ] Good documentation
- [ ] Regular updates
- [ ] Community forums

## Checklist
<!-- Please check the following before submitting -->

- [ ] I have searched existing issues to avoid duplicates
- [ ] I have provided all technical specifications
- [ ] I have included documentation links
- [ ] I have described my use case clearly
- [ ] I have indicated my contribution commitment 
