# Shell Framework 测试指南

本文档详细描述 Nexus Shell Framework 的测试策略、测试用例和测试方法。

## 目录

1. [测试策略](#测试策略)
2. [测试环境](#测试环境)
3. [单元测试](#单元测试)
4. [集成测试](#集成测试)
5. [功能测试](#功能测试)
6. [性能测试](#性能测试)
7. [Mock 后端测试](#mock-后端测试)
8. [测试工具](#测试工具)
9. [持续集成](#持续集成)
10. [测试最佳实践](#测试最佳实践)

## 测试策略

### 测试层次

Shell Framework 采用多层次测试策略：

```
┌─────────────────────────────────────────┐
│         端到端测试 (E2E)                 │  手动测试
│  - 完整用户交互场景                      │
│  - 真实硬件环境                          │
└─────────────────────────────────────────┘
                  ▲
┌─────────────────────────────────────────┐
│         功能测试 (Functional)            │  自动化
│  - 命令执行测试                          │
│  - 行编辑测试                            │
│  - 历史管理测试                          │
└─────────────────────────────────────────┘
                  ▲
┌─────────────────────────────────────────┐
│         集成测试 (Integration)           │  自动化
│  - 多模块协同测试                        │
│  - 后端集成测试                          │
│  - 完整流程测试                          │
└─────────────────────────────────────────┘
                  ▲
┌─────────────────────────────────────────┐
│         单元测试 (Unit)                  │  自动化
│  - 命令注册测试                          │
│  - 行编辑器测试                          │
│  - 历史管理器测试                        │
│  - 参数解析器测试                        │
│  - 自动补全测试                          │
└─────────────────────────────────────────┘
```

### 测试目标

| 测试类型 | 目标覆盖率 | 优先级 |
|---------|-----------|--------|
| 单元测试 | ≥ 85% 行覆盖率 | 高 |
| 集成测试 | 所有关键流程 | 高 |
| 功能测试 | 所有用户功能 | 中 |
| 性能测试 | 关键性能指标 | 中 |

### 测试原则

1. **自动化优先**：尽可能自动化测试
2. **快速反馈**：测试应该快速执行
3. **独立性**：测试之间相互独立
4. **可重复**：测试结果可重复
5. **清晰性**：测试意图清晰

## 测试环境

### 测试框架

使用 Unity 测试框架：

```c
#include "unity.h"
#include "shell/shell.h"

void setUp(void) {
    /* 每个测试前执行 */
}

void tearDown(void) {
    /* 每个测试后执行 */
}

void test_shell_init(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_status_t status = shell_init(&config);
    TEST_ASSERT_EQUAL(SHELL_OK, status);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_shell_init);
    return UNITY_END();
}
```

### 编译配置

```cmake
# CMakeLists.txt
enable_testing()

# 添加测试可执行文件
add_executable(test_shell
    tests/test_shell.c
    tests/test_command.c
    tests/test_line_editor.c
    tests/test_history.c
    tests/test_parser.c
    tests/test_autocomplete.c
)

# 链接库
target_link_libraries(test_shell
    PRIVATE
        shell
        unity
)

# 添加测试
add_test(NAME shell_tests COMMAND test_shell)
```

### 运行测试

```bash
# 编译测试
mkdir build
cd build
cmake ..
make

# 运行所有测试
ctest

# 运行特定测试
./test_shell

# 生成覆盖率报告
cmake -DCMAKE_BUILD_TYPE=Coverage ..
make
make coverage
```

## 单元测试

### 初始化测试

```c
#include "unity.h"
#include "shell/shell.h"

void test_shell_init_with_default_config(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    
    shell_status_t status = shell_init(&config);
    TEST_ASSERT_EQUAL(SHELL_OK, status);
    TEST_ASSERT_TRUE(shell_is_initialized());
    
    shell_deinit();
}

void test_shell_init_with_custom_config(void) {
    shell_config_t config = {
        .prompt = "test> ",
        .cmd_buffer_size = 256,
        .history_depth = 32,
        .max_commands = 64
    };
    
    shell_status_t status = shell_init(&config);
    TEST_ASSERT_EQUAL(SHELL_OK, status);
    
    shell_deinit();
}

void test_shell_init_with_null_config(void) {
    shell_status_t status = shell_init(NULL);
    TEST_ASSERT_EQUAL(SHELL_ERROR_INVALID_PARAM, status);
}

void test_shell_init_with_invalid_buffer_size(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    config.cmd_buffer_size = 32;  /* 小于最小值 64 */
    
    shell_status_t status = shell_init(&config);
    TEST_ASSERT_EQUAL(SHELL_ERROR_INVALID_PARAM, status);
}

void test_shell_init_with_invalid_history_depth(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    config.history_depth = 2;  /* 小于最小值 4 */
    
    shell_status_t status = shell_init(&config);
    TEST_ASSERT_EQUAL(SHELL_ERROR_INVALID_PARAM, status);
}

void test_shell_double_init(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    
    shell_status_t status1 = shell_init(&config);
    TEST_ASSERT_EQUAL(SHELL_OK, status1);
    
    shell_status_t status2 = shell_init(&config);
    TEST_ASSERT_EQUAL(SHELL_ERROR_ALREADY_INIT, status2);
    
    shell_deinit();
}

void test_shell_deinit_without_init(void) {
    shell_status_t status = shell_deinit();
    TEST_ASSERT_EQUAL(SHELL_ERROR_NOT_INIT, status);
}

void test_shell_deinit_after_init(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    shell_status_t status = shell_deinit();
    TEST_ASSERT_EQUAL(SHELL_OK, status);
    TEST_ASSERT_FALSE(shell_is_initialized());
}
```

### 命令注册测试

```c
static int test_cmd_handler(int argc, char* argv[]) {
    return 0;
}

void test_register_command(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    shell_command_t cmd = {
        .name = "test",
        .handler = test_cmd_handler,
        .help = "Test command",
        .usage = "test"
    };
    
    shell_status_t status = shell_register_command(&cmd);
    TEST_ASSERT_EQUAL(SHELL_OK, status);
    
    shell_deinit();
}

void test_register_command_with_null(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    shell_status_t status = shell_register_command(NULL);
    TEST_ASSERT_EQUAL(SHELL_ERROR_INVALID_PARAM, status);
    
    shell_deinit();
}

void test_register_duplicate_command(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    shell_command_t cmd = {
        .name = "test",
        .handler = test_cmd_handler,
        .help = "Test command",
        .usage = "test"
    };
    
    shell_status_t status1 = shell_register_command(&cmd);
    TEST_ASSERT_EQUAL(SHELL_OK, status1);
    
    shell_status_t status2 = shell_register_command(&cmd);
    TEST_ASSERT_EQUAL(SHELL_ERROR_ALREADY_EXISTS, status2);
    
    shell_deinit();
}

void test_find_command(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    shell_command_t cmd = {
        .name = "test",
        .handler = test_cmd_handler,
        .help = "Test command",
        .usage = "test"
    };
    
    shell_register_command(&cmd);
    
    const shell_command_t* found = shell_find_command("test");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("test", found->name);
    
    shell_deinit();
}

void test_find_nonexistent_command(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    const shell_command_t* found = shell_find_command("nonexistent");
    TEST_ASSERT_NULL(found);
    
    shell_deinit();
}

void test_unregister_command(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    shell_command_t cmd = {
        .name = "test",
        .handler = test_cmd_handler,
        .help = "Test command",
        .usage = "test"
    };
    
    shell_register_command(&cmd);
    
    shell_status_t status = shell_unregister_command("test");
    TEST_ASSERT_EQUAL(SHELL_OK, status);
    
    const shell_command_t* found = shell_find_command("test");
    TEST_ASSERT_NULL(found);
    
    shell_deinit();
}

void test_get_command_count(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    uint8_t count1 = shell_get_command_count();
    
    shell_command_t cmd = {
        .name = "test",
        .handler = test_cmd_handler,
        .help = "Test command",
        .usage = "test"
    };
    
    shell_register_command(&cmd);
    
    uint8_t count2 = shell_get_command_count();
    TEST_ASSERT_EQUAL(count1 + 1, count2);
    
    shell_deinit();
}
```

### 行编辑器测试

```c
#include "shell/shell_line_editor.h"

void test_line_editor_init(void) {
    char buffer[128];
    line_editor_t editor;
    
    line_editor_init(&editor, buffer, sizeof(buffer));
    
    TEST_ASSERT_EQUAL(0, line_editor_get_length(&editor));
    TEST_ASSERT_EQUAL(0, line_editor_get_cursor(&editor));
}

void test_line_editor_insert_char(void) {
    char buffer[128];
    line_editor_t editor;
    line_editor_init(&editor, buffer, sizeof(buffer));
    
    bool result = line_editor_insert_char(&editor, 'a');
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, line_editor_get_length(&editor));
    TEST_ASSERT_EQUAL(1, line_editor_get_cursor(&editor));
    TEST_ASSERT_EQUAL_STRING("a", line_editor_get_buffer(&editor));
}

void test_line_editor_insert_multiple_chars(void) {
    char buffer[128];
    line_editor_t editor;
    line_editor_init(&editor, buffer, sizeof(buffer));
    
    line_editor_insert_char(&editor, 'h');
    line_editor_insert_char(&editor, 'e');
    line_editor_insert_char(&editor, 'l');
    line_editor_insert_char(&editor, 'l');
    line_editor_insert_char(&editor, 'o');
    
    TEST_ASSERT_EQUAL(5, line_editor_get_length(&editor));
    TEST_ASSERT_EQUAL_STRING("hello", line_editor_get_buffer(&editor));
}

void test_line_editor_backspace(void) {
    char buffer[128];
    line_editor_t editor;
    line_editor_init(&editor, buffer, sizeof(buffer));
    
    line_editor_insert_char(&editor, 'a');
    line_editor_insert_char(&editor, 'b');
    
    bool result = line_editor_backspace(&editor);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, line_editor_get_length(&editor));
    TEST_ASSERT_EQUAL_STRING("a", line_editor_get_buffer(&editor));
}

void test_line_editor_backspace_at_start(void) {
    char buffer[128];
    line_editor_t editor;
    line_editor_init(&editor, buffer, sizeof(buffer));
    
    bool result = line_editor_backspace(&editor);
    TEST_ASSERT_FALSE(result);
}

void test_line_editor_delete_char(void) {
    char buffer[128];
    line_editor_t editor;
    line_editor_init(&editor, buffer, sizeof(buffer));
    
    line_editor_insert_char(&editor, 'a');
    line_editor_insert_char(&editor, 'b');
    line_editor_insert_char(&editor, 'c');
    
    /* 移动光标到中间 */
    line_editor_move_cursor(&editor, -2);
    
    /* 删除 'b' */
    bool result = line_editor_delete_char(&editor);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(2, line_editor_get_length(&editor));
    TEST_ASSERT_EQUAL_STRING("ac", line_editor_get_buffer(&editor));
}

void test_line_editor_move_cursor(void) {
    char buffer[128];
    line_editor_t editor;
    line_editor_init(&editor, buffer, sizeof(buffer));
    
    line_editor_insert_char(&editor, 'a');
    line_editor_insert_char(&editor, 'b');
    line_editor_insert_char(&editor, 'c');
    
    /* 光标在末尾 (3) */
    TEST_ASSERT_EQUAL(3, line_editor_get_cursor(&editor));
    
    /* 左移 */
    line_editor_move_cursor(&editor, -1);
    TEST_ASSERT_EQUAL(2, line_editor_get_cursor(&editor));
    
    /* 右移 */
    line_editor_move_cursor(&editor, 1);
    TEST_ASSERT_EQUAL(3, line_editor_get_cursor(&editor));
}

void test_line_editor_move_to_start(void) {
    char buffer[128];
    line_editor_t editor;
    line_editor_init(&editor, buffer, sizeof(buffer));
    
    line_editor_insert_char(&editor, 'a');
    line_editor_insert_char(&editor, 'b');
    line_editor_insert_char(&editor, 'c');
    
    line_editor_move_to_start(&editor);
    TEST_ASSERT_EQUAL(0, line_editor_get_cursor(&editor));
}

void test_line_editor_move_to_end(void) {
    char buffer[128];
    line_editor_t editor;
    line_editor_init(&editor, buffer, sizeof(buffer));
    
    line_editor_insert_char(&editor, 'a');
    line_editor_insert_char(&editor, 'b');
    line_editor_insert_char(&editor, 'c');
    
    line_editor_move_to_start(&editor);
    line_editor_move_to_end(&editor);
    
    TEST_ASSERT_EQUAL(3, line_editor_get_cursor(&editor));
}

void test_line_editor_clear(void) {
    char buffer[128];
    line_editor_t editor;
    line_editor_init(&editor, buffer, sizeof(buffer));
    
    line_editor_insert_char(&editor, 'a');
    line_editor_insert_char(&editor, 'b');
    
    line_editor_clear(&editor);
    
    TEST_ASSERT_EQUAL(0, line_editor_get_length(&editor));
    TEST_ASSERT_EQUAL(0, line_editor_get_cursor(&editor));
}

void test_line_editor_delete_to_end(void) {
    char buffer[128];
    line_editor_t editor;
    line_editor_init(&editor, buffer, sizeof(buffer));
    
    line_editor_insert_char(&editor, 'h');
    line_editor_insert_char(&editor, 'e');
    line_editor_insert_char(&editor, 'l');
    line_editor_insert_char(&editor, 'l');
    line_editor_insert_char(&editor, 'o');
    
    /* 移动到中间 */
    line_editor_move_cursor(&editor, -3);
    
    /* 删除到行尾 */
    line_editor_delete_to_end(&editor);
    
    TEST_ASSERT_EQUAL(2, line_editor_get_length(&editor));
    TEST_ASSERT_EQUAL_STRING("he", line_editor_get_buffer(&editor));
}

void test_line_editor_delete_to_start(void) {
    char buffer[128];
    line_editor_t editor;
    line_editor_init(&editor, buffer, sizeof(buffer));
    
    line_editor_insert_char(&editor, 'h');
    line_editor_insert_char(&editor, 'e');
    line_editor_insert_char(&editor, 'l');
    line_editor_insert_char(&editor, 'l');
    line_editor_insert_char(&editor, 'o');
    
    /* 移动到中间 */
    line_editor_move_cursor(&editor, -3);
    
    /* 删除到行首 */
    line_editor_delete_to_start(&editor);
    
    TEST_ASSERT_EQUAL(3, line_editor_get_length(&editor));
    TEST_ASSERT_EQUAL_STRING("llo", line_editor_get_buffer(&editor));
}
```

### 历史管理测试

```c
#include "shell/shell_history.h"

void test_history_init(void) {
    char* entries[4];
    char storage[4 * 64];
    
    for (int i = 0; i < 4; i++) {
        entries[i] = &storage[i * 64];
    }
    
    history_manager_t hist;
    history_init(&hist, entries, 4, 64);
    
    TEST_ASSERT_EQUAL(0, history_get_count(&hist));
}

void test_history_add(void) {
    char* entries[4];
    char storage[4 * 64];
    
    for (int i = 0; i < 4; i++) {
        entries[i] = &storage[i * 64];
    }
    
    history_manager_t hist;
    history_init(&hist, entries, 4, 64);
    
    history_add(&hist, "command1");
    
    TEST_ASSERT_EQUAL(1, history_get_count(&hist));
}

void test_history_add_multiple(void) {
    char* entries[4];
    char storage[4 * 64];
    
    for (int i = 0; i < 4; i++) {
        entries[i] = &storage[i * 64];
    }
    
    history_manager_t hist;
    history_init(&hist, entries, 4, 64);
    
    history_add(&hist, "command1");
    history_add(&hist, "command2");
    history_add(&hist, "command3");
    
    TEST_ASSERT_EQUAL(3, history_get_count(&hist));
}

void test_history_add_duplicate(void) {
    char* entries[4];
    char storage[4 * 64];
    
    for (int i = 0; i < 4; i++) {
        entries[i] = &storage[i * 64];
    }
    
    history_manager_t hist;
    history_init(&hist, entries, 4, 64);
    
    history_add(&hist, "command1");
    history_add(&hist, "command1");  /* 重复 */
    
    TEST_ASSERT_EQUAL(1, history_get_count(&hist));
}

void test_history_overflow(void) {
    char* entries[4];
    char storage[4 * 64];
    
    for (int i = 0; i < 4; i++) {
        entries[i] = &storage[i * 64];
    }
    
    history_manager_t hist;
    history_init(&hist, entries, 4, 64);
    
    /* 添加超过容量的命令 */
    history_add(&hist, "command1");
    history_add(&hist, "command2");
    history_add(&hist, "command3");
    history_add(&hist, "command4");
    history_add(&hist, "command5");  /* 覆盖最旧的 */
    
    TEST_ASSERT_EQUAL(4, history_get_count(&hist));
}

void test_history_get_prev(void) {
    char* entries[4];
    char storage[4 * 64];
    
    for (int i = 0; i < 4; i++) {
        entries[i] = &storage[i * 64];
    }
    
    history_manager_t hist;
    history_init(&hist, entries, 4, 64);
    
    history_add(&hist, "command1");
    history_add(&hist, "command2");
    history_add(&hist, "command3");
    
    const char* cmd = history_get_prev(&hist);
    TEST_ASSERT_NOT_NULL(cmd);
    TEST_ASSERT_EQUAL_STRING("command3", cmd);
}

void test_history_get_next(void) {
    char* entries[4];
    char storage[4 * 64];
    
    for (int i = 0; i < 4; i++) {
        entries[i] = &storage[i * 64];
    }
    
    history_manager_t hist;
    history_init(&hist, entries, 4, 64);
    
    history_add(&hist, "command1");
    history_add(&hist, "command2");
    history_add(&hist, "command3");
    
    history_get_prev(&hist);  /* command3 */
    history_get_prev(&hist);  /* command2 */
    
    const char* cmd = history_get_next(&hist);
    TEST_ASSERT_NOT_NULL(cmd);
    TEST_ASSERT_EQUAL_STRING("command3", cmd);
}

void test_history_clear(void) {
    char* entries[4];
    char storage[4 * 64];
    
    for (int i = 0; i < 4; i++) {
        entries[i] = &storage[i * 64];
    }
    
    history_manager_t hist;
    history_init(&hist, entries, 4, 64);
    
    history_add(&hist, "command1");
    history_add(&hist, "command2");
    
    history_clear(&hist);
    
    TEST_ASSERT_EQUAL(0, history_get_count(&hist));
}
```


### 参数解析测试

```c
#include "shell/shell_parser.h"

void test_parse_simple_command(void) {
    char input[] = "test arg1 arg2";
    parsed_command_t parsed;
    
    shell_status_t status = parse_command_line(input, &parsed);
    
    TEST_ASSERT_EQUAL(SHELL_OK, status);
    TEST_ASSERT_EQUAL(3, parsed.argc);
    TEST_ASSERT_EQUAL_STRING("test", parsed.argv[0]);
    TEST_ASSERT_EQUAL_STRING("arg1", parsed.argv[1]);
    TEST_ASSERT_EQUAL_STRING("arg2", parsed.argv[2]);
}

void test_parse_command_with_quotes(void) {
    char input[] = "test \"hello world\" 'foo bar'";
    parsed_command_t parsed;
    
    shell_status_t status = parse_command_line(input, &parsed);
    
    TEST_ASSERT_EQUAL(SHELL_OK, status);
    TEST_ASSERT_EQUAL(3, parsed.argc);
    TEST_ASSERT_EQUAL_STRING("test", parsed.argv[0]);
    TEST_ASSERT_EQUAL_STRING("hello world", parsed.argv[1]);
    TEST_ASSERT_EQUAL_STRING("foo bar", parsed.argv[2]);
}

void test_parse_command_with_escape(void) {
    char input[] = "test \"hello \\\"world\\\"\"";
    parsed_command_t parsed;
    
    shell_status_t status = parse_command_line(input, &parsed);
    
    TEST_ASSERT_EQUAL(SHELL_OK, status);
    TEST_ASSERT_EQUAL(2, parsed.argc);
    TEST_ASSERT_EQUAL_STRING("hello \"world\"", parsed.argv[1]);
}

void test_parse_empty_command(void) {
    char input[] = "";
    parsed_command_t parsed;
    
    shell_status_t status = parse_command_line(input, &parsed);
    
    TEST_ASSERT_EQUAL(SHELL_OK, status);
    TEST_ASSERT_EQUAL(0, parsed.argc);
}

void test_parse_command_with_multiple_spaces(void) {
    char input[] = "test    arg1     arg2";
    parsed_command_t parsed;
    
    shell_status_t status = parse_command_line(input, &parsed);
    
    TEST_ASSERT_EQUAL(SHELL_OK, status);
    TEST_ASSERT_EQUAL(3, parsed.argc);
}
```

### 自动补全测试

```c
#include "shell/shell_autocomplete.h"

void test_autocomplete_single_match(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    /* 注册命令 */
    shell_command_t cmd = {
        .name = "hello",
        .handler = test_cmd_handler,
        .help = "Test",
        .usage = "hello"
    };
    shell_register_command(&cmd);
    
    /* 测试补全 */
    completion_result_t result;
    shell_status_t status = autocomplete_process("hel", 3, 3, &result);
    
    TEST_ASSERT_EQUAL(SHELL_OK, status);
    TEST_ASSERT_EQUAL(1, result.match_count);
    TEST_ASSERT_EQUAL_STRING("hello", result.matches[0]);
    
    shell_deinit();
}

void test_autocomplete_multiple_matches(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    /* 注册多个命令 */
    shell_command_t cmd1 = {
        .name = "help",
        .handler = test_cmd_handler,
        .help = "Help",
        .usage = "help"
    };
    shell_command_t cmd2 = {
        .name = "hello",
        .handler = test_cmd_handler,
        .help = "Hello",
        .usage = "hello"
    };
    shell_register_command(&cmd1);
    shell_register_command(&cmd2);
    
    /* 测试补全 */
    completion_result_t result;
    shell_status_t status = autocomplete_process("he", 2, 2, &result);
    
    TEST_ASSERT_EQUAL(SHELL_OK, status);
    TEST_ASSERT_EQUAL(2, result.match_count);
    
    shell_deinit();
}

void test_autocomplete_no_match(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    completion_result_t result;
    shell_status_t status = autocomplete_process("xyz", 3, 3, &result);
    
    TEST_ASSERT_EQUAL(SHELL_OK, status);
    TEST_ASSERT_EQUAL(0, result.match_count);
    
    shell_deinit();
}

void test_autocomplete_common_prefix(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    shell_command_t cmd1 = {
        .name = "test1",
        .handler = test_cmd_handler,
        .help = "Test 1",
        .usage = "test1"
    };
    shell_command_t cmd2 = {
        .name = "test2",
        .handler = test_cmd_handler,
        .help = "Test 2",
        .usage = "test2"
    };
    shell_register_command(&cmd1);
    shell_register_command(&cmd2);
    
    completion_result_t result;
    autocomplete_process("te", 2, 2, &result);
    
    char prefix[32];
    int len = autocomplete_get_common_prefix(&result, prefix, sizeof(prefix));
    
    TEST_ASSERT_EQUAL(4, len);
    TEST_ASSERT_EQUAL_STRING("test", prefix);
    
    shell_deinit();
}
```

## 集成测试

### 完整命令执行测试

```c
#include "shell/shell.h"
#include "shell/shell_mock_backend.h"

static int g_test_cmd_called = 0;
static int g_test_cmd_argc = 0;
static char g_test_cmd_argv[8][32];

static int test_integration_cmd(int argc, char* argv[]) {
    g_test_cmd_called = 1;
    g_test_cmd_argc = argc;
    
    for (int i = 0; i < argc && i < 8; i++) {
        strncpy(g_test_cmd_argv[i], argv[i], 31);
        g_test_cmd_argv[i][31] = '\0';
    }
    
    shell_printf("Command executed\r\n");
    return 0;
}

void test_complete_command_execution(void) {
    /* 初始化 Shell */
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    /* 注册命令 */
    shell_command_t cmd = {
        .name = "test",
        .handler = test_integration_cmd,
        .help = "Test command",
        .usage = "test <args>"
    };
    shell_register_command(&cmd);
    
    /* 设置 Mock 后端 */
    const char* input = "test arg1 arg2\r";
    char output[256] = {0};
    shell_mock_backend_set_input(input);
    shell_mock_backend_set_output(output, sizeof(output));
    shell_set_backend(&shell_mock_backend);
    
    /* 重置标志 */
    g_test_cmd_called = 0;
    g_test_cmd_argc = 0;
    
    /* 处理输入 */
    for (size_t i = 0; i < strlen(input); i++) {
        shell_process();
    }
    
    /* 验证结果 */
    TEST_ASSERT_EQUAL(1, g_test_cmd_called);
    TEST_ASSERT_EQUAL(3, g_test_cmd_argc);
    TEST_ASSERT_EQUAL_STRING("test", g_test_cmd_argv[0]);
    TEST_ASSERT_EQUAL_STRING("arg1", g_test_cmd_argv[1]);
    TEST_ASSERT_EQUAL_STRING("arg2", g_test_cmd_argv[2]);
    
    /* 检查输出 */
    TEST_ASSERT_TRUE(strstr(output, "Command executed") != NULL);
    
    shell_deinit();
}

void test_unknown_command(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    const char* input = "unknown\r";
    char output[256] = {0};
    shell_mock_backend_set_input(input);
    shell_mock_backend_set_output(output, sizeof(output));
    shell_set_backend(&shell_mock_backend);
    
    for (size_t i = 0; i < strlen(input); i++) {
        shell_process();
    }
    
    TEST_ASSERT_TRUE(strstr(output, "Unknown command") != NULL);
    
    shell_deinit();
}

void test_command_with_history(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    shell_command_t cmd = {
        .name = "test",
        .handler = test_integration_cmd,
        .help = "Test",
        .usage = "test"
    };
    shell_register_command(&cmd);
    
    /* 执行第一条命令 */
    const char* input1 = "test arg1\r";
    char output1[256] = {0};
    shell_mock_backend_set_input(input1);
    shell_mock_backend_set_output(output1, sizeof(output1));
    shell_set_backend(&shell_mock_backend);
    
    for (size_t i = 0; i < strlen(input1); i++) {
        shell_process();
    }
    
    /* 按上箭头调出历史 */
    const char* input2 = "\033[A\r";  /* ESC[A = 上箭头 */
    char output2[256] = {0};
    shell_mock_backend_set_input(input2);
    shell_mock_backend_set_output(output2, sizeof(output2));
    
    g_test_cmd_called = 0;
    
    for (size_t i = 0; i < strlen(input2); i++) {
        shell_process();
    }
    
    /* 验证命令再次执行 */
    TEST_ASSERT_EQUAL(1, g_test_cmd_called);
    
    shell_deinit();
}
```

## 功能测试

### 行编辑功能测试

```c
void test_cursor_movement(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    /* 输入 "hello" */
    const char* input = "hello";
    char output[256] = {0};
    shell_mock_backend_set_input(input);
    shell_mock_backend_set_output(output, sizeof(output));
    shell_set_backend(&shell_mock_backend);
    
    for (size_t i = 0; i < strlen(input); i++) {
        shell_process();
    }
    
    /* 按 Home 键 (ESC[H) */
    const char* home = "\033[H";
    shell_mock_backend_set_input(home);
    
    for (size_t i = 0; i < strlen(home); i++) {
        shell_process();
    }
    
    /* 插入 "x" */
    shell_mock_backend_set_input("x");
    shell_process();
    
    /* 验证结果应该是 "xhello" */
    /* 这里需要检查内部状态或输出 */
    
    shell_deinit();
}

void test_backspace_delete(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    /* 输入 "hello" */
    const char* input = "hello\b\b";  /* 删除两个字符 */
    char output[256] = {0};
    shell_mock_backend_set_input(input);
    shell_mock_backend_set_output(output, sizeof(output));
    shell_set_backend(&shell_mock_backend);
    
    for (size_t i = 0; i < strlen(input); i++) {
        shell_process();
    }
    
    /* 结果应该是 "hel" */
    
    shell_deinit();
}

void test_ctrl_k_delete_to_end(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    /* 输入 "hello world" */
    const char* input = "hello world";
    shell_mock_backend_set_input(input);
    shell_set_backend(&shell_mock_backend);
    
    for (size_t i = 0; i < strlen(input); i++) {
        shell_process();
    }
    
    /* 移动到中间 */
    const char* left = "\033[D\033[D\033[D\033[D\033[D\033[D";
    shell_mock_backend_set_input(left);
    
    for (size_t i = 0; i < strlen(left); i++) {
        shell_process();
    }
    
    /* Ctrl+K 删除到行尾 */
    shell_mock_backend_set_input("\x0B");  /* Ctrl+K */
    shell_process();
    
    /* 结果应该是 "hello" */
    
    shell_deinit();
}
```

## 性能测试

### 命令执行延迟测试

```c
#include <time.h>

void test_command_execution_latency(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    shell_command_t cmd = {
        .name = "test",
        .handler = test_cmd_handler,
        .help = "Test",
        .usage = "test"
    };
    shell_register_command(&cmd);
    
    const char* input = "test\r";
    shell_mock_backend_set_input(input);
    shell_set_backend(&shell_mock_backend);
    
    /* 测量执行时间 */
    clock_t start = clock();
    
    for (size_t i = 0; i < strlen(input); i++) {
        shell_process();
    }
    
    clock_t end = clock();
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
    
    /* 验证延迟小于 1ms */
    TEST_ASSERT_LESS_THAN(1.0, elapsed);
    
    printf("Command execution latency: %.3f ms\n", elapsed);
    
    shell_deinit();
}

void test_shell_process_performance(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    shell_set_backend(&shell_mock_backend);
    
    /* 无输入时的性能 */
    shell_mock_backend_set_input("");
    
    clock_t start = clock();
    
    for (int i = 0; i < 10000; i++) {
        shell_process();
    }
    
    clock_t end = clock();
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
    
    printf("10000 shell_process() calls: %.3f ms\n", elapsed);
    printf("Average per call: %.3f us\n", elapsed * 1000.0 / 10000.0);
    
    shell_deinit();
}
```

### 内存使用测试

```c
void test_memory_usage(void) {
    /* 记录初始内存 */
    size_t initial_heap = get_free_heap_size();
    
    /* 初始化 Shell */
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    /* 记录初始化后内存 */
    size_t after_init_heap = get_free_heap_size();
    size_t init_memory = initial_heap - after_init_heap;
    
    printf("Shell initialization memory: %zu bytes\n", init_memory);
    
    /* 验证内存使用在预期范围内 */
    TEST_ASSERT_LESS_THAN(5120, init_memory);  /* 小于 5KB */
    
    /* 反初始化 */
    shell_deinit();
    
    /* 验证内存释放 */
    size_t final_heap = get_free_heap_size();
    TEST_ASSERT_EQUAL(initial_heap, final_heap);
}
```

## Mock 后端测试

### Mock 后端使用示例

```c
#include "shell/shell_mock_backend.h"

void test_mock_backend_basic(void) {
    /* 设置输入 */
    const char* input = "hello";
    shell_mock_backend_set_input(input);
    
    /* 设置输出缓冲区 */
    char output[256] = {0};
    shell_mock_backend_set_output(output, sizeof(output));
    
    /* 读取测试 */
    char c;
    int n = shell_mock_backend.read(&c, 1);
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_EQUAL('h', c);
    
    /* 写入测试 */
    const char* msg = "test";
    n = shell_mock_backend.write(msg, strlen(msg));
    TEST_ASSERT_EQUAL(4, n);
    TEST_ASSERT_EQUAL_STRING("test", output);
}

void test_mock_backend_with_shell(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    /* 设置 Mock 后端 */
    const char* input = "help\r";
    char output[512] = {0};
    shell_mock_backend_set_input(input);
    shell_mock_backend_set_output(output, sizeof(output));
    shell_set_backend(&shell_mock_backend);
    
    /* 注册内置命令 */
    shell_register_builtin_commands();
    
    /* 处理输入 */
    for (size_t i = 0; i < strlen(input); i++) {
        shell_process();
    }
    
    /* 验证输出包含帮助信息 */
    TEST_ASSERT_TRUE(strstr(output, "Available commands") != NULL);
    
    shell_deinit();
}
```

## 测试工具

### 测试辅助函数

```c
/* 初始化测试环境 */
void test_setup_shell(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    shell_set_backend(&shell_mock_backend);
}

/* 清理测试环境 */
void test_teardown_shell(void) {
    shell_deinit();
}

/* 模拟输入字符串 */
void test_input_string(const char* str) {
    shell_mock_backend_set_input(str);
    for (size_t i = 0; i < strlen(str); i++) {
        shell_process();
    }
}

/* 检查输出包含字符串 */
void test_assert_output_contains(const char* expected) {
    char output[512];
    shell_mock_backend_get_output(output, sizeof(output));
    TEST_ASSERT_TRUE(strstr(output, expected) != NULL);
}

/* 清空输出缓冲区 */
void test_clear_output(void) {
    char output[512] = {0};
    shell_mock_backend_set_output(output, sizeof(output));
}
```

### 使用测试辅助函数

```c
void test_with_helpers(void) {
    test_setup_shell();
    
    /* 注册命令 */
    shell_command_t cmd = {
        .name = "test",
        .handler = test_cmd_handler,
        .help = "Test",
        .usage = "test"
    };
    shell_register_command(&cmd);
    
    /* 测试命令执行 */
    test_input_string("test\r");
    test_assert_output_contains("test");
    
    test_teardown_shell();
}
```

## 持续集成

### GitHub Actions 配置

```yaml
# .github/workflows/test.yml
name: Shell Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y cmake gcc lcov
    
    - name: Build
      run: |
        mkdir build
        cd build
        cmake -DCMAKE_BUILD_TYPE=Debug ..
        make
    
    - name: Run tests
      run: |
        cd build
        ctest --output-on-failure
    
    - name: Generate coverage
      run: |
        cd build
        lcov --capture --directory . --output-file coverage.info
        lcov --remove coverage.info '/usr/*' --output-file coverage.info
        lcov --list coverage.info
    
    - name: Upload coverage
      uses: codecov/codecov-action@v2
      with:
        files: ./build/coverage.info
```

### CMake 测试配置

```cmake
# tests/CMakeLists.txt
enable_testing()

# 添加测试可执行文件
add_executable(test_shell
    test_shell.c
    test_command.c
    test_line_editor.c
    test_history.c
    test_parser.c
    test_autocomplete.c
    test_integration.c
)

target_link_libraries(test_shell
    PRIVATE
        shell
        unity
)

# 添加测试
add_test(NAME shell_unit_tests COMMAND test_shell)

# 设置测试超时
set_tests_properties(shell_unit_tests PROPERTIES TIMEOUT 30)

# 覆盖率配置
if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    target_compile_options(test_shell PRIVATE --coverage)
    target_link_options(test_shell PRIVATE --coverage)
endif()
```

## 测试最佳实践

### 1. 测试命名

```c
/* 好的测试名称 */
void test_shell_init_with_default_config(void);
void test_command_register_duplicate_returns_error(void);
void test_line_editor_backspace_at_start_returns_false(void);

/* 不好的测试名称 */
void test1(void);
void test_init(void);
void test_error(void);
```

### 2. 测试独立性

```c
/* 好：每个测试独立 */
void test_command_register(void) {
    shell_config_t config = SHELL_CONFIG_DEFAULT;
    shell_init(&config);
    
    /* 测试逻辑 */
    
    shell_deinit();
}

/* 不好：依赖全局状态 */
static shell_config_t g_config;

void test_setup(void) {
    shell_init(&g_config);
}

void test_command_register(void) {
    /* 依赖 test_setup */
}
```

### 3. 断言清晰

```c
/* 好：清晰的断言 */
TEST_ASSERT_EQUAL(SHELL_OK, status);
TEST_ASSERT_EQUAL_STRING("expected", actual);
TEST_ASSERT_TRUE(condition);

/* 不好：模糊的断言 */
TEST_ASSERT(status == 0);
TEST_ASSERT(strcmp(expected, actual) == 0);
```

### 4. 测试覆盖

```c
/* 测试正常情况 */
void test_normal_case(void);

/* 测试边界条件 */
void test_boundary_case(void);

/* 测试错误情况 */
void test_error_case(void);

/* 测试异常情况 */
void test_exception_case(void);
```

### 5. 性能测试

```c
void test_performance(void) {
    /* 预热 */
    for (int i = 0; i < 100; i++) {
        function_under_test();
    }
    
    /* 测量 */
    clock_t start = clock();
    for (int i = 0; i < 1000; i++) {
        function_under_test();
    }
    clock_t end = clock();
    
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Performance: %.3f ms\n", elapsed * 1000.0);
    
    /* 验证性能要求 */
    TEST_ASSERT_LESS_THAN(10.0, elapsed * 1000.0);
}
```

---

**文档版本**: 1.0.0  
**最后更新**: 2026-01-24  
**维护者**: Nexus Team
