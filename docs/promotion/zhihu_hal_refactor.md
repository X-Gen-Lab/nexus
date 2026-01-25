v} nx_dma_manager_t;

/* 获取 DMA 管理器单例 */
nx_dma_manager_t *nx_dma_manager_get(void);
```

#### ISR 管理器

```c
typedef struct nx_isr_manager_s {
    nx_isr_handle_t *(*connect)(nx_isr_manager_t *self, uint32_t irq,
                                 nx_isr_func_t func, void *data, 
                                 nx_isr_priority_t priority);
    nx_status_t (*disconnect)(nx_isr_manager_t *self, nx_isr_handle_t *handle);
    nx_status_t (*set_hw_priority)(nx_isr_manager_t *self, uint32_t irq, uint8_t hw_prio);
    nx_status_t (*enable)(nx_isr_manager_t *self, uint32_t irq);
    nx_status_t (*disable)(nx_isr_manager_t *self, uint32_t irq);
} nx_isr_manager_t;

/* 获取 ISR 管理器单例 */
nx_isr_manager_t *nx_isr_manager_get(void);
```

**设计亮点**：

- **集中管理**：避免资源冲突
- **单例模式**：全局唯一实例
- **优先级管理**：支持多级优先级
- **回调链**：一个中断可以注册多个回调

### 3.5 工厂层

应用层通过工厂获取设备：

```c
/* 获取 UART 设备（基于 Kconfig 配置） */
nx_uart_t *uart = nx_factory_uart(0);  /* 获取 UART0 */
if (uart) {
    /* 获取生命周期接口并初始化 */
    nx_lifecycle_t *lifecycle = uart->get_lifecycle(uart);
    if (lifecycle) {
        lifecycle->init(lifecycle);
    }
    
    /* 使用设备 */
    nx_tx_async_t *tx = uart->get_tx_async(uart);
    if (tx) {
        tx->send(tx, (const uint8_t *)"Hello", 5);
    }
}

/* 获取 GPIO 写接口 */
nx_gpio_write_t *led = nx_factory_gpio_write('A', 5);  /* GPIOA5 */
if (led) {
    /* 初始化 */
    nx_lifecycle_t *lifecycle = led->get_lifecycle(led);
    if (lifecycle) {
        lifecycle->init(lifecycle);
    }
    
    /* 点亮 LED */
    led->write(led, 1);
}
```

**设计亮点**：

- **Kconfig 驱动**：只能获取 Kconfig 中启用的设备
- **延迟初始化**：首次 get 时才初始化
- **API 缓存**：初始化后缓存 API 指针，后续调用直接返回
- **需要手动 init**：获取设备后需要调用 `lifecycle->init()`

---

## 四、GPIO 接口的读写分离

GPIO 是一个很好的例子，展示了接口隔离原则：

```c
/* GPIO 读接口（输入模式） */
typedef struct nx_gpio_read_s {
    uint8_t (*read)(nx_gpio_read_t *self);
    nx_status_t (*register_exti)(nx_gpio_read_t *self,
                                 nx_gpio_callback_t callback, 
                                 void *user_data,
                                 nx_gpio_trigger_t trigger);
    nx_lifecycle_t *(*get_lifecycle)(nx_gpio_read_t *self);
    nx_power_t *(*get_power)(nx_gpio_read_t *self);
} nx_gpio_read_t;

/* GPIO 写接口（输出模式） */
typedef struct nx_gpio_write_s {
    void (*write)(nx_gpio_write_t *self, uint8_t state);
    void (*toggle)(nx_gpio_write_t *self);
    nx_lifecycle_t *(*get_lifecycle)(nx_gpio_write_t *self);
    nx_power_t *(*get_power)(nx_gpio_write_t *self);
} nx_gpio_write_t;

/* GPIO 读写接口（双向模式） */
typedef struct nx_gpio_read_write_s {
    nx_gpio_read_t read;
    nx_gpio_write_t write;
} nx_gpio_read_write_t;
```

**设计亮点**：

- **读写分离**：输入和输出是两个独立接口
- **类型安全**：编译时就能发现错误（如对输入 GPIO 调用 write）
- **灵活组合**：可以单独使用 read 或 write，也可以组合使用
- **接口最小化**：每个接口只包含必要的方法

---

## 五、延迟初始化机制

实际的实现非常简洁：

```c
void *nx_device_get(const char *name) {
    const nx_device_t *dev = nx_device_find(name);
    if (dev == NULL) {
        return NULL;
    }

    return nx_device_init(dev);
}

void *nx_device_init(const nx_device_t *dev) {
    if (dev == NULL) {
        return NULL;
    }

    /* 如果已经初始化，直接返回缓存的 API 指针 */
    if (dev->state->initialized) {
        return dev->state->api;
    }

    /* 第一次获取，执行初始化 */
    if (dev->device_init == NULL) {
        return NULL;
    }

    void *api = dev->device_init(dev);

    if (api != NULL) {
        /* 缓存 API 指针 */
        dev->state->api = api;
        dev->state->initialized = true;
        dev->state->init_res = 0;
    } else {
        dev->state->init_res = 1;
    }

    return api;
}
```

**设计亮点**：

- **延迟初始化**：首次使用时才初始化，节省启动时间
- **API 缓存**：初始化后缓存指针，后续调用零开销
- **简化管理**：无需手动 init/deinit，无引用计数
- **状态保存**：`dev->state` 是可写的，用于保存运行时状态

---

## 六、实际使用示例

### 6.1 简单的 LED 闪烁

```c
#include "hal/nx_factory.h"

void blink_led(void) {
    /* 获取 GPIO 写接口 */
    nx_gpio_write_t *led = nx_factory_gpio_write('A', 5);
    if (!led) return;

    /* 获取生命周期接口并初始化 */
    nx_lifecycle_t *lifecycle = led->get_lifecycle(led);
    if (lifecycle) {
        lifecycle->init(lifecycle);
    }

    /* 闪烁 10 次 */
    for (int i = 0; i < 10; i++) {
        led->toggle(led);
        /* 注意：实际项目中需要使用 OSAL 的延时函数 */
        /* 这里仅作示意 */
    }
}
```

### 6.2 UART 异步发送

```c
#include "hal/nx_factory.h"
#include <string.h>

void uart_send_async(void) {
    /* 获取 UART 设备 */
    nx_uart_t *uart = nx_factory_uart(0);
    if (!uart) return;

    /* 获取生命周期接口并初始化 */
    nx_lifecycle_t *lifecycle = uart->get_lifecycle(uart);
    if (lifecycle) {
        lifecycle->init(lifecycle);
    }

    /* 获取异步发送接口 */
    nx_tx_async_t *tx = uart->get_tx_async(uart);
    if (!tx) return;
    
    /* 发送数据 */
    const char *msg = "Hello, Nexus!\n";
    size_t len = strlen(msg);
    tx->send(tx, (const uint8_t *)msg, len);

    /* 等待发送完成 */
    while (tx->get_state(tx) == NX_ERR_BUSY) {
        /* 等待 */
    }
}
```

### 6.3 UART 同步接收

```c
void uart_receive_sync(void) {
    nx_uart_t *uart = nx_factory_uart(0);
    if (!uart) return;

    /* 初始化 */
    nx_lifecycle_t *lifecycle = uart->get_lifecycle(uart);
    if (lifecycle) {
        lifecycle->init(lifecycle);
    }

    /* 获取同步接收接口 */
    nx_rx_sync_t *rx = uart->get_rx_sync(uart);
    if (!rx) return;

    /* 接收数据（阻塞，超时 1000ms） */
    uint8_t buffer[64];
    size_t len = sizeof(buffer);
    nx_status_t status = rx->receive(rx, buffer, &len, 1000);
    
    if (status == NX_OK) {
        /* 处理接收到的数据 */
        /* 实际应用中在这里处理 buffer 中的数据 */
    }
}
```

### 6.4 GPIO 中断

```c
#include <stdio.h>

void gpio_button_callback(void *user_data) {
    /* 按钮按下的处理 */
    printf("Button pressed!\n");
}

void setup_button_interrupt(void) {
    /* 获取 GPIO 读接口 */
    nx_gpio_read_t *button = nx_factory_gpio_read('C', 13);
    if (!button) return;

    /* 初始化 */
    nx_lifecycle_t *lifecycle = button->get_lifecycle(button);
    if (lifecycle) {
        lifecycle->init(lifecycle);
    }

    /* 注册中断回调（下降沿触发） */
    button->register_exti(button, gpio_button_callback, NULL,
                         NX_GPIO_TRIGGER_FALLING);
}
```

---

## 七、与旧设计对比

| 特性 | 旧设计 | 新设计 |
|------|--------|--------|
| 接口设计 | 单一臃肿接口 | 多个小接口组合 |
| 读写分离 | 混在一起 | GPIO 读写完全分离 |
| 同步异步 | 混在一起 | 通信接口完全分离 |
| 设备注册 | 手动注册 | Kconfig + Linker Section |
| 初始化 | 手动 init/deinit | 延迟初始化 + API 缓存 |
| 资源管理 | 分散在各驱动 | 集中管理（DMA/ISR） |
| 代码复用 | 低 | 高 |
| 可测试性 | 一般 | 优秀 |
| 内存占用 | 中等 | 略高（但可控） |

---

## 八、性能和内存开销

### 8.1 内存开销

- **设备描述符**：约 16-24 字节（编译时分配）
- **设备状态**：约 8-16 字节（运行时分配）
- **实现结构**：根据接口数量，约 100-200 字节
- **接口指针**：每个接口 4-8 字节（取决于平台）

**总体评估**：相比旧设计，每个设备增加约 50-100 字节开销，但换来了更好的架构和功能。

### 8.2 性能开销

- **函数指针调用**：1-2 个时钟周期（现代 CPU 有分支预测）
- **延迟初始化**：首次调用约 100-1000 个时钟周期，后续零开销
- **工厂函数**：static inline，零开销

**总体评估**：性能开销可以忽略不计，对于嵌入式系统完全可以接受。

---

## 九、测试策略

### 9.1 单元测试

```cpp
TEST(NxDeviceTest, LazyInitialization) {
    /* 第一次获取，应该初始化 */
    nx_uart_t *uart1 = nx_factory_uart(0);
    ASSERT_NE(uart1, nullptr);

    /* 第二次获取，应该返回缓存的指针 */
    nx_uart_t *uart2 = nx_factory_uart(0);
    ASSERT_EQ(uart1, uart2);  /* 同一个指针 */
}
```

### 9.2 接口测试

Native 平台提供了完整的测试支持：

```cpp
TEST(NxUartTest, AsyncSend) {
    nx_uart_t *uart = nx_factory_uart(0);
    ASSERT_NE(uart, nullptr);

    /* 初始化 */
    nx_lifecycle_t *lifecycle = uart->get_lifecycle(uart);
    ASSERT_EQ(lifecycle->init(lifecycle), NX_OK);

    /* 获取异步发送接口 */
    nx_tx_async_t *tx = uart->get_tx_async(uart);
    ASSERT_NE(tx, nullptr);

    /* 发送数据 */
    const char *msg = "test";
    EXPECT_EQ(tx->send(tx, (const uint8_t *)msg, 4), NX_OK);
}
```

### 9.3 GPIO 读写分离测试

```cpp
TEST(NxGpioTest, ReadWriteSeparation) {
    /* 获取写接口 */
    nx_gpio_write_t *led = nx_factory_gpio_write('A', 5);
    ASSERT_NE(led, nullptr);

    /* 初始化 */
    nx_lifecycle_t *lifecycle = led->get_lifecycle(led);
    ASSERT_EQ(lifecycle->init(lifecycle), NX_OK);

    /* 写操作 */
    led->write(led, 1);
    led->toggle(led);

    /* 获取读接口 */
    nx_gpio_read_t *button = nx_factory_gpio_read('C', 13);
    ASSERT_NE(button, nullptr);

    /* 读操作 */
    uint8_t state = button->read(button);
    EXPECT_GE(state, 0);
    EXPECT_LE(state, 1);
}
```

### 9.4 测试覆盖

Native 平台已实现完整的测试套件：

- **单元测试**：每个外设都有独立的单元测试
- **属性测试**：验证接口的正确性属性
- **集成测试**：测试多个外设协同工作
- **测试覆盖**：覆盖所有主要外设（UART、GPIO、SPI、I2C、Timer、ADC、DAC、Flash、RTC、Watchdog、CRC、USB、SDIO）

测试文件位置：`tests/hal/native/`

---

## 十、未来计划

### 10.1 已完成 ✅

- ✅ 完成核心接口设计（lifecycle、power、diagnostic）
- ✅ 实现设备基类和 Kconfig 驱动注册
- ✅ 实现资源管理器（DMA、ISR）
- ✅ 完成 Native 平台适配（用于测试）
- ✅ 编写完整的单元测试和属性测试
- ✅ 支持所有主要外设（UART、GPIO、SPI、I2C、Timer、ADC、DAC、Flash、RTC、Watchdog、CRC、USB、SDIO）

### 10.2 进行中 🚧

- 🚧 完成 STM32F4 平台适配
- 🚧 完善文档和示例
- 🚧 性能优化和内存优化

### 10.3 计划中 📋

- 适配更多平台（STM32H7、ESP32、nRF52）
- 支持热插拔设备
- 支持设备树（Device Tree）
- 支持运行时设备发现

---

## 十一、如何参与

这次重构还在进行中，欢迎大家参与：

### 代码贡献

- 实现新的平台适配
- 添加新的外设驱动
- 优化性能和内存
- 修复 Bug

### 文档贡献

- 完善 API 文档
- 编写使用教程
- 翻译文档

### 测试贡献

- 编写单元测试
- 编写属性测试
- 在真实硬件上测试

**GitHub 地址**：https://github.com/X-Gen-Lab/nexus

---

## 结语

这次 HAL 重构，我尝试用纯 C 语言实现了很多现代化的设计模式：

- **接口隔离**：读写分离、同步异步分离
- **组合模式**：通过 getter 获取子接口
- **工厂模式**：统一的设备获取接口
- **延迟初始化**：首次使用时才初始化
- **Kconfig 驱动**：编译时设备注册

有人说 C 语言不适合写面向对象代码，但我觉得，只要设计得当，C 语言一样可以写出优雅、可维护的代码。

关键在于：

1. **清晰的抽象**：接口定义要清晰，职责要单一
2. **接口隔离**：读写分离、同步异步分离
3. **组合优于继承**：灵活性更高，易于扩展
4. **统一的资源管理**：避免混乱和冲突

如果你对这次重构感兴趣，欢迎来 GitHub 看看代码，提出你的想法和建议。

**让我们一起，用 C 语言写出更优雅的嵌入式代码。**

---

> 💬 欢迎在评论区讨论设计思路和实现细节
> 
> 👍 如果觉得有启发，点个赞支持一下
> 
> ⭐ GitHub: https://github.com/X-Gen-Lab/nexus

---

*本文首发于知乎，转载请注明出处。*

**文章版本**: v1.0  
**发布日期**: 2026-01-25  
**作者**: Nexus Team  
**许可证**: CC BY-NC-SA 4.0
