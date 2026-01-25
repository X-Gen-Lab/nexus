SPI 通信教程
=============

本教程教你如何使用 Nexus HAL 的 SPI（串行外设接口）进行设备间通信。你将学习如何配置 SPI、与外部设备通信以及实现常见的 SPI 协议。

学习目标
--------

完成本教程后，你将能够：

- 理解 SPI 协议的工作原理
- 配置 SPI 主机和从机模式
- 与 SPI 外设通信（传感器、存储器、显示器等）
- 处理 SPI 数据传输
- 实现多从机 SPI 总线
- 调试 SPI 通信问题

前置条件
--------

- 完成 :doc:`first_application`、:doc:`gpio_control` 和 :doc:`uart_communication` 教程
- STM32F4 Discovery 开发板或兼容硬件
- 理解基本的数字通信概念
- 可选：逻辑分析仪用于调试

硬件设置
--------

本教程使用以下硬件：

**SPI 引脚（STM32F4 SPI1）：**

- PA5: SCK（时钟）
- PA6: MISO（主机输入从机输出）
- PA7: MOSI（主机输出从机输入）
- PA4: CS/NSS（片选，可选多个）

**可选外部设备：**

- SPI Flash 存储器（W25Q64、AT25SF041 等）
- SPI 传感器（BME280、MPU6050 等）
- SPI 显示器（ILI9341、ST7735 等）
- SPI ADC/DAC

.. warning::
   连接 SPI 设备时，确保电压电平匹配（3.3V 或 5V）。使用电平转换器保护 MCU。

第一部分：SPI 基础
------------------

SPI 协议原理
~~~~~~~~~~~~

SPI 是一种同步、全双工、主从式通信协议。

SPI 的关键特性：

- **同步通信**：使用时钟信号同步数据传输
- **全双工**：可以同时发送和接收数据
- **主从模式**：一个主机，一个或多个从机
- **高速**：可达数十 MHz
- **简单**：只需 4 根线（或 3 根线）

SPI 信号线：

- **SCK（Serial Clock）**：时钟信号，由主机生成
- **MOSI（Master Out Slave In）**：主机发送，从机接收
- **MISO（Master In Slave Out）**：从机发送，主机接收
- **CS/NSS（Chip Select）**：片选信号，选择从机

SPI 通信流程
~~~~~~~~~~~~

以下流程图展示了 SPI 通信的完整过程：

.. mermaid::
   :alt: SPI 通信流程，展示主机和从机之间的数据交换

   sequenceDiagram
       participant Master as SPI 主机
       participant Slave as SPI 从机

       Master->>Master: 拉低 CS（选中从机）
       Master->>Slave: 发送时钟信号（SCK）

       loop 每个时钟周期
           Master->>Slave: MOSI 发送数据位
           Slave->>Master: MISO 发送数据位
       end

       Master->>Master: 拉高 CS（释放从机）
       Master->>Master: 处理接收的数据

SPI 模式
~~~~~~~~

SPI 有 4 种模式，由时钟极性（CPOL）和时钟相位（CPHA）决定：

.. list-table:: SPI 模式
   :header-rows: 1
   :widths: 15 15 15 55

   * - 模式
     - CPOL
     - CPHA
     - 说明
   * - 0
     - 0
     - 0
     - 时钟空闲为低，第一个边沿采样
   * - 1
     - 0
     - 1
     - 时钟空闲为低，第二个边沿采样
   * - 2
     - 1
     - 0
     - 时钟空闲为高，第一个边沿采样
   * - 3
     - 1
     - 1
     - 时钟空闲为高，第二个边沿采样

.. note::
   大多数设备使用模式 0 或模式 3。查看设备数据手册确定正确的模式。

基本 SPI 示例
~~~~~~~~~~~~~

让我们从一个简单的 SPI 回环测试开始：

.. code-block:: c

    /**
     * \file            spi_basic.c
     * \brief           基本 SPI 通信示例
     * \author          Nexus Team
     * \version         1.0.0
     * \date            2026-01-25
     *
     * \copyright       Copyright (c) 2026 Nexus Team
     *
     * \details         演示如何配置和使用 SPI 进行数据传输
     */

    #include "hal/nx_hal.h"
    #include "osal/osal.h"

    /*-----------------------------------------------------------------------*/
    /* Configuration                                                         */
    /*-----------------------------------------------------------------------*/

    #define SPI_INDEX       0           /**< SPI 设备索引 */
    #define SPI_FREQ_HZ     1000000     /**< SPI 频率 1MHz */

    /*-----------------------------------------------------------------------*/
    /* Main Function                                                         */
    /*-----------------------------------------------------------------------*/

    int main(void) {
        /* 初始化 OSAL 和 HAL */
        osal_init();
        nx_hal_init();

        /* 获取 SPI 设备 */
        nx_spi_t* spi = nx_factory_spi(SPI_INDEX);
        if (!spi) {
            while (1) { /* 错误 */ }
        }

        /* 配置 SPI */
        if (spi->configure) {
            /* 配置参数（具体参数取决于 HAL 实现） */
            spi->configure(spi);
        }

        /* 准备测试数据 */
        uint8_t tx_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
        uint8_t rx_data[sizeof(tx_data)] = {0};

        /* 发送和接收数据 */
        if (spi->transfer) {
            nx_status_t status = spi->transfer(spi, tx_data, rx_data,
                                              sizeof(tx_data), 1000);

            if (status == NX_OK) {
                /* 传输成功 */
                /* 在回环模式下，rx_data 应该等于 tx_data */
            }
        }

        /* 主循环 */
        while (1) {
            osal_task_delay(1000);
        }

        return 0;
    }

**关键点：**

- SPI 传输是全双工的（同时发送和接收）
- 传输函数同时填充发送和接收缓冲区
- 超时参数防止无限等待

