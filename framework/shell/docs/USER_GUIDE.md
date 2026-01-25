# Shell Framework 使用指南

本文档提供 Nexus Shell Framework 的详细使用指南，包括快速开始、基本操作、高级功能和最佳实践。

## 目录

1. [快速开始](#快速开始)
2. [基本操作](#基本操作)
3. [命令管理](#命令管理)
4. [行编辑功能](#行编辑功能)
5. [历史管理](#历史管理)
6. [自动补全](#自动补全)
7. [后端配置](#后端配置)
8. [参数解析](#参数解析)
9. [输出函数](#输出函数)
10. [高级功能](#高级功能)
11. [最佳实践](#最佳实践)
12. [常见问题](#常见问题)

## 快速开始

### 最小示例

这是一个最简单的 Shell 应用示例：

```c
#include "shell/shell.h"
#include "hal/hal_uart.h"

/* 自定义命令处理函数 */
static int cmd_hello(int argc, char* argv[]) {
    shell_printf("Hello, Nexus Shell!\r\n");
    return 0;
}

/* 命令定义 */
static const shell_command_t hello_cmd = {
    .name = "hello",
    .handler = cmd_hello,
    .help = "Print hello message",
    .usage = "hello"
};

int main(void) {
    /* 1. 初始化 UART */
    hal_uart_config_t uart_cfg = {
        .baudrate = 115200,
        .wordlen = HAL_UART_WORDLEN_8,
        .stopbits = HAL_UART_STOPBITS_1,
        .parity = HAL_UART_PARITY_NONE
    };
    hal_uart_init(HAL_UART_0, &uart_cfg);
    
    /* 2. 初始化 Shell（使用默认配置） */
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    /* 3. 设置 UART 后端 */
    shell_set_backend(&shell_uart_backend);
    
    /* 4. 注册内置命令 */
    shell_register_builtin_commands();
    
    /* 5. 注册自定义命令 */
    shell_register_command(&hello_cmd);
    
    /* 6. 打印欢迎信息和提示符 */
    shell_printf("\r\nNexus Shell v%s\r\n", shell_get_version());
    shell_printf("Type 'help' for available commands\r\n\r\n");
    shell_print_prompt();
    
    /* 7. 主循环 */
    while (1) {
        /* 处理 Shell 输入（非阻塞） */
        shell_process();
        
        /* 其他任务处理 */
        /* ... */
    }
    
    return 0;
}
```

### 编译和运行

#### CMake 配置

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(my_shell_app)

# 添加 Shell 框架
add_subdirectory(framework/shell)

# 创建可执行文件
add_executable(my_app
    src/main.c
)

# 链接 Shell 库
target_link_libraries(my_app
    PRIVATE
        shell
        hal
        osal
)
```

#### 编译

```bash
mkdir build
cd build
cmake ..
make
```

#### 运行

```bash
# 在目标设备上运行
./my_app

# 或通过串口连接
minicom -D /dev/ttyUSB0 -b 115200
```

### 第一次交互

```
Nexus Shell v1.0.0
Type 'help' for available commands

nexus> help
Available commands:
  hello    - Print hello message
  help     - Show available commands
  version  - Show shell version
  clear    - Clear terminal screen
  history  - Show command history
  echo     - Print arguments

nexus> hello
Hello, Nexus Shell!

nexus> version
Shell version: 1.0.0

nexus> 
```

## 基本操作

### 初始化和配置

#### 使用默认配置

```c
#include "shell/shell.h"

void app_init(void) {
    /* 使用默认配置 */
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    
    shell_status_t status = shell_init(&config);
    if (status != SHELL_OK) {
        printf("Shell init failed: %d\n", status);
        return;
    }
    
    /* 配置后端和注册命令 */
    shell_set_backend(&shell_uart_backend);
    shell_register_builtin_commands();
}
```

#### 自定义配置

```c
void app_init(void) {
    shell_config_t config = {
        .prompt = "myapp> ",        /* 自定义提示符 */
        .cmd_buffer_size = 256,     /* 命令缓冲区 256 字节 */
        .history_depth = 32,        /* 保存 32 条历史 */
        .max_commands = 64          /* 最多 64 个命令 */
    };
    
    shell_init(&config);
    shell_set_backend(&shell_uart_backend);
    shell_register_builtin_commands();
}
```

#### 配置参数说明

| 参数 | 类型 | 范围 | 默认值 | 说明 |
|------|------|------|--------|------|
| `prompt` | `const char*` | 最多 16 字符 | `"nexus> "` | 提示符字符串 |
| `cmd_buffer_size` | `uint16_t` | 64-256 | 128 | 命令缓冲区大小 |
| `history_depth` | `uint8_t` | 4-32 | 16 | 历史记录深度 |
| `max_commands` | `uint8_t` | 1-255 | 32 | 最大命令数量 |

### 反初始化

```c
void app_cleanup(void) {
    /* 反初始化 Shell */
    shell_status_t status = shell_deinit();
    if (status != SHELL_OK) {
        printf("Shell deinit failed: %d\n", status);
    }
}
```

**注意**：反初始化会释放所有分配的内存，包括命令缓冲区和历史存储。

### 检查初始化状态

```c
void check_shell_status(void) {
    if (shell_is_initialized()) {
        printf("Shell is initialized\n");
    } else {
        printf("Shell is not initialized\n");
    }
}
```

### 主循环处理

```c
/* 方式 1：裸机主循环 */
int main(void) {
    app_init();
    
    while (1) {
        /* 处理 Shell 输入（非阻塞） */
        shell_process();
        
        /* 处理其他任务 */
        process_other_tasks();
        
        /* 可选：低功耗模式 */
        /* __WFI(); */
    }
}

/* 方式 2：RTOS 任务 */
void shell_task(void* param) {
    app_init();
    
    while (1) {
        shell_process();
        
        /* 让出 CPU */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* 方式 3：定时器回调 */
void timer_callback(void* param) {
    /* 在定时器中周期性调用 */
    shell_process();
}
```

**关键点**：
- `shell_process()` 是非阻塞的，立即返回
- 建议在主循环或定时器中周期性调用
- 调用频率建议 10-100 Hz

## 命令管理

### 命令结构

```c
typedef struct {
    const char* name;               /* 命令名称 */
    shell_cmd_handler_t handler;    /* 处理函数 */
    const char* help;               /* 帮助文本 */
    const char* usage;              /* 用法说明 */
    shell_completion_t completion;  /* 自动补全函数（可选） */
} shell_command_t;
```

### 命令处理函数

```c
typedef int (*shell_cmd_handler_t)(int argc, char* argv[]);
```

**参数**：
- `argc`：参数数量（包括命令名本身）
- `argv`：参数数组，`argv[0]` 是命令名

**返回值**：
- `0`：成功
- 非 0：错误码

### 注册简单命令

```c
/* 命令处理函数 */
static int cmd_reboot(int argc, char* argv[]) {
    shell_printf("Rebooting system...\r\n");
    
    /* 延迟一下让消息输出 */
    for (volatile int i = 0; i < 1000000; i++);
    
    /* 重启系统 */
    NVIC_SystemReset();
    
    return 0;
}

/* 命令定义 */
static const shell_command_t reboot_cmd = {
    .name = "reboot",
    .handler = cmd_reboot,
    .help = "Reboot the system",
    .usage = "reboot"
};

/* 注册命令 */
void register_commands(void) {
    shell_register_command(&reboot_cmd);
}
```

### 注册带参数的命令

```c
/* LED 控制命令 */
static int cmd_led(int argc, char* argv[]) {
    /* 检查参数数量 */
    if (argc < 2) {
        shell_printf("Usage: %s\r\n", "led <on|off|toggle|status>");
        return -1;
    }
    
    /* 解析参数 */
    if (strcmp(argv[1], "on") == 0) {
        hal_gpio_write(LED_PIN, 1);
        shell_printf("LED turned on\r\n");
    } else if (strcmp(argv[1], "off") == 0) {
        hal_gpio_write(LED_PIN, 0);
        shell_printf("LED turned off\r\n");
    } else if (strcmp(argv[1], "toggle") == 0) {
        hal_gpio_toggle(LED_PIN);
        shell_printf("LED toggled\r\n");
    } else if (strcmp(argv[1], "status") == 0) {
        int state = hal_gpio_read(LED_PIN);
        shell_printf("LED is %s\r\n", state ? "on" : "off");
    } else {
        shell_printf("Unknown option: %s\r\n", argv[1]);
        return -1;
    }
    
    return 0;
}

static const shell_command_t led_cmd = {
    .name = "led",
    .handler = cmd_led,
    .help = "Control LED state",
    .usage = "led <on|off|toggle|status>"
};
```

### 注册多参数命令

```c
/* GPIO 配置命令 */
static int cmd_gpio(int argc, char* argv[]) {
    /* gpio <pin> <mode> [value] */
    
    if (argc < 3) {
        shell_printf("Usage: gpio <pin> <mode> [value]\r\n");
        shell_printf("  mode: input, output, pullup, pulldown\r\n");
        shell_printf("  value: 0 or 1 (for output mode)\r\n");
        return -1;
    }
    
    /* 解析引脚号 */
    int pin = atoi(argv[1]);
    if (pin < 0 || pin > 31) {
        shell_printf("Invalid pin number: %d\r\n", pin);
        return -1;
    }
    
    /* 解析模式 */
    const char* mode = argv[2];
    
    if (strcmp(mode, "input") == 0) {
        hal_gpio_set_mode(pin, GPIO_MODE_INPUT);
        shell_printf("GPIO%d set to input mode\r\n", pin);
    } else if (strcmp(mode, "output") == 0) {
        hal_gpio_set_mode(pin, GPIO_MODE_OUTPUT);
        
        /* 如果提供了初始值 */
        if (argc >= 4) {
            int value = atoi(argv[3]);
            hal_gpio_write(pin, value);
            shell_printf("GPIO%d set to output mode, value=%d\r\n", pin, value);
        } else {
            shell_printf("GPIO%d set to output mode\r\n", pin);
        }
    } else if (strcmp(mode, "pullup") == 0) {
        hal_gpio_set_mode(pin, GPIO_MODE_INPUT_PULLUP);
        shell_printf("GPIO%d set to input with pull-up\r\n", pin);
    } else if (strcmp(mode, "pulldown") == 0) {
        hal_gpio_set_mode(pin, GPIO_MODE_INPUT_PULLDOWN);
        shell_printf("GPIO%d set to input with pull-down\r\n", pin);
    } else {
        shell_printf("Unknown mode: %s\r\n", mode);
        return -1;
    }
    
    return 0;
}

static const shell_command_t gpio_cmd = {
    .name = "gpio",
    .handler = cmd_gpio,
    .help = "Configure GPIO pin",
    .usage = "gpio <pin> <mode> [value]"
};
```

### 批量注册命令

```c
/* 定义多个命令 */
static const shell_command_t system_commands[] = {
    {
        .name = "reboot",
        .handler = cmd_reboot,
        .help = "Reboot the system",
        .usage = "reboot"
    },
    {
        .name = "reset",
        .handler = cmd_reset,
        .help = "Reset configuration",
        .usage = "reset"
    },
    {
        .name = "info",
        .handler = cmd_info,
        .help = "Show system information",
        .usage = "info"
    },
    {
        .name = "uptime",
        .handler = cmd_uptime,
        .help = "Show system uptime",
        .usage = "uptime"
    }
};

/* 批量注册 */
void register_system_commands(void) {
    for (size_t i = 0; i < sizeof(system_commands) / sizeof(system_commands[0]); i++) {
        shell_status_t status = shell_register_command(&system_commands[i]);
        if (status != SHELL_OK) {
            printf("Failed to register command: %s\n", system_commands[i].name);
        }
    }
}
```

### 注销命令

```c
void unregister_command_example(void) {
    /* 注销单个命令 */
    shell_status_t status = shell_unregister_command("led");
    if (status == SHELL_OK) {
        shell_printf("Command 'led' unregistered\r\n");
    } else {
        shell_printf("Failed to unregister command\r\n");
    }
}
```

### 查找命令

```c
void check_command_exists(const char* name) {
    const shell_command_t* cmd = shell_find_command(name);
    if (cmd != NULL) {
        shell_printf("Command '%s' exists\r\n", name);
        shell_printf("  Help: %s\r\n", cmd->help);
        shell_printf("  Usage: %s\r\n", cmd->usage);
    } else {
        shell_printf("Command '%s' not found\r\n", name);
    }
}
```

### 获取命令数量

```c
void show_command_count(void) {
    uint8_t count = shell_get_command_count();
    shell_printf("Total commands: %d\r\n", count);
}
```


## 行编辑功能

Shell Framework 提供丰富的行编辑功能，支持光标移动、字符编辑和快捷键操作。

### 光标移动

| 按键 | 功能 | 说明 |
|------|------|------|
| `←` | 光标左移 | 移动到前一个字符 |
| `→` | 光标右移 | 移动到后一个字符 |
| `Home` | 移到行首 | 光标移到行首 |
| `End` | 移到行尾 | 光标移到行尾 |
| `Ctrl+A` | 移到行首 | 同 Home |
| `Ctrl+E` | 移到行尾 | 同 End |

**示例**：

```
nexus> hello world
       ^     ^
       |     光标在这里，按 Home
       光标移到这里
```

### 字符编辑

| 按键 | 功能 | 说明 |
|------|------|------|
| `Backspace` | 删除前一字符 | 删除光标前的字符 |
| `Delete` | 删除当前字符 | 删除光标位置的字符 |
| `Ctrl+D` | 删除当前字符 | 同 Delete |

**示例**：

```
/* 删除字符 */
nexus> helllo world
          ^
          光标在这里，按 Backspace
          
nexus> hello world
         ^
         删除了一个 'l'
```

### 行操作

| 按键 | 功能 | 说明 |
|------|------|------|
| `Ctrl+K` | 删除到行尾 | 删除从光标到行尾的所有字符 |
| `Ctrl+U` | 删除到行首 | 删除从行首到光标的所有字符 |
| `Ctrl+W` | 删除单词 | 删除光标前的一个单词 |
| `Ctrl+C` | 取消输入 | 取消当前输入，清空缓冲区 |
| `Ctrl+L` | 清屏 | 清除终端屏幕 |

**示例**：

```
/* Ctrl+K - 删除到行尾 */
nexus> hello world
       ^
       光标在这里，按 Ctrl+K
       
nexus> hello
       ^
       删除了 " world"

/* Ctrl+U - 删除到行首 */
nexus> hello world
             ^
             光标在这里，按 Ctrl+U
             
nexus> world
       ^
       删除了 "hello "

/* Ctrl+W - 删除单词 */
nexus> hello world test
                   ^
                   光标在这里，按 Ctrl+W
                   
nexus> hello world 
               ^
               删除了 "test"
```

### 字符插入

Shell 支持在任意位置插入字符：

```
nexus> helo world
        ^
        光标在这里，输入 'l'
        
nexus> hello world
         ^
         插入了 'l'，光标右移
```

**关键特性**：
- 支持在行中间插入字符
- 自动移动后面的字符
- 实时刷新显示

### 行编辑示例

完整的编辑过程示例：

```
/* 1. 输入命令 */
nexus> led on

/* 2. 发现错误，按 Home 回到行首 */
nexus> led on
       ^

/* 3. 按 → 移动到 'led' 后面 */
nexus> led on
          ^

/* 4. 输入空格和 'gpio' */
nexus> led gpio on
               ^

/* 5. 按 End 移到行尾 */
nexus> led gpio on
                  ^

/* 6. 按 Backspace 删除 'on' */
nexus> led gpio 
               ^

/* 7. 输入 'off' */
nexus> led gpio off
                   ^

/* 8. 按 Enter 执行 */
```

## 历史管理

Shell Framework 提供命令历史功能，可以浏览和重复执行之前的命令。

### 历史浏览

| 按键 | 功能 | 说明 |
|------|------|------|
| `↑` | 上一条历史 | 浏览更早的命令 |
| `↓` | 下一条历史 | 浏览更新的命令 |
| `Ctrl+P` | 上一条历史 | 同 ↑ |
| `Ctrl+N` | 下一条历史 | 同 ↓ |

**示例**：

```
/* 执行几条命令 */
nexus> led on
LED turned on

nexus> gpio 5 output 1
GPIO5 set to output mode, value=1

nexus> info
System: Nexus v1.0.0

/* 按 ↑ 浏览历史 */
nexus> info          /* 最新的命令 */

/* 再按 ↑ */
nexus> gpio 5 output 1

/* 再按 ↑ */
nexus> led on        /* 最旧的命令 */

/* 按 ↓ 向前浏览 */
nexus> gpio 5 output 1

/* 按 ↓ */
nexus> info

/* 按 ↓ 回到空输入 */
nexus> 
```

### 历史配置

```c
/* 配置历史深度 */
shell_config_t config = {
    .prompt = "nexus> ",
    .cmd_buffer_size = 128,
    .history_depth = 32,    /* 保存 32 条历史 */
    .max_commands = 32
};

shell_init(&config);
```

**历史深度范围**：4-32 条

### 历史去重

Shell 自动去除重复的历史记录：

```
nexus> led on
LED turned on

nexus> led on        /* 重复命令 */
LED turned on

nexus> led off
LED turned off

/* 按 ↑ 浏览历史 */
nexus> led off       /* 最新 */

/* 按 ↑ */
nexus> led on        /* 只保存一次 'led on' */
```

### 历史持久化（未来功能）

当前版本历史记录在内存中，重启后丢失。未来版本将支持历史持久化：

```c
/* 保存历史到文件 */
history_save("/data/shell_history");

/* 从文件加载历史 */
history_load("/data/shell_history");
```

### 查看历史记录

使用内置的 `history` 命令查看历史：

```
nexus> history
  1: led on
  2: gpio 5 output 1
  3: info
  4: reboot
  5: history

nexus> 
```

### 编程方式访问历史

```c
/* 获取历史管理器 */
history_manager_t* hist = shell_get_history_manager();

if (hist != NULL) {
    /* 添加历史记录 */
    history_add(hist, "custom command");
    
    /* 获取历史数量 */
    uint8_t count = history_get_count(hist);
    shell_printf("History count: %d\r\n", count);
    
    /* 遍历历史 */
    for (uint8_t i = 0; i < count; i++) {
        const char* cmd = history_get_by_index(hist, i);
        if (cmd != NULL) {
            shell_printf("%d: %s\r\n", i + 1, cmd);
        }
    }
    
    /* 清空历史 */
    history_clear(hist);
}
```

## 自动补全

Shell Framework 支持 Tab 键自动补全命令名。

### 基本补全

```
/* 输入部分命令名，按 Tab */
nexus> le<Tab>
led

/* 自动补全并添加空格 */
nexus> led 
```

### 多个匹配

当有多个匹配时，显示所有选项：

```
/* 输入 'h'，按 Tab */
nexus> h<Tab>
help  history

/* 完成公共前缀 */
nexus> h
```

### 无匹配

当没有匹配时，不做任何操作：

```
nexus> xyz<Tab>
nexus> xyz        /* 无变化 */
```

### 自定义补全函数

为命令添加参数补全功能：

```c
/* LED 命令的补全函数 */
static int led_completion(const char* partial, char* completions[], int max) {
    /* 可补全的选项 */
    const char* options[] = {"on", "off", "toggle", "status"};
    int count = 0;
    
    /* 匹配前缀 */
    for (int i = 0; i < 4 && count < max; i++) {
        if (strncmp(options[i], partial, strlen(partial)) == 0) {
            completions[count++] = (char*)options[i];
        }
    }
    
    return count;
}

/* 注册带补全的命令 */
static const shell_command_t led_cmd = {
    .name = "led",
    .handler = cmd_led,
    .help = "Control LED state",
    .usage = "led <on|off|toggle|status>",
    .completion = led_completion    /* 补全函数 */
};
```

**使用效果**：

```
nexus> led <Tab>
on  off  toggle  status

nexus> led o<Tab>
on  off

nexus> led of<Tab>
off

nexus> led off 
```

### 文件名补全（未来功能）

未来版本将支持文件名和路径补全：

```c
/* 文件操作命令的补全 */
static int file_completion(const char* partial, char* completions[], int max) {
    /* 列出匹配的文件 */
    return list_files(partial, completions, max);
}

static const shell_command_t cat_cmd = {
    .name = "cat",
    .handler = cmd_cat,
    .help = "Display file content",
    .usage = "cat <filename>",
    .completion = file_completion
};
```

## 后端配置

Shell Framework 支持多种 I/O 后端，可以根据需要选择或自定义。

### UART 后端

UART 后端用于通过串口与 Shell 交互：

```c
#include "shell/shell.h"
#include "shell/shell_backend.h"
#include "hal/hal_uart.h"

void setup_uart_backend(void) {
    /* 1. 初始化 UART 硬件 */
    hal_uart_config_t uart_cfg = {
        .baudrate = 115200,
        .wordlen = HAL_UART_WORDLEN_8,
        .stopbits = HAL_UART_STOPBITS_1,
        .parity = HAL_UART_PARITY_NONE,
        .flowctrl = HAL_UART_FLOWCTRL_NONE
    };
    
    hal_status_t status = hal_uart_init(HAL_UART_0, &uart_cfg);
    if (status != HAL_OK) {
        printf("UART init failed\n");
        return;
    }
    
    /* 2. 设置 Shell 后端 */
    shell_set_backend(&shell_uart_backend);
    
    printf("UART backend configured\n");
}
```

**UART 后端特点**：
- 适用于嵌入式目标板
- 通过串口工具（minicom、PuTTY 等）连接
- 支持硬件流控（可选）

### Console 后端

Console 后端用于在 PC 上测试 Shell：

```c
#include "shell/shell.h"
#include "shell/shell_backend.h"

void setup_console_backend(void) {
    /* 设置 Console 后端 */
    shell_set_backend(&shell_console_backend);
    
    printf("Console backend configured\n");
}
```

**Console 后端特点**：
- 适用于 Native 平台（Linux、Windows、macOS）
- 使用标准输入输出
- 便于开发和测试

### Mock 后端（测试用）

Mock 后端用于单元测试：

```c
#include "shell/shell_mock_backend.h"

void test_shell_command(void) {
    /* 设置输入 */
    const char* input = "led on\r";
    char output[256] = {0};
    
    /* 配置 Mock 后端 */
    shell_mock_backend_set_input(input);
    shell_mock_backend_set_output(output, sizeof(output));
    shell_set_backend(&shell_mock_backend);
    
    /* 处理输入 */
    while (*input) {
        shell_process();
    }
    
    /* 检查输出 */
    if (strstr(output, "LED turned on") != NULL) {
        printf("Test passed\n");
    } else {
        printf("Test failed\n");
    }
}
```

**Mock 后端特点**：
- 适用于单元测试
- 可以预设输入
- 可以捕获输出
- 不依赖硬件

### 自定义后端

实现自定义后端：

```c
/* 自定义后端上下文 */
typedef struct {
    /* 自定义数据 */
    int fd;
    uint8_t buffer[256];
    size_t buffer_pos;
} my_backend_ctx_t;

static my_backend_ctx_t g_my_ctx;

/* 读取函数 */
static int my_backend_read(char* buf, size_t len) {
    /* 实现读取逻辑 */
    /* 返回实际读取的字节数 */
    
    /* 示例：从文件描述符读取 */
    ssize_t n = read(g_my_ctx.fd, buf, len);
    return (n > 0) ? (int)n : 0;
}

/* 写入函数 */
static int my_backend_write(const char* buf, size_t len) {
    /* 实现写入逻辑 */
    /* 返回实际写入的字节数 */
    
    /* 示例：写入到文件描述符 */
    ssize_t n = write(g_my_ctx.fd, buf, len);
    return (n > 0) ? (int)n : 0;
}

/* 后端定义 */
static const shell_backend_t my_backend = {
    .read = my_backend_read,
    .write = my_backend_write
};

/* 使用自定义后端 */
void setup_my_backend(void) {
    /* 初始化上下文 */
    g_my_ctx.fd = open("/dev/ttyS1", O_RDWR | O_NONBLOCK);
    g_my_ctx.buffer_pos = 0;
    
    /* 设置后端 */
    shell_set_backend(&my_backend);
}
```

**自定义后端要求**：
- `read` 函数必须是非阻塞的
- `read` 返回实际读取的字节数，无数据时返回 0
- `write` 返回实际写入的字节数
- 可以是阻塞或非阻塞

### 切换后端

可以在运行时切换后端：

```c
void switch_backend_example(void) {
    /* 初始使用 UART */
    shell_set_backend(&shell_uart_backend);
    
    /* 运行一段时间... */
    
    /* 切换到 Console */
    shell_set_backend(&shell_console_backend);
    
    /* 继续运行... */
}
```

### 获取当前后端

```c
void check_current_backend(void) {
    const shell_backend_t* backend = shell_get_backend();
    
    if (backend == &shell_uart_backend) {
        shell_printf("Current backend: UART\r\n");
    } else if (backend == &shell_console_backend) {
        shell_printf("Current backend: Console\r\n");
    } else if (backend == NULL) {
        shell_printf("No backend configured\r\n");
    } else {
        shell_printf("Current backend: Custom\r\n");
    }
}
```

## 参数解析

Shell Framework 提供强大的参数解析功能，支持引号、转义字符等。

### 基本参数解析

命令行自动分割为参数数组：

```c
static int cmd_test(int argc, char* argv[]) {
    shell_printf("argc = %d\r\n", argc);
    
    for (int i = 0; i < argc; i++) {
        shell_printf("argv[%d] = \"%s\"\r\n", i, argv[i]);
    }
    
    return 0;
}
```

**示例**：

```
nexus> test arg1 arg2 arg3
argc = 4
argv[0] = "test"
argv[1] = "arg1"
argv[2] = "arg2"
argv[3] = "arg3"
```

### 引号处理

支持单引号和双引号：

```
nexus> test "hello world" 'foo bar'
argc = 3
argv[0] = "test"
argv[1] = "hello world"
argv[2] = "foo bar"
```

### 转义字符

支持反斜杠转义：

```
nexus> test "hello \"world\"" 'it\'s ok'
argc = 3
argv[0] = "test"
argv[1] = "hello "world""
argv[2] = "it's ok"
```

### 空格处理

多个空格被视为一个分隔符：

```
nexus> test   arg1    arg2     arg3
argc = 4
argv[0] = "test"
argv[1] = "arg1"
argv[2] = "arg2"
argv[3] = "arg3"
```

### 参数验证示例

```c
static int cmd_set(int argc, char* argv[]) {
    /* set <key> <value> */
    
    /* 检查参数数量 */
    if (argc != 3) {
        shell_printf("Usage: set <key> <value>\r\n");
        return -1;
    }
    
    const char* key = argv[1];
    const char* value = argv[2];
    
    /* 验证 key */
    if (strlen(key) == 0) {
        shell_printf("Error: key cannot be empty\r\n");
        return -1;
    }
    
    /* 验证 value */
    if (strlen(value) == 0) {
        shell_printf("Error: value cannot be empty\r\n");
        return -1;
    }
    
    /* 设置配置 */
    config_set(key, value);
    shell_printf("Set %s = %s\r\n", key, value);
    
    return 0;
}
```

### 数值参数解析

```c
static int cmd_delay(int argc, char* argv[]) {
    /* delay <milliseconds> */
    
    if (argc != 2) {
        shell_printf("Usage: delay <milliseconds>\r\n");
        return -1;
    }
    
    /* 解析整数 */
    char* endptr;
    long ms = strtol(argv[1], &endptr, 10);
    
    /* 检查转换是否成功 */
    if (*endptr != '\0') {
        shell_printf("Error: invalid number '%s'\r\n", argv[1]);
        return -1;
    }
    
    /* 检查范围 */
    if (ms < 0 || ms > 10000) {
        shell_printf("Error: delay must be 0-10000 ms\r\n");
        return -1;
    }
    
    /* 执行延迟 */
    shell_printf("Delaying %ld ms...\r\n", ms);
    hal_delay_ms((uint32_t)ms);
    shell_printf("Done\r\n");
    
    return 0;
}
```

### 十六进制参数解析

```c
static int cmd_write(int argc, char* argv[]) {
    /* write <address> <value> */
    
    if (argc != 3) {
        shell_printf("Usage: write <address> <value>\r\n");
        shell_printf("  address and value in hex (0x prefix optional)\r\n");
        return -1;
    }
    
    /* 解析地址（十六进制） */
    char* endptr;
    unsigned long addr = strtoul(argv[1], &endptr, 16);
    if (*endptr != '\0') {
        shell_printf("Error: invalid address '%s'\r\n", argv[1]);
        return -1;
    }
    
    /* 解析值（十六进制） */
    unsigned long value = strtoul(argv[2], &endptr, 16);
    if (*endptr != '\0') {
        shell_printf("Error: invalid value '%s'\r\n", argv[2]);
        return -1;
    }
    
    /* 写入内存 */
    *(volatile uint32_t*)addr = (uint32_t)value;
    shell_printf("Wrote 0x%08lX to 0x%08lX\r\n", value, addr);
    
    return 0;
}
```

**使用示例**：

```
nexus> write 0x40000000 0x12345678
Wrote 0x12345678 to 0x40000000

nexus> write 40000004 ABCD
Wrote 0x0000ABCD to 0x40000004
```


## 输出函数

Shell Framework 提供多种输出函数用于向终端输出信息。

### shell_printf

格式化输出函数，类似 printf：

```c
void example_printf(void) {
    /* 基本输出 */
    shell_printf("Hello, Shell!\r\n");
    
    /* 格式化输出 */
    int value = 42;
    shell_printf("The answer is %d\r\n", value);
    
    /* 多个参数 */
    shell_printf("x=%d, y=%d, z=%d\r\n", 1, 2, 3);
    
    /* 浮点数 */
    float temp = 25.5f;
    shell_printf("Temperature: %.1f C\r\n", temp);
    
    /* 十六进制 */
    uint32_t addr = 0x08000000;
    shell_printf("Address: 0x%08X\r\n", addr);
    
    /* 字符串 */
    const char* name = "Nexus";
    shell_printf("System: %s\r\n", name);
}
```

**支持的格式说明符**：
- `%d`, `%i` - 有符号十进制整数
- `%u` - 无符号十进制整数
- `%x`, `%X` - 十六进制整数
- `%o` - 八进制整数
- `%f` - 浮点数
- `%s` - 字符串
- `%c` - 字符
- `%p` - 指针
- `%%` - 百分号

### shell_puts

输出字符串（不自动换行）：

```c
void example_puts(void) {
    shell_puts("Hello");
    shell_puts(", ");
    shell_puts("World");
    shell_puts("\r\n");
    
    /* 输出: Hello, World */
}
```

### shell_putchar

输出单个字符：

```c
void example_putchar(void) {
    shell_putchar('H');
    shell_putchar('i');
    shell_putchar('\r');
    shell_putchar('\n');
    
    /* 输出: Hi */
}
```

### 输出表格

```c
static int cmd_list(int argc, char* argv[]) {
    shell_printf("ID   Name        Status\r\n");
    shell_printf("---  ----------  ------\r\n");
    shell_printf("%-4d %-12s %s\r\n", 1, "Task1", "Running");
    shell_printf("%-4d %-12s %s\r\n", 2, "Task2", "Stopped");
    shell_printf("%-4d %-12s %s\r\n", 3, "Task3", "Running");
    
    return 0;
}
```

**输出**：

```
nexus> list
ID   Name        Status
---  ----------  ------
1    Task1       Running
2    Task2       Stopped
3    Task3       Running
```

### 输出进度条

```c
static int cmd_download(int argc, char* argv[]) {
    shell_printf("Downloading...\r\n");
    
    for (int i = 0; i <= 100; i += 10) {
        /* 输出进度条 */
        shell_printf("\r[");
        
        int bars = i / 10;
        for (int j = 0; j < 10; j++) {
            if (j < bars) {
                shell_putchar('=');
            } else {
                shell_putchar(' ');
            }
        }
        
        shell_printf("] %d%%", i);
        
        /* 延迟 */
        hal_delay_ms(200);
    }
    
    shell_printf("\r\nDone!\r\n");
    return 0;
}
```

**输出**：

```
nexus> download
Downloading...
[==========] 100%
Done!
```

### ANSI 颜色输出（未来功能）

未来版本将支持 ANSI 颜色：

```c
/* 彩色输出 */
shell_printf_color(SHELL_COLOR_RED, "Error: %s\r\n", msg);
shell_printf_color(SHELL_COLOR_GREEN, "Success!\r\n");
shell_printf_color(SHELL_COLOR_YELLOW, "Warning: %s\r\n", warning);
```

### 输出重定向（未来功能）

未来版本将支持输出重定向：

```c
/* 重定向到文件 */
shell_redirect_output("/data/output.txt");
shell_printf("This goes to file\r\n");
shell_redirect_output(NULL);  /* 恢复到终端 */
```

## 高级功能

### 命令别名（未来功能）

未来版本将支持命令别名：

```c
/* 注册别名 */
shell_register_alias("ll", "ls -l");
shell_register_alias("la", "ls -a");

/* 使用别名 */
nexus> ll
/* 等同于 ls -l */
```

### 脚本执行（未来功能）

未来版本将支持脚本执行：

```c
/* 从字符串执行脚本 */
const char* script = 
    "led on\n"
    "delay 1000\n"
    "led off\n";

shell_execute_script(script);

/* 从文件执行脚本 */
shell_execute_file("/data/startup.sh");
```

### 管道和重定向（未来功能）

未来版本将支持管道和重定向：

```bash
# 管道
nexus> ps | grep task

# 输出重定向
nexus> info > /data/info.txt

# 输入重定向
nexus> config < /data/config.txt
```

### 环境变量（未来功能）

未来版本将支持环境变量：

```bash
# 设置变量
nexus> set PATH=/bin:/usr/bin

# 使用变量
nexus> echo $PATH
/bin:/usr/bin

# 在命令中使用
nexus> cd $HOME
```

### 条件执行（未来功能）

未来版本将支持条件执行：

```bash
# 与操作（前一个成功才执行后一个）
nexus> led on && delay 1000 && led off

# 或操作（前一个失败才执行后一个）
nexus> connect || echo "Connection failed"
```

## 最佳实践

### 命令设计原则

#### 1. 命令名称

- 使用小写字母
- 简短且有意义
- 避免特殊字符
- 保持一致性

```c
/* 好的命令名 */
"led", "gpio", "info", "reboot"

/* 不好的命令名 */
"LED", "GPIO_Control", "show-info", "reboot_system_now"
```

#### 2. 参数设计

- 参数数量尽量少（≤ 3 个）
- 使用清晰的参数名
- 提供默认值
- 支持可选参数

```c
/* 好的参数设计 */
gpio <pin> <mode> [value]
config <key> <value>

/* 不好的参数设计 */
gpio <pin> <mode> <pull> <speed> <alternate> <value>
```

#### 3. 帮助信息

- 提供清晰的帮助文本
- 包含用法说明
- 说明参数含义
- 提供示例

```c
static const shell_command_t gpio_cmd = {
    .name = "gpio",
    .handler = cmd_gpio,
    .help = "Configure GPIO pin",
    .usage = "gpio <pin> <mode> [value]\n"
             "  pin: 0-31\n"
             "  mode: input, output, pullup, pulldown\n"
             "  value: 0 or 1 (for output mode)\n"
             "Example: gpio 5 output 1"
};
```

#### 4. 错误处理

- 验证所有参数
- 提供清晰的错误信息
- 返回适当的错误码
- 不要让系统崩溃

```c
static int cmd_example(int argc, char* argv[]) {
    /* 检查参数数量 */
    if (argc < 2) {
        shell_printf("Error: missing argument\r\n");
        shell_printf("Usage: %s\r\n", "example <arg>");
        return -1;
    }
    
    /* 验证参数 */
    int value = atoi(argv[1]);
    if (value < 0 || value > 100) {
        shell_printf("Error: value must be 0-100\r\n");
        return -1;
    }
    
    /* 执行操作 */
    if (do_something(value) != 0) {
        shell_printf("Error: operation failed\r\n");
        return -1;
    }
    
    shell_printf("Success\r\n");
    return 0;
}
```

### 性能优化

#### 1. 避免阻塞操作

```c
/* 不好：阻塞主循环 */
static int cmd_bad_delay(int argc, char* argv[]) {
    for (int i = 0; i < 1000000; i++) {
        /* 长时间循环 */
    }
    return 0;
}

/* 好：使用非阻塞方式 */
static int cmd_good_delay(int argc, char* argv[]) {
    /* 启动定时器或设置标志 */
    start_async_operation();
    shell_printf("Operation started\r\n");
    return 0;
}
```

#### 2. 减少输出

```c
/* 不好：频繁输出 */
for (int i = 0; i < 1000; i++) {
    shell_printf("%d\r\n", i);
}

/* 好：批量输出 */
char buffer[256];
int pos = 0;
for (int i = 0; i < 1000; i++) {
    pos += snprintf(&buffer[pos], sizeof(buffer) - pos, "%d ", i);
    if (pos >= sizeof(buffer) - 20 || i == 999) {
        shell_printf("%s\r\n", buffer);
        pos = 0;
    }
}
```

#### 3. 优化字符串操作

```c
/* 不好：频繁调用 strlen */
for (int i = 0; i < strlen(str); i++) {
    /* ... */
}

/* 好：缓存长度 */
size_t len = strlen(str);
for (size_t i = 0; i < len; i++) {
    /* ... */
}
```

### 内存使用优化

#### 1. 使用栈变量

```c
/* 好：使用栈变量 */
static int cmd_example(int argc, char* argv[]) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Result: %d", value);
    shell_printf("%s\r\n", buffer);
    return 0;
}

/* 避免：动态分配 */
static int cmd_bad_example(int argc, char* argv[]) {
    char* buffer = malloc(128);
    if (buffer == NULL) return -1;
    snprintf(buffer, 128, "Result: %d", value);
    shell_printf("%s\r\n", buffer);
    free(buffer);
    return 0;
}
```

#### 2. 重用缓冲区

```c
/* 全局缓冲区（如果线程安全） */
static char g_temp_buffer[256];

static int cmd_example(int argc, char* argv[]) {
    /* 重用全局缓冲区 */
    snprintf(g_temp_buffer, sizeof(g_temp_buffer), "Value: %d", value);
    shell_printf("%s\r\n", g_temp_buffer);
    return 0;
}
```

#### 3. 避免大数组

```c
/* 不好：大栈数组 */
static int cmd_bad(int argc, char* argv[]) {
    char huge_buffer[4096];  /* 可能导致栈溢出 */
    /* ... */
    return 0;
}

/* 好：使用静态或动态分配 */
static char g_large_buffer[4096];

static int cmd_good(int argc, char* argv[]) {
    /* 使用全局缓冲区 */
    /* ... */
    return 0;
}
```

### 线程安全

#### 1. 单线程使用

Shell Framework 设计为单线程使用：

```c
/* 在主循环中调用 */
int main(void) {
    app_init();
    
    while (1) {
        shell_process();
        /* ... */
    }
}
```

#### 2. 多线程环境

如果需要在多线程环境中使用，需要外部同步：

```c
/* 创建互斥锁 */
osal_mutex_t shell_mutex;

void app_init(void) {
    osal_mutex_create(&shell_mutex);
    shell_init(&config);
    /* ... */
}

/* Shell 任务 */
void shell_task(void* param) {
    while (1) {
        osal_mutex_lock(&shell_mutex);
        shell_process();
        osal_mutex_unlock(&shell_mutex);
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* 其他任务输出 */
void other_task(void* param) {
    while (1) {
        osal_mutex_lock(&shell_mutex);
        shell_printf("Message from other task\r\n");
        osal_mutex_unlock(&shell_mutex);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### 调试技巧

#### 1. 使用 Mock 后端测试

```c
void test_command(void) {
    /* 设置 Mock 后端 */
    const char* input = "led on\r";
    char output[256] = {0};
    
    shell_mock_backend_set_input(input);
    shell_mock_backend_set_output(output, sizeof(output));
    shell_set_backend(&shell_mock_backend);
    
    /* 处理输入 */
    for (size_t i = 0; i < strlen(input); i++) {
        shell_process();
    }
    
    /* 检查输出 */
    printf("Output: %s\n", output);
}
```

#### 2. 添加调试命令

```c
#ifdef DEBUG
static int cmd_debug(int argc, char* argv[]) {
    shell_printf("Debug information:\r\n");
    shell_printf("  Heap free: %d bytes\r\n", get_free_heap());
    shell_printf("  Stack used: %d bytes\r\n", get_stack_usage());
    shell_printf("  Tasks: %d\r\n", get_task_count());
    return 0;
}

static const shell_command_t debug_cmd = {
    .name = "debug",
    .handler = cmd_debug,
    .help = "Show debug information",
    .usage = "debug"
};
#endif
```

#### 3. 日志输出

```c
static int cmd_example(int argc, char* argv[]) {
    LOG_DEBUG("Shell", "Executing command: %s", argv[0]);
    
    /* 命令逻辑 */
    int result = do_something();
    
    if (result != 0) {
        LOG_ERROR("Shell", "Command failed: %d", result);
        return -1;
    }
    
    LOG_INFO("Shell", "Command completed successfully");
    return 0;
}
```

## 常见问题

### Q1: Shell 无响应

**问题**：输入字符没有回显，命令无法执行。

**可能原因**：
1. Shell 未初始化
2. 后端未配置
3. UART 未初始化
4. 未调用 `shell_process()`

**解决方案**：

```c
/* 检查初始化 */
if (!shell_is_initialized()) {
    shell_init(&config);
}

/* 检查后端 */
if (shell_get_backend() == NULL) {
    shell_set_backend(&shell_uart_backend);
}

/* 确保在主循环中调用 */
while (1) {
    shell_process();  /* 必须周期性调用 */
}
```

### Q2: 命令未找到

**问题**：输入命令后提示 "Unknown command"。

**可能原因**：
1. 命令未注册
2. 命令名拼写错误
3. 注册失败

**解决方案**：

```c
/* 检查命令是否注册 */
const shell_command_t* cmd = shell_find_command("mycommand");
if (cmd == NULL) {
    printf("Command not registered\n");
    
    /* 重新注册 */
    shell_status_t status = shell_register_command(&my_cmd);
    if (status != SHELL_OK) {
        printf("Registration failed: %d\n", status);
    }
}

/* 列出所有命令 */
uint8_t count = shell_get_command_count();
printf("Total commands: %d\n", count);
```

### Q3: 输出乱码

**问题**：Shell 输出显示乱码。

**可能原因**：
1. 波特率不匹配
2. 数据位/停止位/校验位配置错误
3. 终端编码设置错误

**解决方案**：

```c
/* 检查 UART 配置 */
hal_uart_config_t uart_cfg = {
    .baudrate = 115200,         /* 确保与终端一致 */
    .wordlen = HAL_UART_WORDLEN_8,
    .stopbits = HAL_UART_STOPBITS_1,
    .parity = HAL_UART_PARITY_NONE
};
hal_uart_init(HAL_UART_0, &uart_cfg);

/* 终端设置（minicom 示例） */
/* 波特率: 115200 */
/* 数据位: 8 */
/* 停止位: 1 */
/* 校验: None */
/* 流控: None */
```

### Q4: 历史记录不工作

**问题**：按上下箭头没有反应。

**可能原因**：
1. 历史深度设置为 0
2. 终端不支持转义序列
3. 转义序列处理错误

**解决方案**：

```c
/* 检查历史配置 */
shell_config_t config = {
    .prompt = "nexus> ",
    .cmd_buffer_size = 128,
    .history_depth = 16,  /* 确保 > 0 */
    .max_commands = 32
};

/* 测试转义序列 */
/* 在终端中按上箭头，应该看到: ESC[A */
/* 如果看不到，说明终端不支持或配置错误 */
```

### Q5: 自动补全不工作

**问题**：按 Tab 键没有反应。

**可能原因**：
1. 没有匹配的命令
2. 输入为空
3. Tab 键被终端拦截

**解决方案**：

```c
/* 确保有注册的命令 */
shell_register_builtin_commands();

/* 测试补全 */
/* 输入 "he" 然后按 Tab */
/* 应该补全为 "help" */

/* 如果还是不工作，检查终端设置 */
/* 确保 Tab 键发送 0x09 字符 */
```

### Q6: 内存不足

**问题**：Shell 初始化失败，返回 `SHELL_ERROR_NO_MEMORY`。

**可能原因**：
1. 堆内存不足
2. 配置的缓冲区太大

**解决方案**：

```c
/* 减小缓冲区大小 */
shell_config_t config = {
    .prompt = "$ ",
    .cmd_buffer_size = 64,   /* 减小到 64 */
    .history_depth = 4,      /* 减小到 4 */
    .max_commands = 16       /* 减小到 16 */
};

/* 或者使用静态分配（未来功能） */
#define SHELL_USE_STATIC_ALLOC 1
```

### Q7: 命令执行慢

**问题**：命令执行响应慢。

**可能原因**：
1. 命令处理函数有阻塞操作
2. `shell_process()` 调用频率太低
3. UART 波特率太低

**解决方案**：

```c
/* 增加 shell_process() 调用频率 */
while (1) {
    shell_process();  /* 尽可能频繁调用 */
    /* 其他任务 */
}

/* 避免命令中的阻塞操作 */
static int cmd_example(int argc, char* argv[]) {
    /* 不要这样做 */
    /* hal_delay_ms(5000); */
    
    /* 应该使用异步方式 */
    start_async_operation();
    shell_printf("Operation started\r\n");
    return 0;
}

/* 提高波特率 */
uart_cfg.baudrate = 921600;  /* 从 115200 提高到 921600 */
```

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**维护者**: Nexus Team
