# Shell Framework 移植指南

本文档详细描述如何将 Nexus Shell Framework 移植到不同的平台和环境。

## 目录

1. [移植概述](#移植概述)
2. [依赖项分析](#依赖项分析)
3. [平台适配](#平台适配)
4. [后端实现](#后端实现)
5. [编译配置](#编译配置)
6. [移植步骤](#移植步骤)
7. [平台特定优化](#平台特定优化)
8. [验证清单](#验证清单)
9. [故障排查](#故障排查)
10. [示例项目](#示例项目)

## 移植概述

### 可移植性设计

Shell Framework 采用分层设计，最大化可移植性：

```
┌─────────────────────────────────────────┐
│         Shell 核心层                     │  平台无关
│  - 命令管理                              │
│  - 行编辑                                │
│  - 历史管理                              │
│  - 自动补全                              │
│  - 参数解析                              │
└─────────────────────────────────────────┘
                  ▲
                  │ 后端接口（需要实现）
                  ▼
┌─────────────────────────────────────────┐
│         后端层                           │  平台相关
│  - UART 后端                             │
│  - Console 后端                          │
│  - 自定义后端                            │
└─────────────────────────────────────────┘
                  ▲
                  │ HAL 调用
                  ▼
┌─────────────────────────────────────────┐
│         硬件抽象层 (HAL)                 │  平台相关
│  - UART 驱动                             │
│  - GPIO 驱动                             │
└─────────────────────────────────────────┘
```

**关键特性**：
- 核心代码平台无关
- 使用标准 C99
- 最小化平台依赖
- 清晰的接口定义

### 支持的平台

| 平台类型 | 示例 | 移植难度 | 预计工作量 |
|---------|------|---------|-----------|
| ARM Cortex-M | STM32, NRF52 | 低 | 1-2 天 |
| ARM Cortex-A | i.MX, Raspberry Pi | 低 | 1-2 天 |
| RISC-V | ESP32-C3, GD32VF103 | 低 | 1-2 天 |
| x86/x64 | Linux, Windows | 极低 | 0.5-1 天 |
| 8051 | STC, AT89 | 中 | 3-5 天 |
| AVR | ATmega | 中 | 3-5 天 |

### 工作量评估

| 任务 | 工作量 | 说明 |
|------|--------|------|
| HAL UART 接口实现 | 2-4 小时 | 如果已有 HAL 层 |
| UART 后端实现 | 1-2 小时 | 基于 HAL 接口 |
| Console 后端实现 | 1-2 小时 | 仅 Native 平台 |
| 编译配置 | 1-2 小时 | CMake 或 Makefile |
| 测试验证 | 2-4 小时 | 功能测试 |
| **总计** | **1-2 天** | 完整移植 |

## 依赖项分析

### 必需依赖

#### 1. 标准 C 库

```c
#include <stdint.h>    /* 整数类型 */
#include <stdbool.h>   /* 布尔类型 */
#include <stddef.h>    /* size_t, NULL */
#include <string.h>    /* 字符串操作 */
#include <stdlib.h>    /* malloc, free */
#include <stdio.h>     /* snprintf, vsnprintf */
#include <stdarg.h>    /* 可变参数 */
```

**最小要求**：
- C99 标准
- 支持动态内存分配（malloc/free）
- 支持可变参数函数

**替代方案**：
- 如果不支持动态分配，可以使用静态分配模式（需要修改）
- 如果不支持 snprintf，可以使用简化版本

#### 2. HAL UART 接口

```c
/* UART 初始化 */
hal_status_t hal_uart_init(hal_uart_t uart, const hal_uart_config_t* config);

/* UART 反初始化 */
hal_status_t hal_uart_deinit(hal_uart_t uart);

/* UART 读取（非阻塞） */
int hal_uart_read(hal_uart_t uart, uint8_t* buf, size_t len);

/* UART 写入 */
int hal_uart_write(hal_uart_t uart, const uint8_t* buf, size_t len);

/* UART 可用字节数 */
int hal_uart_available(hal_uart_t uart);
```

**关键要求**：
- `hal_uart_read` 必须是非阻塞的
- `hal_uart_write` 可以是阻塞或非阻塞
- 返回实际读写的字节数

### 可选依赖

#### 1. OSAL（线程安全）

如果需要线程安全，需要 OSAL 互斥锁：

```c
/* 互斥锁创建 */
osal_status_t osal_mutex_create(osal_mutex_t* mutex);

/* 互斥锁销毁 */
osal_status_t osal_mutex_destroy(osal_mutex_t* mutex);

/* 互斥锁加锁 */
osal_status_t osal_mutex_lock(osal_mutex_t* mutex);

/* 互斥锁解锁 */
osal_status_t osal_mutex_unlock(osal_mutex_t* mutex);
```

**注意**：当前版本不需要 OSAL，设计为单线程使用。

#### 2. 文件系统（未来功能）

未来版本可能需要文件系统支持：

```c
/* 文件操作 */
int fs_open(const char* path, int flags);
int fs_read(int fd, void* buf, size_t len);
int fs_write(int fd, const void* buf, size_t len);
int fs_close(int fd);
```

### 内存需求

| 组件 | RAM | Flash | 说明 |
|------|-----|-------|------|
| Shell 核心 | ~100 B | ~8 KB | 固定开销 |
| 命令缓冲区 | 128 B | - | 默认配置 |
| 历史存储 | 2048 B | - | 16 * 128 |
| 命令注册表 | ~256 B | - | 32 个命令 |
| 栈空间 | ~512 B | - | 每次调用 |
| **总计** | **~3 KB** | **~8 KB** | 默认配置 |

## 平台适配

### ARM Cortex-M 平台

#### STM32 示例

```c
/* HAL UART 实现 */
#include "stm32f4xx_hal.h"

static UART_HandleTypeDef huart1;

hal_status_t hal_uart_init(hal_uart_t uart, const hal_uart_config_t* config) {
    if (uart != HAL_UART_0) {
        return HAL_ERROR;
    }
    
    /* 配置 UART */
    huart1.Instance = USART1;
    huart1.Init.BaudRate = config->baudrate;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        return HAL_ERROR;
    }
    
    return HAL_OK;
}

int hal_uart_read(hal_uart_t uart, uint8_t* buf, size_t len) {
    if (uart != HAL_UART_0 || len == 0) {
        return 0;
    }
    
    /* 非阻塞读取 */
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE)) {
        HAL_UART_Receive(&huart1, buf, 1, 0);
        return 1;
    }
    
    return 0;
}

int hal_uart_write(hal_uart_t uart, const uint8_t* buf, size_t len) {
    if (uart != HAL_UART_0) {
        return 0;
    }
    
    /* 阻塞写入 */
    HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, HAL_MAX_DELAY);
    return len;
}

int hal_uart_available(hal_uart_t uart) {
    if (uart != HAL_UART_0) {
        return 0;
    }
    
    return __HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) ? 1 : 0;
}
```

#### NRF52 示例

```c
/* HAL UART 实现 */
#include "nrf_uart.h"
#include "nrf_drv_uart.h"

static nrf_drv_uart_t uart_inst = NRF_DRV_UART_INSTANCE(0);
static uint8_t rx_buffer[1];
static volatile bool rx_ready = false;

static void uart_event_handler(nrf_drv_uart_event_t* event, void* context) {
    if (event->type == NRF_DRV_UART_EVT_RX_DONE) {
        rx_ready = true;
    }
}

hal_status_t hal_uart_init(hal_uart_t uart, const hal_uart_config_t* config) {
    nrf_drv_uart_config_t uart_config = NRF_DRV_UART_DEFAULT_CONFIG;
    uart_config.baudrate = NRF_UART_BAUDRATE_115200;
    uart_config.hwfc = NRF_UART_HWFC_DISABLED;
    
    ret_code_t err = nrf_drv_uart_init(&uart_inst, &uart_config, 
                                       uart_event_handler);
    if (err != NRF_SUCCESS) {
        return HAL_ERROR;
    }
    
    /* 启动接收 */
    nrf_drv_uart_rx(&uart_inst, rx_buffer, 1);
    
    return HAL_OK;
}

int hal_uart_read(hal_uart_t uart, uint8_t* buf, size_t len) {
    if (!rx_ready || len == 0) {
        return 0;
    }
    
    *buf = rx_buffer[0];
    rx_ready = false;
    
    /* 重新启动接收 */
    nrf_drv_uart_rx(&uart_inst, rx_buffer, 1);
    
    return 1;
}

int hal_uart_write(hal_uart_t uart, const uint8_t* buf, size_t len) {
    nrf_drv_uart_tx(&uart_inst, buf, len);
    return len;
}
```

### ARM Cortex-A 平台

#### Linux 示例

```c
/* HAL UART 实现（使用串口设备） */
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

static int uart_fd = -1;

hal_status_t hal_uart_init(hal_uart_t uart, const hal_uart_config_t* config) {
    /* 打开串口设备 */
    uart_fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (uart_fd < 0) {
        return HAL_ERROR;
    }
    
    /* 配置串口 */
    struct termios options;
    tcgetattr(uart_fd, &options);
    
    /* 设置波特率 */
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    
    /* 8N1 */
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    
    /* 原始模式 */
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;
    
    tcsetattr(uart_fd, TCSANOW, &options);
    
    return HAL_OK;
}

int hal_uart_read(hal_uart_t uart, uint8_t* buf, size_t len) {
    if (uart_fd < 0) {
        return 0;
    }
    
    ssize_t n = read(uart_fd, buf, len);
    return (n > 0) ? (int)n : 0;
}

int hal_uart_write(hal_uart_t uart, const uint8_t* buf, size_t len) {
    if (uart_fd < 0) {
        return 0;
    }
    
    ssize_t n = write(uart_fd, buf, len);
    return (n > 0) ? (int)n : 0;
}

hal_status_t hal_uart_deinit(hal_uart_t uart) {
    if (uart_fd >= 0) {
        close(uart_fd);
        uart_fd = -1;
    }
    return HAL_OK;
}
```

### RISC-V 平台

#### ESP32-C3 示例

```c
/* HAL UART 实现 */
#include "driver/uart.h"

#define UART_NUM UART_NUM_0
#define BUF_SIZE 128

hal_status_t hal_uart_init(hal_uart_t uart, const hal_uart_config_t* config) {
    uart_config_t uart_config = {
        .baud_rate = config->baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    
    uart_param_config(UART_NUM, &uart_config);
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    
    return HAL_OK;
}

int hal_uart_read(hal_uart_t uart, uint8_t* buf, size_t len) {
    int n = uart_read_bytes(UART_NUM, buf, len, 0);
    return (n > 0) ? n : 0;
}

int hal_uart_write(hal_uart_t uart, const uint8_t* buf, size_t len) {
    return uart_write_bytes(UART_NUM, (const char*)buf, len);
}

int hal_uart_available(hal_uart_t uart) {
    size_t available;
    uart_get_buffered_data_len(UART_NUM, &available);
    return (int)available;
}
```

### Native 平台（PC）

#### Windows 示例

```c
/* Console 后端实现 */
#include <windows.h>
#include <conio.h>

static HANDLE hStdin;
static HANDLE hStdout;

static int console_backend_read(char* buf, size_t len) {
    if (_kbhit()) {
        *buf = _getch();
        return 1;
    }
    return 0;
}

static int console_backend_write(const char* buf, size_t len) {
    DWORD written;
    WriteConsole(hStdout, buf, len, &written, NULL);
    return (int)written;
}

const shell_backend_t shell_console_backend = {
    .read = console_backend_read,
    .write = console_backend_write
};

void console_backend_init(void) {
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    
    /* 设置控制台模式 */
    DWORD mode;
    GetConsoleMode(hStdin, &mode);
    mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
    SetConsoleMode(hStdin, mode);
}
```

#### Linux/macOS 示例

```c
/* Console 后端实现 */
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

static struct termios orig_termios;

static int console_backend_read(char* buf, size_t len) {
    /* 设置非阻塞 */
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    
    ssize_t n = read(STDIN_FILENO, buf, len);
    
    /* 恢复标志 */
    fcntl(STDIN_FILENO, F_SETFL, flags);
    
    return (n > 0) ? (int)n : 0;
}

static int console_backend_write(const char* buf, size_t len) {
    ssize_t n = write(STDOUT_FILENO, buf, len);
    return (n > 0) ? (int)n : 0;
}

const shell_backend_t shell_console_backend = {
    .read = console_backend_read,
    .write = console_backend_write
};

void console_backend_init(void) {
    /* 保存原始终端设置 */
    tcgetattr(STDIN_FILENO, &orig_termios);
    
    /* 设置原始模式 */
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void console_backend_deinit(void) {
    /* 恢复终端设置 */
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}
```

## 后端实现

### 后端接口规范

```c
/**
 * \brief           Shell 后端接口
 */
typedef struct {
    /**
     * \brief           读取函数（非阻塞）
     * \param[out]      buf: 接收缓冲区
     * \param[in]       len: 缓冲区大小
     * \return          实际读取的字节数，0 表示无数据
     */
    int (*read)(char* buf, size_t len);
    
    /**
     * \brief           写入函数
     * \param[in]       buf: 发送缓冲区
     * \param[in]       len: 数据长度
     * \return          实际写入的字节数
     */
    int (*write)(const char* buf, size_t len);
} shell_backend_t;
```

**关键要求**：

1. **read 函数**：
   - 必须是非阻塞的
   - 无数据时立即返回 0
   - 返回实际读取的字节数
   - 通常每次只读取 1 个字节

2. **write 函数**：
   - 可以是阻塞或非阻塞
   - 返回实际写入的字节数
   - 应该尽快完成

### UART 后端模板

```c
/* uart_backend.c */
#include "shell/shell_backend.h"
#include "hal/hal_uart.h"

/**
 * \brief           UART 后端读取函数
 */
static int uart_backend_read(char* buf, size_t len) {
    /* 检查是否有数据可读 */
    if (hal_uart_available(HAL_UART_0) > 0) {
        /* 读取一个字节 */
        return hal_uart_read(HAL_UART_0, (uint8_t*)buf, 1);
    }
    
    return 0;  /* 无数据 */
}

/**
 * \brief           UART 后端写入函数
 */
static int uart_backend_write(const char* buf, size_t len) {
    /* 写入数据 */
    return hal_uart_write(HAL_UART_0, (const uint8_t*)buf, len);
}

/**
 * \brief           UART 后端实例
 */
const shell_backend_t shell_uart_backend = {
    .read = uart_backend_read,
    .write = uart_backend_write
};
```

### 自定义后端示例

#### SPI 后端

```c
/* spi_backend.c */
#include "shell/shell_backend.h"
#include "hal/hal_spi.h"

static uint8_t rx_buffer[256];
static size_t rx_head = 0;
static size_t rx_tail = 0;

/**
 * \brief           SPI 接收中断处理
 */
void spi_rx_irq_handler(void) {
    uint8_t data;
    if (hal_spi_read(HAL_SPI_0, &data, 1) > 0) {
        rx_buffer[rx_head] = data;
        rx_head = (rx_head + 1) % sizeof(rx_buffer);
    }
}

/**
 * \brief           SPI 后端读取函数
 */
static int spi_backend_read(char* buf, size_t len) {
    if (rx_head == rx_tail) {
        return 0;  /* 无数据 */
    }
    
    *buf = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % sizeof(rx_buffer);
    
    return 1;
}

/**
 * \brief           SPI 后端写入函数
 */
static int spi_backend_write(const char* buf, size_t len) {
    return hal_spi_write(HAL_SPI_0, (const uint8_t*)buf, len);
}

const shell_backend_t shell_spi_backend = {
    .read = spi_backend_read,
    .write = spi_backend_write
};
```

#### USB CDC 后端

```c
/* usb_cdc_backend.c */
#include "shell/shell_backend.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"

/**
 * \brief           USB CDC 后端读取函数
 */
static int usb_cdc_backend_read(char* buf, size_t len) {
    /* 从 USB CDC 接收缓冲区读取 */
    return CDC_Receive_FS((uint8_t*)buf, len);
}

/**
 * \brief           USB CDC 后端写入函数
 */
static int usb_cdc_backend_write(const char* buf, size_t len) {
    /* 发送到 USB CDC */
    CDC_Transmit_FS((uint8_t*)buf, len);
    return len;
}

const shell_backend_t shell_usb_cdc_backend = {
    .read = usb_cdc_backend_read,
    .write = usb_cdc_backend_write
};
```


## 编译配置

### CMake 配置

#### 基本配置

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(my_shell_app C)

# 设置 C 标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 添加 Shell 框架
add_subdirectory(framework/shell)

# 添加 HAL 层
add_subdirectory(hal)

# 创建可执行文件
add_executable(my_app
    src/main.c
    src/hal_uart.c
    src/uart_backend.c
)

# 链接库
target_link_libraries(my_app
    PRIVATE
        shell
        hal
)

# 包含目录
target_include_directories(my_app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
```

#### 交叉编译配置

```cmake
# toolchain-arm-none-eabi.cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

# 工具链路径
set(TOOLCHAIN_PREFIX arm-none-eabi-)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE ${TOOLCHAIN_PREFIX}size)

# 编译选项
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-specs=nano.specs -specs=nosys.specs")

# 使用方式
# cmake -DCMAKE_TOOLCHAIN_FILE=toolchain-arm-none-eabi.cmake ..
```

#### 平台特定配置

```cmake
# 检测平台
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Linux 平台
    target_compile_definitions(my_app PRIVATE PLATFORM_LINUX=1)
    target_sources(my_app PRIVATE src/console_backend_linux.c)
    
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Windows 平台
    target_compile_definitions(my_app PRIVATE PLATFORM_WINDOWS=1)
    target_sources(my_app PRIVATE src/console_backend_windows.c)
    
elseif(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    # 嵌入式平台
    target_compile_definitions(my_app PRIVATE PLATFORM_EMBEDDED=1)
    target_sources(my_app PRIVATE src/uart_backend.c)
endif()
```

### Makefile 配置

```makefile
# Makefile
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

# 编译选项
CFLAGS = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
CFLAGS += -Wall -Wextra -O2 -g
CFLAGS += -std=c99

# 包含目录
INCLUDES = -Iframework/shell/include
INCLUDES += -Ihal/include
INCLUDES += -Iinclude

# 源文件
SRCS = src/main.c
SRCS += src/hal_uart.c
SRCS += src/uart_backend.c
SRCS += framework/shell/src/shell.c
SRCS += framework/shell/src/shell_command.c
SRCS += framework/shell/src/shell_line_editor.c
SRCS += framework/shell/src/shell_history.c
SRCS += framework/shell/src/shell_parser.c
SRCS += framework/shell/src/shell_autocomplete.c
SRCS += framework/shell/src/shell_backend.c
SRCS += framework/shell/src/shell_builtin.c
SRCS += framework/shell/src/shell_uart_backend.c

# 目标文件
OBJS = $(SRCS:.c=.o)

# 链接脚本
LDSCRIPT = STM32F407VGTx_FLASH.ld

# 链接选项
LDFLAGS = -T$(LDSCRIPT)
LDFLAGS += -Wl,-Map=output.map
LDFLAGS += -specs=nano.specs -specs=nosys.specs

# 目标
TARGET = my_app

all: $(TARGET).elf $(TARGET).bin $(TARGET).hex
	$(SIZE) $(TARGET).elf

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin $(TARGET).hex output.map

.PHONY: all clean
```

### Kconfig 配置

```kconfig
# Kconfig
menu "Shell Configuration"

config SHELL_ENABLED
    bool "Enable Shell Framework"
    default y
    help
      Enable the Shell/CLI framework

if SHELL_ENABLED

config SHELL_CMD_BUFFER_SIZE
    int "Command buffer size"
    range 64 256
    default 128
    help
      Size of the command input buffer

config SHELL_HISTORY_DEPTH
    int "History depth"
    range 4 32
    default 16
    help
      Number of commands to keep in history

config SHELL_MAX_COMMANDS
    int "Maximum number of commands"
    range 8 128
    default 32
    help
      Maximum number of registered commands

config SHELL_UART_BACKEND
    bool "Enable UART backend"
    default y
    help
      Enable UART backend for Shell

config SHELL_CONSOLE_BACKEND
    bool "Enable Console backend"
    default n
    help
      Enable Console backend for Shell (Native platforms only)

endif # SHELL_ENABLED

endmenu
```

### 编译宏定义

```c
/* shell_config.h */
#ifndef SHELL_CONFIG_H
#define SHELL_CONFIG_H

/* 命令缓冲区大小 */
#ifndef SHELL_DEFAULT_CMD_BUFFER_SIZE
#define SHELL_DEFAULT_CMD_BUFFER_SIZE 128
#endif

/* 历史深度 */
#ifndef SHELL_DEFAULT_HISTORY_DEPTH
#define SHELL_DEFAULT_HISTORY_DEPTH 16
#endif

/* 最大命令数 */
#ifndef SHELL_MAX_COMMANDS
#define SHELL_MAX_COMMANDS 32
#endif

/* 最大参数数 */
#ifndef SHELL_MAX_ARGS
#define SHELL_MAX_ARGS 8
#endif

/* 默认提示符 */
#ifndef SHELL_DEFAULT_PROMPT
#define SHELL_DEFAULT_PROMPT "$ "
#endif

/* 启用 UART 后端 */
#ifndef SHELL_ENABLE_UART_BACKEND
#define SHELL_ENABLE_UART_BACKEND 1
#endif

/* 启用 Console 后端 */
#ifndef SHELL_ENABLE_CONSOLE_BACKEND
#define SHELL_ENABLE_CONSOLE_BACKEND 0
#endif

#endif /* SHELL_CONFIG_H */
```

## 移植步骤

### 步骤 1：准备工作

1. **评估平台**：
   - 确认 CPU 架构和编译器
   - 检查内存资源（RAM ≥ 4KB，Flash ≥ 10KB）
   - 确认是否有 UART 或其他通信接口

2. **准备开发环境**：
   - 安装交叉编译工具链
   - 配置调试工具（JTAG/SWD）
   - 准备串口工具（minicom、PuTTY 等）

3. **获取源代码**：
   ```bash
   git clone https://github.com/nexus/shell.git
   cd shell
   ```

### 步骤 2：实现 HAL 接口

创建 `hal_uart.c` 文件：

```c
/**
 * \file            hal_uart.c
 * \brief           HAL UART implementation for [Platform]
 * \author          Your Name
 * \version         1.0.0
 * \date            2026-01-24
 *
 * \copyright       Copyright (c) 2026 Your Company
 */

#include "hal/hal_uart.h"
/* 包含平台特定的头文件 */

/**
 * \brief           Initialize UART
 */
hal_status_t hal_uart_init(hal_uart_t uart, const hal_uart_config_t* config) {
    /* TODO: 实现 UART 初始化 */
    /* 1. 配置 GPIO 引脚 */
    /* 2. 配置 UART 参数（波特率、数据位等） */
    /* 3. 使能 UART */
    
    return HAL_OK;
}

/**
 * \brief           Deinitialize UART
 */
hal_status_t hal_uart_deinit(hal_uart_t uart) {
    /* TODO: 实现 UART 反初始化 */
    /* 1. 禁用 UART */
    /* 2. 释放资源 */
    
    return HAL_OK;
}

/**
 * \brief           Read from UART (non-blocking)
 */
int hal_uart_read(hal_uart_t uart, uint8_t* buf, size_t len) {
    /* TODO: 实现非阻塞读取 */
    /* 1. 检查是否有数据 */
    /* 2. 读取数据到缓冲区 */
    /* 3. 返回实际读取的字节数 */
    
    return 0;  /* 无数据时返回 0 */
}

/**
 * \brief           Write to UART
 */
int hal_uart_write(hal_uart_t uart, const uint8_t* buf, size_t len) {
    /* TODO: 实现写入 */
    /* 1. 发送数据 */
    /* 2. 等待发送完成（可选） */
    /* 3. 返回实际写入的字节数 */
    
    return len;
}

/**
 * \brief           Get available bytes in UART RX buffer
 */
int hal_uart_available(hal_uart_t uart) {
    /* TODO: 实现可用字节数查询 */
    /* 返回接收缓冲区中的字节数 */
    
    return 0;
}
```

### 步骤 3：实现后端

创建 `uart_backend.c` 文件：

```c
/**
 * \file            uart_backend.c
 * \brief           UART backend for Shell
 * \author          Your Name
 */

#include "shell/shell_backend.h"
#include "hal/hal_uart.h"

/**
 * \brief           UART backend read function
 */
static int uart_backend_read(char* buf, size_t len) {
    if (hal_uart_available(HAL_UART_0) > 0) {
        return hal_uart_read(HAL_UART_0, (uint8_t*)buf, 1);
    }
    return 0;
}

/**
 * \brief           UART backend write function
 */
static int uart_backend_write(const char* buf, size_t len) {
    return hal_uart_write(HAL_UART_0, (const uint8_t*)buf, len);
}

/**
 * \brief           UART backend instance
 */
const shell_backend_t shell_uart_backend = {
    .read = uart_backend_read,
    .write = uart_backend_write
};
```

### 步骤 4：编写应用代码

创建 `main.c` 文件：

```c
/**
 * \file            main.c
 * \brief           Shell application main
 * \author          Your Name
 */

#include "shell/shell.h"
#include "hal/hal_uart.h"

/* 自定义命令示例 */
static int cmd_hello(int argc, char* argv[]) {
    shell_printf("Hello from [Platform]!\r\n");
    return 0;
}

static const shell_command_t hello_cmd = {
    .name = "hello",
    .handler = cmd_hello,
    .help = "Print hello message",
    .usage = "hello"
};

int main(void) {
    /* 1. 系统初始化 */
    SystemInit();  /* 平台特定 */
    
    /* 2. 初始化 UART */
    hal_uart_config_t uart_cfg = {
        .baudrate = 115200,
        .wordlen = HAL_UART_WORDLEN_8,
        .stopbits = HAL_UART_STOPBITS_1,
        .parity = HAL_UART_PARITY_NONE
    };
    hal_uart_init(HAL_UART_0, &uart_cfg);
    
    /* 3. 初始化 Shell */
    shell_config_t shell_cfg = SHELL_CONFIG_DEFAULT;
    shell_init(&shell_cfg);
    
    /* 4. 设置后端 */
    shell_set_backend(&shell_uart_backend);
    
    /* 5. 注册命令 */
    shell_register_builtin_commands();
    shell_register_command(&hello_cmd);
    
    /* 6. 打印欢迎信息 */
    shell_printf("\r\n");
    shell_printf("=================================\r\n");
    shell_printf("  Shell on [Platform]\r\n");
    shell_printf("  Version: %s\r\n", shell_get_version());
    shell_printf("=================================\r\n");
    shell_printf("\r\n");
    shell_print_prompt();
    
    /* 7. 主循环 */
    while (1) {
        /* 处理 Shell 输入 */
        shell_process();
        
        /* 其他任务处理 */
        /* ... */
    }
    
    return 0;
}
```

### 步骤 5：编译和链接

```bash
# 使用 CMake
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake ..
make

# 或使用 Makefile
make

# 检查生成的文件
ls -lh my_app.elf my_app.bin
```

### 步骤 6：烧录和测试

```bash
# 烧录到目标板（示例）
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
    -c "program my_app.elf verify reset exit"

# 连接串口
minicom -D /dev/ttyUSB0 -b 115200

# 或使用 screen
screen /dev/ttyUSB0 115200
```

### 步骤 7：功能验证

在串口终端中测试：

```
=================================
  Shell on STM32F407
  Version: 1.0.0
=================================

nexus> help
Available commands:
  hello    - Print hello message
  help     - Show available commands
  version  - Show shell version
  clear    - Clear terminal screen
  history  - Show command history
  echo     - Print arguments

nexus> hello
Hello from STM32F407!

nexus> version
Shell version: 1.0.0

nexus> echo test 123
test 123

nexus> 
```

## 平台特定优化

### 内存优化

#### 1. 减小缓冲区大小

```c
shell_config_t config = {
    .prompt = "$ ",
    .cmd_buffer_size = 64,   /* 减小到 64 字节 */
    .history_depth = 4,      /* 减小到 4 条 */
    .max_commands = 16       /* 减小到 16 个命令 */
};
```

#### 2. 使用静态分配（未来功能）

```c
#define SHELL_USE_STATIC_ALLOC 1

/* 静态缓冲区 */
static char g_cmd_buffer[64];
static char g_history_storage[4 * 64];
static char* g_history_entries[4];

/* 初始化时使用静态缓冲区 */
shell_init_static(&config, g_cmd_buffer, g_history_storage, g_history_entries);
```

#### 3. 禁用不需要的功能

```c
/* 禁用历史功能 */
#define SHELL_ENABLE_HISTORY 0

/* 禁用自动补全 */
#define SHELL_ENABLE_AUTOCOMPLETE 0

/* 禁用内置命令 */
#define SHELL_ENABLE_BUILTIN_COMMANDS 0
```

### 性能优化

#### 1. 提高 UART 波特率

```c
hal_uart_config_t uart_cfg = {
    .baudrate = 921600,  /* 从 115200 提高到 921600 */
    /* ... */
};
```

#### 2. 使用 DMA 传输

```c
/* 启用 UART DMA 接收 */
void uart_dma_init(void) {
    /* 配置 DMA */
    /* ... */
    
    /* 启动 DMA 接收 */
    HAL_UART_Receive_DMA(&huart1, rx_buffer, sizeof(rx_buffer));
}

/* DMA 接收完成回调 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart) {
    /* 处理接收到的数据 */
    /* ... */
}
```

#### 3. 优化 shell_process 调用频率

```c
/* 方式 1：定时器中断 */
void TIM_IRQHandler(void) {
    shell_process();
}

/* 方式 2：UART 接收中断 */
void UART_IRQHandler(void) {
    if (uart_rx_ready) {
        shell_process();
    }
}

/* 方式 3：RTOS 任务 */
void shell_task(void* param) {
    while (1) {
        shell_process();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

### 功耗优化

#### 1. 低功耗模式

```c
int main(void) {
    /* 初始化 */
    /* ... */
    
    while (1) {
        /* 处理 Shell */
        shell_process();
        
        /* 如果无输入，进入低功耗模式 */
        if (!uart_has_data()) {
            __WFI();  /* 等待中断 */
        }
    }
}
```

#### 2. 动态调整波特率

```c
/* 空闲时降低波特率 */
void enter_low_power_mode(void) {
    hal_uart_set_baudrate(HAL_UART_0, 9600);
}

/* 活动时恢复波特率 */
void exit_low_power_mode(void) {
    hal_uart_set_baudrate(HAL_UART_0, 115200);
}
```

## 验证清单

### 功能验证

- [ ] Shell 初始化成功
- [ ] 能够输入字符并回显
- [ ] 能够执行内置命令（help、version 等）
- [ ] 能够执行自定义命令
- [ ] 光标移动功能正常（左右、Home、End）
- [ ] 字符编辑功能正常（插入、删除、Backspace）
- [ ] 历史功能正常（上下箭头）
- [ ] 自动补全功能正常（Tab 键）
- [ ] 参数解析正确（引号、转义字符）
- [ ] 错误处理正确（未知命令、参数错误）

### 性能验证

- [ ] 命令执行延迟 < 10 ms
- [ ] shell_process() 调用开销 < 100 μs
- [ ] 内存使用在预期范围内（< 5 KB）
- [ ] 无内存泄漏
- [ ] 长时间运行稳定

### 兼容性验证

- [ ] 支持常用终端（minicom、PuTTY、screen）
- [ ] 转义序列正确处理
- [ ] 不同波特率下工作正常
- [ ] 支持不同的换行符（\r、\n、\r\n）

## 故障排查

### 问题 1：无法编译

**症状**：编译错误，找不到头文件或符号未定义。

**解决方案**：
1. 检查包含路径是否正确
2. 确认所有源文件都已添加到编译列表
3. 检查编译器版本和标准（需要 C99）

### 问题 2：Shell 无响应

**症状**：输入字符没有回显。

**解决方案**：
1. 检查 UART 是否正确初始化
2. 检查后端是否正确设置
3. 确认 shell_process() 被周期性调用
4. 使用示波器或逻辑分析仪检查 UART 信号

### 问题 3：输出乱码

**症状**：Shell 输出显示乱码。

**解决方案**：
1. 检查波特率是否匹配
2. 检查数据位、停止位、校验位配置
3. 检查终端编码设置（UTF-8）

### 问题 4：内存不足

**症状**：初始化失败，返回 SHELL_ERROR_NO_MEMORY。

**解决方案**：
1. 减小缓冲区大小
2. 减小历史深度
3. 使用静态分配模式
4. 增加堆大小

### 问题 5：历史或补全不工作

**症状**：按上下箭头或 Tab 键没有反应。

**解决方案**：
1. 检查终端是否支持转义序列
2. 使用十六进制查看器检查实际发送的字符
3. 确认转义序列处理代码正确

## 示例项目

### STM32F4 完整示例

项目结构：
```
stm32f4-shell/
├── CMakeLists.txt
├── STM32F407VGTx_FLASH.ld
├── framework/
│   └── shell/
├── hal/
│   ├── include/
│   │   └── hal_uart.h
│   └── src/
│       └── hal_uart_stm32.c
├── src/
│   ├── main.c
│   ├── system_stm32f4xx.c
│   ├── startup_stm32f407xx.s
│   └── uart_backend.c
└── include/
    └── stm32f4xx.h
```

完整代码请参考：`examples/stm32f4-shell/`

### ESP32 完整示例

项目结构：
```
esp32-shell/
├── CMakeLists.txt
├── sdkconfig
├── main/
│   ├── CMakeLists.txt
│   ├── main.c
│   └── uart_backend.c
└── components/
    └── shell/
```

完整代码请参考：`examples/esp32-shell/`

### Linux 完整示例

项目结构：
```
linux-shell/
├── CMakeLists.txt
├── Makefile
├── src/
│   ├── main.c
│   └── console_backend.c
└── framework/
    └── shell/
```

完整代码请参考：`examples/linux-shell/`

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**维护者**: Nexus Team
