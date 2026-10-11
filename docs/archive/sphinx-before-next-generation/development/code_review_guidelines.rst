Code Review Guidelines
======================

This guide provides comprehensive code review guidelines for the Nexus Embedded Platform, covering review process, checklist, best practices, and common issues.

.. contents:: Table of Contents
   :local:
   :depth: 3

Overview
--------

Code review is a critical part of the development process that ensures:

* **Code Quality**: Maintain high code quality standards
* **Knowledge Sharing**: Spread knowledge across the team
* **Bug Prevention**: Catch bugs before they reach production
* **Consistency**: Ensure consistent code style and patterns
* **Learning**: Help developers improve their skills

Code review is required for all changes to the Nexus codebase.

Review Process
--------------

Pull Request Workflow
~~~~~~~~~~~~~~~~~~~~~

**1. Author Prepares PR**

Before submitting a pull request:

* Ensure all tests pass locally
* Run code formatting tools
* Update documentation
* Write clear commit messages
* Add tests for new functionality
* Verify CI checks pass

**2. Submit Pull Request**

Create a pull request with:

* **Clear Title**: Descriptive title following commit conventions
* **Description**: Detailed description of changes
* **Context**: Why the change is needed
* **Testing**: How the change was tested
* **Screenshots**: For UI changes (if applicable)
* **Related Issues**: Link to related issues

**Example PR Template**:

.. code-block:: markdown

   ## Description
   Add support for SPI DMA transfers to improve performance.

   ## Motivation
   Current SPI implementation uses polling which blocks the CPU.
   DMA transfers allow CPU to perform other tasks during SPI operations.

   ## Changes
   - Add `hal_spi_transfer_dma()` API
   - Implement DMA support for STM32 platform
   - Add unit tests for DMA transfers
   - Update documentation

   ## Testing
   - All existing tests pass
   - Added 15 new tests for DMA functionality
   - Tested on STM32F4 hardware
   - Verified performance improvement (10x faster for large transfers)

   ## Related Issues
   Closes #123

**3. Automated Checks**

CI system automatically runs:

* Build verification
* Unit tests
* Integration tests
* Code coverage analysis
* Static analysis
* Code formatting check
* Documentation build

**4. Code Review**

Reviewers examine:

* Code correctness
* Design and architecture
* Test coverage
* Documentation
* Performance implications
* Security considerations

**5. Address Feedback**

Author responds to review comments:

* Make requested changes
* Explain design decisions
* Ask clarifying questions
* Update PR based on feedback

**6. Approval and Merge**

After approval:

* Squash commits if needed
* Ensure CI passes
* Merge to main branch
* Delete feature branch



Review Roles
~~~~~~~~~~~~

**Author Responsibilities**

* Write clear, maintainable code
* Add comprehensive tests
* Update documentation
* Respond to review comments promptly
* Be open to feedback
* Explain design decisions

**Reviewer Responsibilities**

* Review code thoroughly
* Provide constructive feedback
* Ask clarifying questions
* Suggest improvements
* Approve when ready
* Be respectful and helpful

**Maintainer Responsibilities**

* Final approval authority
* Ensure quality standards
* Merge approved PRs
* Resolve conflicts
* Guide contributors

Review Checklist
----------------

General Checklist
~~~~~~~~~~~~~~~~~

**Code Correctness**

☐ Code implements the intended functionality
☐ Logic is correct and handles edge cases
☐ No obvious bugs or errors
☐ Error handling is appropriate
☐ Resource cleanup is proper
☐ No memory leaks or resource leaks

**Code Quality**

☐ Code follows coding standards
☐ Naming is clear and consistent
☐ Functions are focused and single-purpose
☐ Code is readable and maintainable
☐ No code duplication
☐ Appropriate use of abstractions

**Testing**

☐ Tests are included for new functionality
☐ Tests cover edge cases and error conditions
☐ All tests pass
☐ Code coverage is adequate (>90%)
☐ Tests are clear and maintainable
☐ Property-based tests for HAL (if applicable)

**Documentation**

☐ Public APIs are documented
☐ Doxygen comments are complete
☐ User documentation is updated
☐ Examples are provided (if needed)
☐ README is updated (if needed)
☐ CHANGELOG is updated

**Performance**

☐ No obvious performance issues
☐ Algorithms are efficient
☐ Memory usage is reasonable
☐ No unnecessary allocations
☐ Critical paths are optimized

**Security**

☐ Input validation is proper
☐ No buffer overflows
☐ No integer overflows
☐ Sensitive data is protected
☐ No hardcoded credentials
☐ Secure coding practices followed

**Portability**

☐ Code is platform-independent (where applicable)
☐ Platform-specific code is isolated
☐ No assumptions about endianness
☐ Fixed-width types used appropriately
☐ Alignment requirements considered

**Compatibility**

☐ API changes are backward compatible
☐ Breaking changes are documented
☐ Deprecation warnings added (if needed)
☐ Migration guide provided (if needed)

HAL-Specific Checklist
~~~~~~~~~~~~~~~~~~~~~~

**API Design**

☐ API follows HAL conventions
☐ Function naming is consistent
☐ Parameter order is standard
☐ Return values follow conventions
☐ Error codes are appropriate

**Implementation**

☐ Platform abstraction is proper
☐ Hardware access is safe
☐ Interrupt handling is correct
☐ DMA usage is safe
☐ Clock configuration is correct

**Testing**

☐ Unit tests for all functions
☐ Property-based tests included
☐ Hardware tests documented
☐ Edge cases covered
☐ Error conditions tested

**Documentation**

☐ Hardware requirements documented
☐ Pin configurations documented
☐ Timing requirements documented
☐ Limitations documented
☐ Examples provided

OSAL-Specific Checklist
~~~~~~~~~~~~~~~~~~~~~~~

**API Design**

☐ API is RTOS-agnostic
☐ Consistent with OSAL conventions
☐ Thread-safe where required
☐ Timeout handling is consistent

**Implementation**

☐ RTOS abstraction is proper
☐ Resource management is correct
☐ Priority handling is appropriate
☐ Synchronization is correct

**Testing**

☐ Multi-threaded tests included
☐ Synchronization tested
☐ Timeout behavior tested
☐ Resource cleanup tested

Framework-Specific Checklist
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Logging**

☐ Log levels are appropriate
☐ Log messages are clear
☐ No excessive logging
☐ Sensitive data not logged

**Configuration**

☐ Kconfig options added
☐ Default values are sensible
☐ Dependencies are correct
☐ Help text is clear

**Shell Commands**

☐ Command syntax is clear
☐ Help text is provided
☐ Error messages are helpful
☐ Input validation is proper

Review Best Practices
---------------------

For Reviewers
~~~~~~~~~~~~~

**Be Constructive**

* Focus on the code, not the person
* Explain why changes are needed
* Suggest specific improvements
* Acknowledge good work
* Be respectful and professional

**Example - Good Feedback**:

.. code-block:: text

   This function could be more efficient by using a hash table instead
   of linear search. For large datasets, this would reduce complexity
   from O(n) to O(1). Consider using `hash_table_t` from utils.h.

**Example - Poor Feedback**:

.. code-block:: text

   This is slow and inefficient.

**Ask Questions**

* Ask for clarification when needed
* Understand the context
* Learn from the author
* Discuss trade-offs

**Example Questions**:

.. code-block:: text

   - Why did you choose approach A over approach B?
   - Have you considered the case where X happens?
   - How does this handle error condition Y?
   - What's the expected performance for large inputs?

**Prioritize Issues**

* Distinguish between critical and minor issues
* Focus on important problems first
* Don't nitpick on style (use automated tools)
* Be pragmatic

**Issue Priority Levels**:

* **Critical**: Must fix (bugs, security issues, breaking changes)
* **Important**: Should fix (design issues, missing tests)
* **Minor**: Nice to have (style improvements, optimizations)
* **Nit**: Optional (personal preferences)

**Review Thoroughly**

* Read the entire change
* Understand the context
* Check related code
* Verify tests
* Test locally if needed

**Be Timely**

* Review within 24 hours
* Provide initial feedback quickly
* Don't block progress unnecessarily
* Communicate delays

For Authors
~~~~~~~~~~~

**Prepare Well**

* Self-review before submitting
* Run all checks locally
* Write clear descriptions
* Provide context
* Anticipate questions

**Self-Review Checklist**:

.. code-block:: text

   ☐ Read through all changes
   ☐ Remove debug code
   ☐ Check for typos
   ☐ Verify tests pass
   ☐ Run code formatter
   ☐ Update documentation

**Respond Constructively**

* Thank reviewers for feedback
* Address all comments
* Explain decisions clearly
* Ask for clarification
* Be open to suggestions

**Example Responses**:

.. code-block:: text

   Good: "Thanks for catching that! I've updated the code to use
   hash tables as you suggested. Performance improved by 10x."

   Good: "I considered that approach, but chose this one because
   it's simpler and performance isn't critical here. What do you think?"

   Poor: "I disagree." (without explanation)

**Keep Changes Focused**

* One logical change per PR
* Avoid mixing refactoring with features
* Split large changes into smaller PRs
* Keep commits atomic

**Communicate Clearly**

* Write clear commit messages
* Explain complex changes
* Document trade-offs
* Update PR description as needed



Common Review Issues
--------------------

Code Quality Issues
~~~~~~~~~~~~~~~~~~~

**Issue: Unclear Variable Names**

.. code-block:: c

   /* Bad */
   int x = get_data();
   int y = process(x);

   /* Good */
   int sensor_reading = get_sensor_data();
   int filtered_value = apply_filter(sensor_reading);

**Issue: Long Functions**

.. code-block:: c

   /* Bad: 200-line function doing everything */
   hal_status_t hal_uart_init(hal_uart_id_t id, const hal_uart_config_t* config) {
       /* 200 lines of code */
   }

   /* Good: Split into focused functions */
   hal_status_t hal_uart_init(hal_uart_id_t id, const hal_uart_config_t* config) {
       hal_status_t status;

       status = uart_validate_config(config);
       if (status != HAL_OK) return status;

       status = uart_configure_hardware(id, config);
       if (status != HAL_OK) return status;

       status = uart_enable_interrupts(id);
       if (status != HAL_OK) return status;

       return HAL_OK;
   }

**Issue: Code Duplication**

.. code-block:: c

   /* Bad: Duplicated code */
   void process_sensor_a(void) {
       int value = read_sensor_a();
       value = apply_filter(value);
       value = apply_calibration(value);
       store_value(value);
   }

   void process_sensor_b(void) {
       int value = read_sensor_b();
       value = apply_filter(value);
       value = apply_calibration(value);
       store_value(value);
   }

   /* Good: Extract common logic */
   static void process_sensor_value(int (*read_fn)(void)) {
       int value = read_fn();
       value = apply_filter(value);
       value = apply_calibration(value);
       store_value(value);
   }

   void process_sensor_a(void) {
       process_sensor_value(read_sensor_a);
   }

   void process_sensor_b(void) {
       process_sensor_value(read_sensor_b);
   }

Error Handling Issues
~~~~~~~~~~~~~~~~~~~~~

**Issue: Ignoring Return Values**

.. code-block:: c

   /* Bad: Ignoring return value */
   hal_gpio_init(HAL_GPIO_PORT_A, 5, &config);
   hal_gpio_write(HAL_GPIO_PORT_A, 5, HAL_GPIO_LEVEL_HIGH);

   /* Good: Check return values */
   hal_status_t status = hal_gpio_init(HAL_GPIO_PORT_A, 5, &config);
   if (status != HAL_OK) {
       LOG_ERROR("GPIO init failed: %d", status);
       return status;
   }

   status = hal_gpio_write(HAL_GPIO_PORT_A, 5, HAL_GPIO_LEVEL_HIGH);
   if (status != HAL_OK) {
       LOG_ERROR("GPIO write failed: %d", status);
       return status;
   }

**Issue: Missing Parameter Validation**

.. code-block:: c

   /* Bad: No validation */
   hal_status_t hal_gpio_write(hal_gpio_port_t port, uint8_t pin,
                               hal_gpio_level_t level) {
       GPIO_PORTS[port]->ODR = (level << pin);
       return HAL_OK;
   }

   /* Good: Validate parameters */
   hal_status_t hal_gpio_write(hal_gpio_port_t port, uint8_t pin,
                               hal_gpio_level_t level) {
       if (port >= HAL_GPIO_PORT_MAX) {
           return HAL_ERROR_PARAM;
       }
       if (pin >= HAL_GPIO_PIN_MAX) {
           return HAL_ERROR_PARAM;
       }

       GPIO_PORTS[port]->ODR = (level << pin);
       return HAL_OK;
   }

**Issue: Resource Leaks**

.. code-block:: c

   /* Bad: Memory leak on error */
   hal_status_t init_system(void) {
       uint8_t* buffer = malloc(1024);

       if (init_hardware() != HAL_OK) {
           return HAL_ERROR;  /* Leak! */
       }

       /* Use buffer */
       free(buffer);
       return HAL_OK;
   }

   /* Good: Cleanup on error */
   hal_status_t init_system(void) {
       uint8_t* buffer = malloc(1024);
       if (buffer == NULL) {
           return HAL_ERROR_NO_MEMORY;
       }

       if (init_hardware() != HAL_OK) {
           free(buffer);  /* Cleanup */
           return HAL_ERROR;
       }

       /* Use buffer */
       free(buffer);
       return HAL_OK;
   }

Testing Issues
~~~~~~~~~~~~~~

**Issue: Missing Edge Case Tests**

.. code-block:: c

   /* Bad: Only tests normal case */
   void test_divide(void) {
       assert(divide(10, 2) == 5);
   }

   /* Good: Tests edge cases */
   void test_divide_normal(void) {
       assert(divide(10, 2) == 5);
   }

   void test_divide_by_zero(void) {
       assert(divide(10, 0) == ERROR_DIVIDE_BY_ZERO);
   }

   void test_divide_negative(void) {
       assert(divide(-10, 2) == -5);
   }

   void test_divide_overflow(void) {
       assert(divide(INT_MIN, -1) == ERROR_OVERFLOW);
   }

**Issue: Tests Not Independent**

.. code-block:: c

   /* Bad: Tests depend on each other */
   static int global_state = 0;

   void test_increment(void) {
       increment();
       assert(global_state == 1);  /* Depends on initial state */
   }

   void test_decrement(void) {
       decrement();
       assert(global_state == 0);  /* Depends on previous test */
   }

   /* Good: Independent tests */
   void test_increment(void) {
       int state = 0;
       state = increment(state);
       assert(state == 1);
   }

   void test_decrement(void) {
       int state = 1;
       state = decrement(state);
       assert(state == 0);
   }

Documentation Issues
~~~~~~~~~~~~~~~~~~~~

**Issue: Missing Documentation**

.. code-block:: c

   /* Bad: No documentation */
   hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin,
                             const hal_gpio_config_t* config);

   /* Good: Complete documentation */
   /**
    * \brief           Initialize GPIO pin
    * \param[in]       port: GPIO port (HAL_GPIO_PORT_A to HAL_GPIO_PORT_K)
    * \param[in]       pin: GPIO pin number (0-15)
    * \param[in]       config: Pointer to configuration structure
    * \return          HAL_OK on success, error code otherwise
    * \retval          HAL_OK: Success
    * \retval          HAL_ERROR_PARAM: Invalid parameter
    * \retval          HAL_ERROR_STATE: GPIO already initialized
    * \note            Pin must be deinitialized before re-initialization
    * \warning         This function is NOT thread-safe
    * \see             hal_gpio_deinit
    */
   hal_status_t hal_gpio_init(hal_gpio_port_t port, uint8_t pin,
                             const hal_gpio_config_t* config);

**Issue: Outdated Documentation**

.. code-block:: c

   /* Bad: Documentation doesn't match implementation */
   /**
    * \brief           Send data via UART
    * \param[in]       data: Data buffer
    * \param[in]       length: Data length
    * \return          Number of bytes sent
    */
   hal_status_t hal_uart_send(hal_uart_id_t id, const uint8_t* data,
                             size_t length, uint32_t timeout);

   /* Good: Documentation matches implementation */
   /**
    * \brief           Send data via UART
    * \param[in]       id: UART instance ID
    * \param[in]       data: Data buffer
    * \param[in]       length: Data length
    * \param[in]       timeout: Timeout in milliseconds
    * \return          HAL_OK on success, error code otherwise
    */
   hal_status_t hal_uart_send(hal_uart_id_t id, const uint8_t* data,
                             size_t length, uint32_t timeout);

Performance Issues
~~~~~~~~~~~~~~~~~~

**Issue: Inefficient Algorithms**

.. code-block:: c

   /* Bad: O(n²) algorithm */
   bool has_duplicates(int* array, size_t length) {
       for (size_t i = 0; i < length; i++) {
           for (size_t j = i + 1; j < length; j++) {
               if (array[i] == array[j]) {
                   return true;
               }
           }
       }
       return false;
   }

   /* Good: O(n) algorithm using hash set */
   bool has_duplicates(int* array, size_t length) {
       hash_set_t* seen = hash_set_create();

       for (size_t i = 0; i < length; i++) {
           if (hash_set_contains(seen, array[i])) {
               hash_set_destroy(seen);
               return true;
           }
           hash_set_add(seen, array[i]);
       }

       hash_set_destroy(seen);
       return false;
   }

**Issue: Unnecessary Allocations**

.. code-block:: c

   /* Bad: Allocates on every call */
   void process_data(const uint8_t* input, size_t length) {
       uint8_t* buffer = malloc(length);
       memcpy(buffer, input, length);
       /* Process buffer */
       free(buffer);
   }

   /* Good: Use stack or caller-provided buffer */
   void process_data(const uint8_t* input, size_t length) {
       uint8_t buffer[256];  /* Stack allocation */
       if (length > sizeof(buffer)) {
           return;  /* Or use dynamic allocation only when needed */
       }
       memcpy(buffer, input, length);
       /* Process buffer */
   }

Security Issues
~~~~~~~~~~~~~~~

**Issue: Buffer Overflow**

.. code-block:: c

   /* Bad: No bounds checking */
   void copy_string(char* dest, const char* src) {
       strcpy(dest, src);  /* Buffer overflow risk! */
   }

   /* Good: Bounds checking */
   void copy_string(char* dest, size_t dest_size, const char* src) {
       if (dest == NULL || src == NULL || dest_size == 0) {
           return;
       }
       strncpy(dest, src, dest_size - 1);
       dest[dest_size - 1] = '\0';
   }

**Issue: Integer Overflow**

.. code-block:: c

   /* Bad: Integer overflow */
   size_t allocate_buffer(size_t count, size_t size) {
       return malloc(count * size);  /* Overflow risk! */
   }

   /* Good: Check for overflow */
   void* allocate_buffer(size_t count, size_t size) {
       if (count > 0 && size > SIZE_MAX / count) {
           return NULL;  /* Would overflow */
       }
       return malloc(count * size);
   }

Review Tools
------------

Automated Tools
~~~~~~~~~~~~~~~

**Code Formatting**

.. code-block:: bash

   # Format code
   python scripts/tools/format.py

   # Check formatting
   python scripts/tools/format.py --check

**Static Analysis**

.. code-block:: bash

   # Run clang-tidy
   python scripts/tools/static_analysis.py

   # Run cppcheck
   cppcheck --enable=all --inconclusive src/

**Code Coverage**

.. code-block:: bash

   # Generate coverage report
   python scripts/test/test.py --coverage

   # View coverage report
   open build/coverage/index.html

**Testing**

.. code-block:: bash

   # Run all tests
   python scripts/test/test.py

   # Run specific test suite
   python scripts/test/test.py --suite hal

Manual Review Tools
~~~~~~~~~~~~~~~~~~~

**GitHub Features**

* Line comments
* File comments
* Suggested changes
* Review summary
* Request changes
* Approve

**Local Testing**

.. code-block:: bash

   # Checkout PR branch
   gh pr checkout 123

   # Build and test
   python scripts/building/build.py
   python scripts/test/test.py

   # Test on hardware (if applicable)
   python scripts/building/build.py --platform stm32f4
   python scripts/building/flash.py

Review Metrics
--------------

Quality Metrics
~~~~~~~~~~~~~~~

* **Review Coverage**: Percentage of code reviewed
* **Review Time**: Time from PR creation to approval
* **Iteration Count**: Number of review cycles
* **Defect Density**: Bugs found per 1000 lines
* **Test Coverage**: Percentage of code covered by tests

Target Metrics
~~~~~~~~~~~~~~

* Review within 24 hours
* Maximum 3 review iterations
* Minimum 90% test coverage
* Zero critical security issues
* All automated checks pass



Review Examples
---------------

Example 1: Good Review
~~~~~~~~~~~~~~~~~~~~~~

**PR**: Add SPI DMA support

**Reviewer Comment**:

.. code-block:: text

   Great work on adding DMA support! The implementation looks solid.
   I have a few suggestions:

   **Critical**:
   - Line 45: Missing null pointer check for `callback` parameter.
     This could cause a crash if user passes NULL.

   **Important**:
   - Line 78: Consider adding a timeout for DMA completion to prevent
     infinite wait if DMA fails.
   - Missing tests for error conditions (DMA failure, timeout).

   **Minor**:
   - Line 92: Could use `hal_cache_clean()` helper instead of direct
     cache operations for better portability.

   **Nit**:
   - Line 105: Typo in comment: "tranfer" -> "transfer"

   Overall this is a valuable addition. Once the critical and important
   issues are addressed, I'll approve.

**Author Response**:

.. code-block:: text

   Thanks for the thorough review!

   - Fixed null pointer check (commit abc123)
   - Added timeout with default 5 seconds (commit def456)
   - Added 8 new tests for error conditions (commit ghi789)
   - Updated to use hal_cache_clean() (commit jkl012)
   - Fixed typo (commit mno345)

   All tests pass and coverage is now 95%.

Example 2: Constructive Feedback
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**PR**: Refactor GPIO driver

**Reviewer Comment**:

.. code-block:: text

   I see you've refactored the GPIO driver to reduce code duplication.
   The intent is good, but I have concerns about the approach:

   **Design Question**:
   The new `gpio_configure_pin()` function combines initialization and
   configuration. This breaks the init/deinit pattern used throughout
   the HAL. Have you considered keeping them separate?

   **Alternative Approach**:
   Instead of combining functions, what if we extract the common logic
   into internal helper functions? This would reduce duplication while
   maintaining the existing API. For example:

   ```c
   static hal_status_t gpio_validate_params(...);
   static hal_status_t gpio_configure_hardware(...);

   hal_status_t hal_gpio_init(...) {
       /* Use helpers */
   }
   ```

   What do you think?

**Author Response**:

.. code-block:: text

   Good point about breaking the pattern. I was focused on reducing
   duplication and didn't consider API consistency.

   Your suggestion makes sense. I've updated the PR to use internal
   helpers while keeping the public API unchanged. This reduces
   duplication by 60% without breaking the pattern.

   Thanks for the guidance!

Example 3: Security Review
~~~~~~~~~~~~~~~~~~~~~~~~~~~

**PR**: Add network packet parser

**Reviewer Comment**:

.. code-block:: text

   **SECURITY CONCERNS**:

   1. Line 34: Buffer overflow vulnerability
      ```c
      memcpy(dest, packet->data, packet->length);
      ```
      No validation that `packet->length` fits in `dest` buffer.
      This could allow remote code execution.

   2. Line 56: Integer overflow
      ```c
      size_t total = header_size + payload_size;
      ```
      If `header_size + payload_size` overflows, could allocate
      small buffer and cause heap overflow.

   3. Line 78: Unchecked return value
      ```c
      parse_header(packet);
      ```
      If parsing fails, continues with invalid data.

   **Required Changes**:
   - Add bounds checking for all buffer operations
   - Check for integer overflow before allocation
   - Validate all return values
   - Add fuzzing tests for malformed packets

   Please address these security issues before this can be merged.

See Also
--------

Related Documentation
~~~~~~~~~~~~~~~~~~~~~

* :doc:`contributing` - Contribution guidelines
* :doc:`coding_standards` - Code style guidelines
* :doc:`api_design_guidelines` - API design principles
* :doc:`testing` - Testing guidelines

External Resources
~~~~~~~~~~~~~~~~~~

* `Code Review Best Practices <https://google.github.io/eng-practices/review/>`_
* `How to Do Code Reviews <https://www.kevinlondon.com/2015/05/05/code-review-best-practices.html>`_
* `Effective Code Reviews <https://www.atlassian.com/agile/software-development/code-reviews>`_

Summary
-------

Effective code review is essential for maintaining code quality in the Nexus project:

* **Process**: Clear workflow from PR submission to merge
* **Checklist**: Comprehensive review checklist covering all aspects
* **Best Practices**: Constructive feedback, timely reviews, clear communication
* **Common Issues**: Code quality, error handling, testing, documentation, performance, security
* **Tools**: Automated and manual review tools
* **Metrics**: Track review quality and efficiency
* **Examples**: Real-world review scenarios

Following these guidelines ensures consistent, high-quality code reviews that improve code quality, share knowledge, and help developers grow.
