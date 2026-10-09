# Resource Managers

This directory contains resource management implementations for the Native platform.

## Files

- `nx_dma_native.c` - DMA channel manager implementation
- `nx_isr_native.c` - Interrupt service routine manager implementation

## Purpose

The resource managers provide:
- **DMA Manager**: Simulates DMA channel allocation and management
- **ISR Manager**: Simulates interrupt registration and triggering

## DMA Manager

The DMA manager provides:
- DMA channel allocation and release
- Simulated DMA operations for testing
- Test interfaces for verification

## ISR Manager

The ISR manager provides:
- Interrupt callback registration
- Interrupt triggering simulation
- Test interfaces for verification

## Usage

These resource managers are used internally by peripheral drivers to manage shared resources.

```c
/* Allocate a DMA channel */
int channel = nx_dma_allocate();

/* Register an interrupt callback */
nx_isr_register(irq_num, callback, user_data);
```

## Implementation Notes

- Resource managers provide test interfaces for verification
- All operations are simulated for the Native platform
- Proper resource tracking and cleanup is implemented
