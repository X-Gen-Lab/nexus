编码规范
========

Nexus 遵循严格的编码规范以保证质量和安全。

C 标准
------

- 源代码使用 C11
- 测试代码使用 C++17

代码格式
--------

项目使用 clang-format 进行自动代码格式化。提交代码前请运行 ``clang-format``。

缩进
~~~~

- 使用 4 个空格缩进（不使用制表符）
- switch 语句中的 case 标签需要缩进
- 预处理指令不缩进

行长度
~~~~~~

- 最大行长度：80 个字符
- 在适当位置换行

大括号
~~~~~~

使用附着式大括号（K&R 风格）::

    if (condition) {
        /* 代码 */
    } else {
        /* 代码 */
    }

    void function(void) {
        /* 代码 */
    }

空格
~~~~

- 控制关键字后加空格：``if (``、``for (``、``while (``
- 函数名后不加空格：``func()``
- 类型转换后不加空格：``(int)value``
- 尾部注释前加两个空格
- 二元运算符两侧加空格：``a + b``、``x = y``

指针对齐
~~~~~~~~

指针符号靠左对齐（与类型一起）::

    int* ptr;           /* 正确 */
    char* str;          /* 正确 */

空行
~~~~

- 最多连续一个空行
- 使用空行分隔逻辑段落

命名约定
--------

- 函数: ``module_action_object()`` (例如 ``hal_gpio_init()``)
- 类型: ``module_type_t`` (例如 ``hal_status_t``)
- 宏: ``MODULE_MACRO_NAME`` (例如 ``HAL_OK``)
- 常量: ``MODULE_CONSTANT`` (例如 ``HAL_GPIO_PORT_MAX``)
- 静态变量: ``s_variable_name`` 前缀
- 全局变量: ``g_variable_name`` 前缀（尽量避免使用）

文档注释
--------

所有公共 API 必须有 Doxygen 注释。本项目使用 ``\`` 风格的 Doxygen 标签
（不使用 ``@`` 风格）。

标签对齐
~~~~~~~~

所有 Doxygen 标签必须对齐到第 20 列（``*`` 后面 17 个空格）。
这确保了整个代码库的格式一致性::

    /**
     * \brief           简要描述从第 20 列开始
     * \param[in]       param: 参数描述
     * \param[out]      result: 输出参数描述
     * \return          返回值描述
     */

文件头格式
~~~~~~~~~~

每个源文件必须有文件头注释::

    /**
     * \file            filename.c
     * \brief           文件的简要描述
     * \author          Nexus Team
     * \version         1.0.0
     * \date            2026-01-12
     *
     * \copyright       Copyright (c) 2026 Nexus Team
     *
     * \details         文件内容和用途的详细描述。
     *                  可以跨越多行。
     */

头文件 (.h)
~~~~~~~~~~~

头文件包含完整的 API 文档，包括参数和返回值::

    /**
     * \brief           创建互斥锁
     * \param[out]      handle: 用于存储互斥锁句柄的指针
     * \return          成功返回 OSAL_OK，否则返回错误码
     */
    osal_status_t osal_mutex_create(osal_mutex_handle_t* handle);

源文件 (.c)
~~~~~~~~~~~

源文件不应重复头文件中的 ``\param`` 和 ``\return`` 文档。
只使用 ``\brief``、``\details`` 和 ``\note``::

    /**
     * \brief           创建互斥锁
     * \details         分配并初始化一个新的互斥锁对象。
     *                  互斥锁创建时处于未锁定状态。
     * \note            线程安全函数。
     */
    osal_status_t osal_mutex_create(osal_mutex_handle_t* handle) {
        /* 实现代码 */
    }

分节注释
~~~~~~~~

使用分节注释将代码组织成逻辑块::

    /*---------------------------------------------------------------------------*/
    /* 分节名称                                                                  */
    /*---------------------------------------------------------------------------*/

行内注释
~~~~~~~~

使用 ``/* comment */`` 风格的行内注释，不使用 ``//``::

    int value = 0;  /* 初始化为零 */

MISRA C
-------

代码应符合 MISRA C:2012 指南。
