编码规范
========

Nexus 遵循严格的编码规范以保证质量和安全。

C 标准
------

- 源代码使用 C11
- 测试代码使用 C++17

命名约定
--------

- 函数: ``module_action_object()`` (例如 ``hal_gpio_init()``)
- 类型: ``module_type_t`` (例如 ``hal_status_t``)
- 宏: ``MODULE_MACRO_NAME`` (例如 ``HAL_OK``)
- 常量: ``MODULE_CONSTANT`` (例如 ``HAL_GPIO_PORT_MAX``)

文档
----

所有公共 API 必须有 Doxygen 注释::

    /**
     * \\brief           简要描述
     * \\param[in]       param: 参数描述
     * \\return          返回值描述
     */

MISRA C
-------

代码应符合 MISRA C:2012 指南。
