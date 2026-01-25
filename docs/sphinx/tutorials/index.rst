Tutorials
=========

This section provides step-by-step tutorials to help you learn the Nexus platform through practical examples.

.. contents:: Table of Contents
   :local:
   :depth: 2

.. toctree::
   :maxdepth: 2
   :caption: Tutorial Contents

   first_application
   gpio_control
   uart_communication
   task_creation
   interrupt_handling
   timer_pwm
   spi_communication
   i2c_sensors
   adc_sampling
   examples

Learning Path
-------------

If you're new to Nexus, follow these tutorials in order:

Beginner Tutorials (Weeks 1-2)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. :doc:`first_application` - Build your first Nexus application from scratch
2. :doc:`gpio_control` - Learn GPIO control and LED operations
3. :doc:`uart_communication` - Add serial communication to your application
4. :doc:`task_creation` - Create multi-task applications using OSAL

Intermediate Tutorials (Weeks 3-4)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

5. :doc:`interrupt_handling` - Master interrupt handling and event-driven programming
6. :doc:`timer_pwm` - Timer and PWM control (LED dimming, servos, motors)
7. :doc:`spi_communication` - SPI communication and peripheral control

Advanced Tutorials (Weeks 5-6)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

8. **I2C Sensor Integration** - Read sensor data using I2C protocol
9. **ADC Sampling and Signal Processing** - Analog signal acquisition and processing
10. **DMA Transfers** - Efficient data transfer techniques
11. **Multi-task Synchronization** - Advanced OSAL synchronization mechanisms
12. **State Machine Design** - Build robust state machine applications

Project Tutorials (Weeks 7-8)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

13. **Weather Station Project** - Complete system integrating multiple sensors
14. **Data Logger** - Data acquisition and storage system
15. **Motor Control System** - Closed-loop motor control
16. **IoT Device** - Smart device connected to cloud platforms

Each tutorial builds on concepts from previous ones, so we recommend following them in order.

Prerequisites
-------------

Before starting these tutorials, ensure you have:

* Completed the :doc:`../getting_started/environment_setup` guide
* A supported development board (STM32F4 Discovery recommended)
* Basic knowledge of C programming
* Familiarity with embedded systems concepts

Learning Stages
---------------

Stage 1: Fundamentals (1-2 weeks)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Complete beginner tutorials 1-4
* Understand HAL and OSAL basic concepts
* Able to create simple applications

Stage 2: Intermediate Skills (2-3 weeks)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Complete intermediate tutorials
* Master interrupt and event-driven programming
* Understand peripheral communication protocols

Stage 3: Advanced Applications (3-4 weeks)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Study advanced tutorials
* Implement complex multi-task systems
* Optimize performance and power consumption

Stage 4: Real Projects (4+ weeks)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Complete project tutorials
* Build complete embedded systems
* Apply best practices

Additional Resources
--------------------

* :doc:`../user_guide/hal` - HAL API Reference
* :doc:`../user_guide/osal` - OSAL API Reference
* :doc:`../user_guide/config` - Configuration Management
* :doc:`../user_guide/shell` - Shell/CLI Framework
* :doc:`../development/testing` - Testing Your Application

Example Applications
--------------------

See :doc:`examples` for complete example applications, including:

* **blinky** - Basic GPIO control
* **config_demo** - Configuration management demonstration
* **freertos_demo** - Multi-task OSAL demonstration
* **shell_demo** - Command-line interface demonstration

Getting Help
------------

If you encounter issues:

1. Check :doc:`../getting_started/faq` for common questions
2. Review :doc:`../user_guide/debugging` debugging guide
3. Consult platform-specific documentation
4. Examine example code
5. Ask on `GitHub Discussions <https://github.com/X-Gen-Lab/nexus/discussions>`_

Next Steps
----------

After completing tutorials:

* Explore :doc:`../user_guide/index` for in-depth documentation
* Review :doc:`../platform_guides/index` for platform-specific features
* Check :doc:`../api/index` for complete API reference
* Join the community and share your projects!

