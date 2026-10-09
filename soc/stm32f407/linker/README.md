# STM32 Linker Scripts

This directory contains linker scripts for various STM32 series and toolchains.

## Directory Structure

```
linker/
├── stm32f0/
│   └── gcc/          # GCC linker scripts for STM32F0
├── stm32f1/
│   └── gcc/          # GCC linker scripts for STM32F1
├── stm32f4/
│   ├── gcc/          # GCC linker scripts for STM32F4
│   ├── iar/          # IAR linker scripts for STM32F4 (future)
│   └── arm/          # ARM Compiler linker scripts (future)
├── stm32f7/
│   └── gcc/          # GCC linker scripts for STM32F7
├── stm32h7/
│   └── gcc/          # GCC linker scripts for STM32H7
└── README.md         # This file
```

## Linker Script Features

All linker scripts include the following sections:

### Standard Sections

- `.isr_vector` - Interrupt vector table (must be at Flash start)
- `.text` - Program code
- `.rodata` - Read-only data (constants)
- `.data` - Initialized data (copied from Flash to RAM at startup)
- `.bss` - Uninitialized data (zeroed at startup)

### Nexus-Specific Sections

- `.nx_device` - Nexus HAL device registration section
  - Contains device descriptors registered at compile time
  - Bounded by `__nx_device_start` and `__nx_device_end` symbols
  - Used by Nexus HAL device factory for runtime device lookup

### Memory Regions

Each linker script defines memory regions based on the specific chip variant:

#### STM32F0/F1 (Simple Memory Layout)
- `FLASH` - Program Flash memory
- `RAM` - SRAM

#### STM32F4 (With CCM)
- `FLASH` - Program Flash memory
- `RAM` - Main SRAM
- `CCMRAM` - Core Coupled Memory (optional, not accessible by DMA)

#### STM32F7 (Multiple RAM Regions)
- `FLASH` - Program Flash memory
- `DTCMRAM` - Data Tightly Coupled Memory (zero wait-state)
- `SRAM1` - Main SRAM
- `SRAM2` - Additional SRAM
- `ITCMRAM` - Instruction Tightly Coupled Memory (optional)

#### STM32H7 (Complex Memory Layout)
- `FLASH` - Program Flash memory
- `DTCMRAM` - Data Tightly Coupled Memory
- `RAM_D1` - AXI SRAM (D1 domain, for DMA)
- `RAM_D2` - SRAM (D2 domain)
- `RAM_D3` - SRAM (D3 domain)
- `ITCMRAM` - Instruction Tightly Coupled Memory (optional)

## Stack and Heap Configuration

Stack and heap sizes are configured through Kconfig and passed to the linker via CMake:

```cmake
# In CMakeLists.txt
target_link_options(${TARGET} PRIVATE
    --defsym=_Min_Stack_Size=${CONFIG_STM32_STACK_SIZE}
    --defsym=_Min_Heap_Size=${CONFIG_STM32_HEAP_SIZE}
)
```

Default values are provided in the linker scripts if not specified:
- Stack: 1KB (0x400) for F0, 4KB (0x1000) for others
- Heap: 512B (0x200) for F0, 4KB (0x1000) for others

## Creating Linker Scripts for New Chips

To create a linker script for a new chip variant:

1. **Copy the template** for the appropriate series:
   ```bash
   cp stm32f4/gcc/stm32f4xx_template.ld stm32f4/gcc/stm32f4xx_new.ld
   ```

2. **Update memory sizes** in the `MEMORY` section:
   - Check the chip datasheet for Flash and RAM sizes
   - Update `LENGTH` values accordingly
   - Add or remove memory regions as needed (e.g., CCMRAM)

3. **Update file header**:
   - Change filename in `\file` tag
   - Update chip variant in `\brief` description
   - Update memory sizes in description

4. **Test the linker script**:
   - Build a simple application
   - Verify memory usage with `arm-none-eabi-size`
   - Check that all sections are placed correctly

## Memory Layout Example (STM32F407)

```
0x08000000  ┌─────────────────┐
            │  .isr_vector    │  Interrupt vectors
            ├─────────────────┤
            │  .text          │  Program code
            ├─────────────────┤
            │  .rodata        │  Constants
            ├─────────────────┤
            │  .nx_device     │  Device descriptors
            ├─────────────────┤
            │  .data (Flash)  │  Initialized data (copy source)
0x08100000  └─────────────────┘  End of Flash (1MB)

0x20000000  ┌─────────────────┐
            │  .data (RAM)    │  Initialized data (copy destination)
            ├─────────────────┤
            │  .bss           │  Uninitialized data
            ├─────────────────┤
            │  Heap           │  Dynamic allocation
            ├─────────────────┤
            │  Stack          │  Function calls, ISRs
0x20020000  └─────────────────┘  End of RAM (128KB)

0x10000000  ┌─────────────────┐
            │  .ccmram        │  CCM data (optional)
0x10010000  └─────────────────┘  End of CCM (64KB)
```

## Special Memory Sections

### Placing Data in CCM (STM32F4/F7)

```c
/* Place variable in CCM RAM */
__attribute__((section(".ccmram"))) int fast_data;

/* Place array in CCM RAM */
__attribute__((section(".ccmram"))) uint8_t buffer[1024];
```

**Note**: CCM cannot be used for DMA buffers!

### Placing Data in DTCM (STM32F7/H7)

DTCM is used by default for `.data` and `.bss` sections.

### Placing Data in Specific SRAM (STM32F7/H7)

```c
/* STM32F7: Place in SRAM1 */
__attribute__((section(".sram1"))) uint8_t large_buffer[100000];

/* STM32H7: Place in D1 domain (for DMA) */
__attribute__((section(".ram_d1"))) uint8_t dma_buffer[4096];
```

## Troubleshooting

### Link Error: Section Overflow

```
region `RAM' overflowed by XXX bytes
```

**Solution**: Increase RAM size in linker script or reduce memory usage:
- Reduce stack size (`CONFIG_STM32_STACK_SIZE`)
- Reduce heap size (`CONFIG_STM32_HEAP_SIZE`)
- Move large buffers to Flash (const) or external memory
- Use CCM/DTCM for frequently accessed data

### Link Error: Undefined Reference to `__nx_device_start`

```
undefined reference to `__nx_device_start'
```

**Solution**: Ensure the `.nx_device` section is defined in your linker script:
```ld
.nx_device :
{
    . = ALIGN(4);
    __nx_device_start = .;
    KEEP(*(SORT(.nx_device*)))
    __nx_device_end = .;
    . = ALIGN(4);
} >FLASH
```

### Runtime Error: Hard Fault on Startup

**Possible causes**:
1. Stack size too small - increase `CONFIG_STM32_STACK_SIZE`
2. Heap size too small - increase `CONFIG_STM32_HEAP_SIZE`
3. Wrong memory addresses - verify linker script matches chip variant
4. Missing `.data` initialization - check startup code

## References

- [STM32 Datasheets](https://www.st.com/en/microcontrollers-microprocessors/stm32-32-bit-arm-cortex-mcus.html)
- [GNU LD Linker Manual](https://sourceware.org/binutils/docs/ld/)
- [ARM Cortex-M Programming Guide](https://developer.arm.com/documentation/)
- Nexus HAL Design Document: `nexus/hal/docs/DESIGN.md`
