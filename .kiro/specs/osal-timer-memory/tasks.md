# Implementation Plan: OSAL Timer and Memory Module

## Overview

This plan implements the Timer and Memory modules for the OSAL layer, following the established patterns from existing modules (Task, Mutex, Semaphore, Queue). Implementation will be done in C, targeting the FreeRTOS adapter first, with native adapter support for testing.

## Tasks

- [x] 1. Create Timer module header and types
  - Create `osal/include/osal/osal_timer.h` with type definitions
  - Define `osal_timer_handle_t`, `osal_timer_callback_t`, `osal_timer_mode_t`
  - Define `osal_timer_config_t` structure
  - Declare all timer function prototypes
  - Add Doxygen documentation following existing OSAL style
  - _Requirements: 1.1, 1.6, 2.1-2.7, 3.1-3.4, 4.1-4.3_

- [x] 2. Create Memory module header and types
  - Create `osal/include/osal/osal_mem.h` with type definitions
  - Define `osal_mem_stats_t` structure
  - Declare all memory function prototypes
  - Add Doxygen documentation following existing OSAL style
  - _Requirements: 5.1-5.6, 6.1-6.5, 7.1-7.4_

- [x] 3. Update main OSAL header
  - Add `#include "osal_timer.h"` to `osal/include/osal/osal.h`
  - Add `#include "osal_mem.h"` to `osal/include/osal/osal.h`
  - _Requirements: 8.5_

- [x] 4. Implement Timer functions in FreeRTOS adapter
  - [x] 4.1 Implement `osal_timer_create` and `osal_timer_delete`
    - Use `xTimerCreate()` and `xTimerDelete()` FreeRTOS APIs
    - Implement timer context structure for callback argument passing
    - Handle NULL pointer and invalid parameter validation
    - _Requirements: 1.1-1.6, 2.4, 8.2, 8.3_
  - [x] 4.2 Implement `osal_timer_start`, `osal_timer_stop`, `osal_timer_reset`
    - Use `xTimerStart()`, `xTimerStop()`, `xTimerReset()` FreeRTOS APIs
    - Handle NULL pointer validation
    - _Requirements: 2.1-2.3, 2.5-2.7_
  - [x] 4.3 Implement `osal_timer_set_period` and `osal_timer_is_active`
    - Use `xTimerChangePeriod()` and `xTimerIsTimerActive()` FreeRTOS APIs
    - _Requirements: 3.3, 3.4_
  - [x] 4.4 Implement ISR variants (`osal_timer_start_from_isr`, etc.)
    - Use `xTimerStartFromISR()`, `xTimerStopFromISR()`, `xTimerResetFromISR()`
    - Handle context switch request via `portYIELD_FROM_ISR()`
    - _Requirements: 4.1-4.3_

- [x] 5. Implement Memory functions in FreeRTOS adapter
  - [x] 5.1 Implement `osal_mem_alloc` and `osal_mem_free`
    - Use `pvPortMalloc()` and `vPortFree()` FreeRTOS APIs
    - Handle zero size and NULL pointer cases
    - _Requirements: 5.1-5.6_
  - [x] 5.2 Implement `osal_mem_calloc` and `osal_mem_realloc`
    - Implement calloc using alloc + memset
    - Implement realloc with proper data preservation
    - Handle edge cases (zero size, NULL pointer)
    - _Requirements: 6.1, 6.2, 6.4, 6.5_
  - [x] 5.3 Implement `osal_mem_alloc_aligned`
    - Implement aligned allocation with padding
    - Validate alignment is power of 2
    - _Requirements: 6.3_
  - [x] 5.4 Implement memory statistics functions
    - Use `xPortGetFreeHeapSize()` and `xPortGetMinimumEverFreeHeapSize()`
    - Implement `osal_mem_get_stats()`, `osal_mem_get_free_size()`, `osal_mem_get_min_free_size()`
    - _Requirements: 7.1-7.4_

- [x] 6. Checkpoint - Verify FreeRTOS adapter compiles
  - Ensure all code compiles without errors
  - Ensure all tests pass, ask the user if questions arise

- [x] 7. Implement Timer functions in Native adapter
  - [x] 7.1 Implement timer creation and deletion for native platform
    - Use POSIX timer_create/timer_delete or custom implementation
    - _Requirements: 1.1-1.6, 2.4_
  - [x] 7.2 Implement timer control functions for native platform
    - Implement start, stop, reset using POSIX timers or threads
    - _Requirements: 2.1-2.3, 2.5-2.7, 3.3, 3.4_

- [x] 8. Implement Memory functions in Native adapter
  - [x] 8.1 Implement basic memory allocation for native platform
    - Use standard malloc/free with tracking wrapper
    - _Requirements: 5.1-5.6_
  - [x] 8.2 Implement memory variants and statistics for native platform
    - Implement calloc, realloc, aligned_alloc wrappers
    - Track allocation statistics for testing
    - _Requirements: 6.1-6.5, 7.1-7.4_

- [x] 9. Checkpoint - Verify Native adapter compiles
  - Ensure all code compiles without errors
  - Ensure all tests pass, ask the user if questions arise

- [x] 10. Write Timer unit tests
  - [x] 10.1 Write unit tests for timer creation
    - Test valid creation, NULL pointer errors, invalid parameter errors
    - _Requirements: 1.1-1.5_
  - [x] 10.2 Write unit tests for timer lifecycle
    - Test start, stop, reset, delete operations
    - Test NULL handle errors
    - _Requirements: 2.1-2.7_
  - [x] 10.3 Write unit tests for timer callback and state
    - Test callback invocation with correct argument
    - Test is_active state transitions
    - _Requirements: 3.1, 3.4_

- [x] 11. Write Timer property tests
  - [x] 11.1 Write property test for timer creation and callback
    - **Property 1: Timer Creation and Callback Invocation**
    - **Validates: Requirements 1.1, 2.1, 3.1**
  - [x] 11.2 Write property test for timer stop behavior
    - **Property 2: Timer Stop Prevents Callback**
    - **Validates: Requirements 2.2**
  - [x] 11.3 Write property test for periodic timer
    - **Property 4: Periodic Timer Auto-Restart**
    - **Validates: Requirements 2.6**
  - [x] 11.4 Write property test for one-shot timer
    - **Property 5: One-Shot Timer Single Execution**
    - **Validates: Requirements 2.7**
  - [x] 11.5 Write property test for timer active state
    - **Property 6: Timer Active State Consistency**
    - **Validates: Requirements 3.4**

- [x] 12. Write Memory unit tests
  - [x] 12.1 Write unit tests for basic allocation
    - Test valid allocation, zero size, NULL free
    - _Requirements: 5.1-5.5_
  - [x] 12.2 Write unit tests for allocation variants
    - Test calloc zero-initialization, realloc behavior
    - Test aligned allocation
    - _Requirements: 6.1-6.5_
  - [x] 12.3 Write unit tests for memory statistics
    - Test stats retrieval, NULL pointer error
    - _Requirements: 7.1-7.4_

- [x] 13. Write Memory property tests
  - [x] 13.1 Write property test for allocation round-trip
    - **Property 8: Memory Allocation Round-Trip**
    - **Validates: Requirements 5.1, 5.4**
  - [x] 13.2 Write property test for calloc zero-initialization
    - **Property 9: Calloc Zero-Initialization**
    - **Validates: Requirements 6.1**
  - [x] 13.3 Write property test for realloc data preservation
    - **Property 10: Realloc Data Preservation**
    - **Validates: Requirements 6.2**
  - [x] 13.4 Write property test for aligned allocation
    - **Property 11: Aligned Allocation Alignment**
    - **Validates: Requirements 6.3**
  - [x] 13.5 Write property test for memory statistics consistency
    - **Property 12: Memory Statistics Consistency**
    - **Validates: Requirements 7.2, 7.3**

- [x] 14. Write error handling property tests
  - [x] 14.1 Write property test for NULL pointer error handling
    - **Property 13: NULL Pointer Error Handling**
    - **Validates: Requirements 8.2**
  - [x] 14.2 Write property test for invalid parameter error handling
    - **Property 14: Invalid Parameter Error Handling**
    - **Validates: Requirements 8.3**

- [x] 15. Update CMakeLists.txt
  - Add new header files to OSAL include list
  - Add new source files to adapter builds
  - Add new test files to test build
  - _Requirements: 8.5_

- [x] 16. Final checkpoint - Ensure all tests pass
  - Run full test suite
  - Ensure all tests pass, ask the user if questions arise

## Notes

- All tasks are required for comprehensive test coverage
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation
- Property tests validate universal correctness properties
- Unit tests validate specific examples and edge cases
- Implementation follows existing OSAL patterns from Task, Mutex, Semaphore, Queue modules
