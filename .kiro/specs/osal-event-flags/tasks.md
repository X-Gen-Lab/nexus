# Implementation Plan: OSAL Event Flags Module

## Overview

This plan implements the Event Flags module for the OSAL layer, following the established patterns from existing modules (Task, Mutex, Semaphore, Queue). Implementation will be done in C, targeting the FreeRTOS adapter first, with native adapter support for testing.

## Tasks

- [x] 1. Create Event Flags module header and types
  - Create `osal/include/osal/osal_event.h` with type definitions
  - Define `osal_event_handle_t`, `osal_event_bits_t`
  - Define `osal_event_wait_mode_t` enumeration
  - Define `osal_event_wait_options_t` structure
  - Declare all event flags function prototypes
  - Add Doxygen documentation following existing OSAL style
  - _Requirements: 1.1-1.6, 2.1-2.5, 3.1-3.5, 4.1-4.9, 5.1-5.4, 6.1-6.3_

- [x] 2. Update main OSAL header
  - Add `#include "osal_event.h"` to `osal/include/osal/osal.h`
  - _Requirements: 8.7_

- [x] 3. Implement Event Flags functions in FreeRTOS adapter
  - [x] 3.1 Implement `osal_event_create` and `osal_event_delete`
    - Use `xEventGroupCreate()` and `vEventGroupDelete()` FreeRTOS APIs
    - Handle NULL pointer validation
    - Define OSAL_EVENT_BITS_MASK for 24-bit support
    - _Requirements: 1.1-1.6, 8.2, 8.4_
  - [x] 3.2 Implement `osal_event_set` and `osal_event_clear`
    - Use `xEventGroupSetBits()` and `xEventGroupClearBits()` FreeRTOS APIs
    - Handle NULL pointer and zero mask validation
    - Ensure atomic operations
    - _Requirements: 2.1-2.5, 3.1-3.5, 8.2, 8.3_
  - [x] 3.3 Implement `osal_event_wait`
    - Use `xEventGroupWaitBits()` FreeRTOS API
    - Implement WAIT_ALL and WAIT_ANY mode mapping
    - Implement auto-clear option
    - Handle timeout conversion using existing helper
    - Handle NULL pointer and zero mask validation
    - Detect ISR context and return error
    - _Requirements: 4.1-4.9, 8.2, 8.3, 8.5, 8.6_
  - [x] 3.4 Implement `osal_event_get`
    - Use `xEventGroupGetBits()` FreeRTOS API
    - Handle NULL handle gracefully (return 0)
    - _Requirements: 5.1-5.4_
  - [x] 3.5 Implement `osal_event_set_from_isr`
    - Use `xEventGroupSetBitsFromISR()` FreeRTOS API
    - Handle context switch request via `portYIELD_FROM_ISR()`
    - Handle NULL pointer and zero mask validation
    - _Requirements: 6.1-6.2, 8.2, 8.3_

- [x] 4. Checkpoint - Verify FreeRTOS adapter compiles
  - Ensure all code compiles without errors
  - Ensure all tests pass, ask the user if questions arise

- [x] 5. Implement Event Flags functions in Native adapter
  - [x] 5.1 Implement event flags creation and deletion for native platform
    - Create native event structure with mutex and condition variable
    - Initialize bits to 0
    - _Requirements: 1.1-1.6_
  - [x] 5.2 Implement set and clear for native platform
    - Lock mutex, modify bits, signal condition variable, unlock mutex
    - _Requirements: 2.1-2.5, 3.1-3.5_
  - [x] 5.3 Implement wait for native platform
    - Lock mutex, check condition in loop, wait on condition variable
    - Implement WAIT_ALL and WAIT_ANY logic
    - Implement auto-clear logic
    - Handle timeout using pthread_cond_timedwait
    - _Requirements: 4.1-4.9_
  - [x] 5.4 Implement get for native platform
    - Lock mutex, read bits, unlock mutex
    - _Requirements: 5.1-5.4_

- [x] 6. Checkpoint - Verify Native adapter compiles
  - Ensure all code compiles without errors
  - Ensure all tests pass, ask the user if questions arise

- [x] 7. Write Event Flags unit tests
  - [x] 7.1 Write unit tests for event flags creation and deletion
    - Test valid creation, NULL pointer errors
    - Test deletion with valid and NULL handles
    - _Requirements: 1.1, 1.2, 1.5_
  - [x] 7.2 Write unit tests for setting and clearing bits
    - Test valid set/clear operations
    - Test NULL handle errors
    - Test zero mask errors
    - Test that clear doesn't affect other bits
    - _Requirements: 2.1-2.3, 3.1-3.3, 3.5_
  - [x] 7.3 Write unit tests for wait operations
    - Test WAIT_ALL mode with various bit patterns
    - Test WAIT_ANY mode with various bit patterns
    - Test auto-clear enabled and disabled
    - Test timeout behavior
    - Test immediate return when condition met
    - Test NULL handle and zero mask errors
    - _Requirements: 4.1-4.9_
  - [x] 7.4 Write unit tests for get operation
    - Test get returns correct value
    - Test get with NULL handle returns 0
    - Test get doesn't modify bits
    - _Requirements: 5.1-5.3_
  - [x] 7.5 Write unit tests for ISR operations
    - Test set_from_isr with valid parameters
    - Test error handling for NULL handle and zero mask
    - _Requirements: 6.1, 8.2, 8.3_
  - [x] 7.6 Write unit test for 24-bit support
    - Test that at least 24 bits can be set and retrieved
    - _Requirements: 1.6_

- [x] 8. Write Event Flags property tests
  - [x] 8.1 Write property test for event flags creation
    - **Property 1: Event Flags Creation Success**
    - **Validates: Requirements 1.1**
  - [x] 8.2 Write property test for set bits atomic update
    - **Property 2: Set Bits Atomically Updates State**
    - **Validates: Requirements 2.1, 2.5**
  - [x] 8.3 Write property test for set bits wakes waiting tasks
    - **Property 3: Set Bits Wakes Waiting Tasks**
    - **Validates: Requirements 2.4**
  - [x] 8.4 Write property test for clear bits atomic update
    - **Property 4: Clear Bits Atomically Updates Only Specified Bits**
    - **Validates: Requirements 3.1, 3.4, 3.5**
  - [x] 8.5 Write property test for WAIT_ALL mode
    - **Property 5: Wait All Mode Requires All Bits**
    - **Validates: Requirements 4.4**
  - [x] 8.6 Write property test for WAIT_ANY mode
    - **Property 6: Wait Any Mode Requires Any Bit**
    - **Validates: Requirements 4.5**
  - [x] 8.7 Write property test for auto-clear behavior
    - **Property 7: Auto-Clear Clears Matched Bits**
    - **Validates: Requirements 4.6**
  - [x] 8.8 Write property test for non-auto-clear behavior
    - **Property 8: Non-Auto-Clear Preserves Bits**
    - **Validates: Requirements 4.7**
  - [x] 8.9 Write property test for wait timeout
    - **Property 9: Wait Timeout Returns Error**
    - **Validates: Requirements 4.8**
  - [x] 8.10 Write property test for wait immediate return
    - **Property 10: Wait Immediate Return When Satisfied**
    - **Validates: Requirements 4.9**
  - [x] 8.11 Write property test for get operation
    - **Property 11: Get Returns Current Value Without Modification**
    - **Validates: Requirements 5.1, 5.3**
  - [x] 8.12 Write property test for set operation atomicity
    - **Property 12: Set Operation Atomicity**
    - **Validates: Requirements 7.1**
  - [x] 8.13 Write property test for clear operation atomicity
    - **Property 13: Clear Operation Atomicity**
    - **Validates: Requirements 7.2**
  - [x] 8.14 Write property test for wait check-and-clear atomicity
    - **Property 14: Wait Check-and-Clear Atomicity**
    - **Validates: Requirements 7.3**
  - [x] 8.15 Write property test for broadcast wake
    - **Property 15: Broadcast Wake All Waiting Tasks**
    - **Validates: Requirements 7.4**

- [x] 9. Write error handling property tests
  - [x] 9.1 Write property test for NULL pointer error handling
    - **Property 16: NULL Pointer Error Handling**
    - **Validates: Requirements 8.2**
  - [x] 9.2 Write property test for invalid parameter error handling
    - **Property 17: Invalid Parameter Error Handling**
    - **Validates: Requirements 8.3**

- [x] 10. Write concurrency tests
  - [x] 10.1 Write test for concurrent set operations
    - Test multiple threads setting different bits simultaneously
    - Verify all bits are set correctly
    - _Requirements: 7.1_
  - [x] 10.2 Write test for concurrent clear operations
    - Test multiple threads clearing different bits simultaneously
    - Verify only specified bits are cleared
    - _Requirements: 7.2_
  - [x] 10.3 Write test for concurrent wait operations
    - Test multiple threads waiting for overlapping bit patterns
    - Verify all waiting threads wake when condition is met
    - _Requirements: 7.4_
  - [x] 10.4 Write test for set/clear/wait race conditions
    - Test set and clear racing with wait operations
    - Verify no race conditions or lost wakeups
    - _Requirements: 7.3_

- [x] 11. Update CMakeLists.txt
  - Add new header file to OSAL include list
  - Add new test files to test build
  - _Requirements: 8.7_

- [x] 12. Final checkpoint - Ensure all tests pass
  - Run full test suite
  - Ensure all tests pass, ask the user if questions arise

## Notes

- All tasks are required for comprehensive test coverage
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation
- Property tests validate universal correctness properties
- Unit tests validate specific examples and edge cases
- Concurrency tests validate atomicity guarantees
- Implementation follows existing OSAL patterns from Task, Mutex, Semaphore, Queue modules
