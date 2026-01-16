# Nexus 代码注释规范

本文档定义了 Nexus 项目的代码注释规范，确保所有代码注释风格统一。

## Doxygen 标签对齐

所有 Doxygen 标签必须对齐到第 20 列（`*` 后 17 个空格）：

```c
/**
 * \brief           描述从第 20 列开始
 * \param[in]       param: 参数描述
 * \return          返回值描述
 */
```

## 文件头注释

### 头文件 (.h) - 完整格式

```c
/**
 * \file            filename.h
 * \brief           文件简要描述
 * \author          Nexus Team
 */
```

### 源文件 (.c) - 完整格式

```c
/**
 * \file            filename.c
 * \brief           文件简要描述
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-XX
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         详细描述（可选，多行）
 */
```

## 函数注释

### 头文件中的公共 API

```c
/**
 * \brief           函数简要描述
 * \param[in]       param1: 输入参数描述
 * \param[out]      param2: 输出参数描述
 * \param[in,out]   param3: 输入输出参数描述
 * \return          返回值描述
 * \note            附加说明（可选）
 */
```

### 源文件中的实现

源文件不重复 `\param` 和 `\return`，只使用 `\brief`、`\details`、`\note`：

```c
/**
 * \brief           函数简要描述
 * \details         实现细节说明
 * \note            注意事项
 */
```

### 静态函数

静态函数使用简化注释：

```c
/**
 * \brief           静态函数描述
 */
static void internal_function(void) {
```

## 结构体和枚举

```c
/**
 * \brief           结构体描述
 */
typedef struct {
    int field1;    /**< 字段描述 */
    int field2;    /**< 字段描述 */
} my_struct_t;

/**
 * \brief           枚举描述
 */
typedef enum {
    VALUE_A = 0,   /**< 值 A 描述 */
    VALUE_B,       /**< 值 B 描述 */
} my_enum_t;
```

## 代码分隔线

使用统一的分隔线风格：

```c
/*---------------------------------------------------------------------------*/
/* Section Name                                                              */
/*---------------------------------------------------------------------------*/
```

分隔线总长度为 77 个字符（`/*` + 75 个 `-` 或空格 + `*/`）。

## 内联注释

- 使用 `/* comment */` 风格，不使用 `//`
- 行尾注释前保留两个空格

```c
int value = 0;  /* 初始化为零 */
```

## 宏定义注释

```c
/**
 * \brief           宏描述
 */
#define MY_MACRO 42

/* 或使用行尾注释 */
#define MY_MACRO 42  /**< 宏描述 */
```

## 禁止事项

- 不使用 `@` 风格的 Doxygen 标签（使用 `\`）
- 不使用 `//` 单行注释
- 不使用 `/*===...===*/` 分隔线（统一使用 `/*---...---*/`）
