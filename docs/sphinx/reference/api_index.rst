API Functions Index
===================

This page provides a comprehensive index of all API functions available in the Nexus Embedded Platform, organized by module.

.. contents:: Table of Contents
   :local:
   :depth: 2

HAL API
-------

Core HAL Functions
^^^^^^^^^^^^^^^^^^

.. index::
   single: nx_hal_init
   single: nx_hal_deinit
   single: nx_hal_get_version

* ``nx_hal_init()`` - Initialize HAL subsystem
* ``nx_hal_deinit()`` - Deinitialize HAL subsystem
* ``nx_hal_get_version()`` - Get HAL version

See :doc:`../api/hal` for complete HAL API documentation.

Factory Functions
^^^^^^^^^^^^^^^^^

GPIO Factory
""""""""""""

.. index::
   single: nx_factory_gpio
   single: nx_factory_gpio_with_config
   single: nx_factory_gpio_release

* ``nx_factory_gpio(port, pin)`` - Create GPIO device
* ``nx_factory_gpio_with_config(port, pin, config)`` - Create GPIO with configuration
* ``nx_factory_gpio_release(device)`` - Release GPIO device

UART Factory
""""""""""""

.. index::
   single: nx_factory_uart
   single: nx_factory_uart_with_config
   single: nx_factory_uart_release

* ``nx_factory_uart(instance)`` - Create UART device
* ``nx_factory_uart_with_config(instance, config)`` - Create UART with configuration
* ``nx_factory_uart_release(device)`` - Release UART device

SPI Factory
"""""""""""

.. index::
   single: nx_factory_spi
   single: nx_factory_spi_with_config
   single: nx_factory_spi_release

* ``nx_factory_spi(instance)`` - Create SPI device
* ``nx_factory_spi_with_config(instance, config)`` - Create SPI with configuration
* ``nx_factory_spi_release(device)`` - Release SPI device

I2C Factory
"""""""""""

.. index::
   single: nx_factory_i2c
   single: nx_factory_i2c_with_config
   single: nx_factory_i2c_release

* ``nx_factory_i2c(instance)`` - Create I2C device
* ``nx_factory_i2c_with_config(instance, config)`` - Create I2C with configuration
* ``nx_factory_i2c_release(device)`` - Release I2C device

GPIO Interface
^^^^^^^^^^^^^^

.. index::
   single: nx_gpio_t
   single: gpio->write
   single: gpio->read
   single: gpio->toggle
   single: gpio->set_mode
   single: gpio->set_pull
   single: gpio->set_exti

* ``gpio->write(gpio, value)`` - Write GPIO pin
* ``gpio->read(gpio)`` - Read GPIO pin
* ``gpio->toggle(gpio)`` - Toggle GPIO pin
* ``gpio->set_mode(gpio, mode)`` - Set GPIO mode
* ``gpio->set_pull(gpio, pull)`` - Set GPIO pull resistor
* ``gpio->set_exti(gpio, trigger, callback, ctx)`` - Configure external interrupt

UART Interface
^^^^^^^^^^^^^^

.. index::
   single: nx_uart_t
   single: uart->get_tx_sync
   single: uart->get_rx_sync
   single: uart->get_tx_async
   single: uart->get_rx_async

* ``uart->get_tx_sync(uart)`` - Get synchronous TX interface
* ``uart->get_rx_sync(uart)`` - Get synchronous RX interface
* ``uart->get_tx_async(uart)`` - Get asynchronous TX interface
* ``uart->get_rx_async(uart)`` - Get asynchronous RX interface

OSAL API
--------

Core OSAL Functions
^^^^^^^^^^^^^^^^^^^

.. index::
   single: osal_init
   single: osal_start
   single: osal_get_backend

* ``osal_init()`` - Initialize OSAL
* ``osal_start()`` - Start OSAL scheduler
* ``osal_get_backend()`` - Get current backend name

See :doc:`../api/osal` for complete OSAL API documentation.

Task Management
^^^^^^^^^^^^^^^

.. index::
   single: osal_task_create
   single: osal_task_delete
   single: osal_task_suspend
   single: osal_task_resume
   single: osal_task_delay
   single: osal_task_get_current

* ``osal_task_create(name, func, arg, priority, stack_size)`` - Create task
* ``osal_task_delete(task)`` - Delete task
* ``osal_task_suspend(task)`` - Suspend task
* ``osal_task_resume(task)`` - Resume task
* ``osal_task_delay(ms)`` - Delay task
* ``osal_task_get_current()`` - Get current task handle

Synchronization
^^^^^^^^^^^^^^^

Mutex
"""""

.. index::
   single: osal_mutex_create
   single: osal_mutex_delete
   single: osal_mutex_lock
   single: osal_mutex_unlock
   single: osal_mutex_try_lock

* ``osal_mutex_create()`` - Create mutex
* ``osal_mutex_delete(mutex)`` - Delete mutex
* ``osal_mutex_lock(mutex, timeout)`` - Lock mutex
* ``osal_mutex_unlock(mutex)`` - Unlock mutex
* ``osal_mutex_try_lock(mutex)`` - Try to lock mutex

Semaphore
"""""""""

.. index::
   single: osal_sem_create
   single: osal_sem_delete
   single: osal_sem_wait
   single: osal_sem_post
   single: osal_sem_get_count

* ``osal_sem_create(initial_count, max_count)`` - Create semaphore
* ``osal_sem_delete(sem)`` - Delete semaphore
* ``osal_sem_wait(sem, timeout)`` - Wait on semaphore
* ``osal_sem_post(sem)`` - Post semaphore
* ``osal_sem_get_count(sem)`` - Get semaphore count

Message Queue
^^^^^^^^^^^^^

.. index::
   single: osal_queue_create
   single: osal_queue_delete
   single: osal_queue_send
   single: osal_queue_receive
   single: osal_queue_available

* ``osal_queue_create(length, item_size)`` - Create queue
* ``osal_queue_delete(queue)`` - Delete queue
* ``osal_queue_send(queue, item, timeout)`` - Send to queue
* ``osal_queue_receive(queue, item, timeout)`` - Receive from queue
* ``osal_queue_available(queue)`` - Get available items

Timer
^^^^^

.. index::
   single: osal_timer_create
   single: osal_timer_delete
   single: osal_timer_start
   single: osal_timer_stop
   single: osal_timer_reset

* ``osal_timer_create(name, period, auto_reload, callback, arg)`` - Create timer
* ``osal_timer_delete(timer)`` - Delete timer
* ``osal_timer_start(timer)`` - Start timer
* ``osal_timer_stop(timer)`` - Stop timer
* ``osal_timer_reset(timer)`` - Reset timer

Config Manager API
------------------

.. index::
   single: nx_config_init
   single: nx_config_deinit
   single: nx_config_set
   single: nx_config_get
   single: nx_config_delete
   single: nx_config_commit
   single: nx_config_rollback

* ``nx_config_init()`` - Initialize Config Manager
* ``nx_config_deinit()`` - Deinitialize Config Manager
* ``nx_config_set(key, value, type)`` - Set configuration value
* ``nx_config_get(key, value, type)`` - Get configuration value
* ``nx_config_delete(key)`` - Delete configuration key
* ``nx_config_commit()`` - Commit changes to storage
* ``nx_config_rollback()`` - Rollback uncommitted changes

See :doc:`../api/config` for complete Config Manager API documentation.

Log Framework API
-----------------

.. index::
   single: nx_log_init
   single: nx_log_deinit
   single: NX_LOG_ERROR
   single: NX_LOG_WARN
   single: NX_LOG_INFO
   single: NX_LOG_DEBUG
   single: NX_LOG_TRACE

* ``nx_log_init()`` - Initialize Log Framework
* ``nx_log_deinit()`` - Deinitialize Log Framework
* ``NX_LOG_ERROR(tag, fmt, ...)`` - Log error message
* ``NX_LOG_WARN(tag, fmt, ...)`` - Log warning message
* ``NX_LOG_INFO(tag, fmt, ...)`` - Log info message
* ``NX_LOG_DEBUG(tag, fmt, ...)`` - Log debug message
* ``NX_LOG_TRACE(tag, fmt, ...)`` - Log trace message

See :doc:`../api/log` for complete Log Framework API documentation.

Shell Framework API
-------------------

.. index::
   single: nx_shell_init
   single: nx_shell_deinit
   single: nx_shell_register_command
   single: nx_shell_unregister_command
   single: nx_shell_run

* ``nx_shell_init()`` - Initialize Shell Framework
* ``nx_shell_deinit()`` - Deinitialize Shell Framework
* ``nx_shell_register_command(name, handler, help)`` - Register command
* ``nx_shell_unregister_command(name)`` - Unregister command
* ``nx_shell_run()`` - Run shell main loop

See :doc:`../api/shell` for complete Shell Framework API documentation.

Init Framework API
------------------

.. index::
   single: nx_init_register
   single: NX_INIT_BOARD
   single: NX_INIT_PREV
   single: NX_INIT_BSP
   single: NX_INIT_DRIVER
   single: NX_INIT_COMPONENT
   single: NX_INIT_APP

* ``nx_init_register(level, func)`` - Register initialization function
* ``NX_INIT_BOARD(func)`` - Register board-level init
* ``NX_INIT_PREV(func)`` - Register pre-BSP init
* ``NX_INIT_BSP(func)`` - Register BSP-level init
* ``NX_INIT_DRIVER(func)`` - Register driver-level init
* ``NX_INIT_COMPONENT(func)`` - Register component-level init
* ``NX_INIT_APP(func)`` - Register application-level init

See :doc:`../api/init` for complete Init Framework API documentation.

See Also
--------

* :doc:`../api/index` - API Reference Overview
* :doc:`kconfig_index` - Configuration Options Index
* :ref:`genindex` - General Index
* :ref:`modindex` - Module Index
* :ref:`search` - Search Documentation
