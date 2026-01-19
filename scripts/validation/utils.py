r"""
\file            utils.py
\brief           系统验证框架的工具函数
\author          Nexus Team
\version         1.0.0
\date            2026-01-18

\copyright       Copyright (c) 2026 Nexus Team

\details         提供跨平台的工具函数，如安全打印等。
"""


def safe_print(message: str) -> None:
    r"""
    \brief           安全打印消息，处理Unicode编码问题
    \param[in]       message: 要打印的消息
    \details         在Windows上将Unicode字符替换为ASCII等价字符
    """
    try:
        print(message)
    except UnicodeEncodeError:
        # 替换常见的Unicode字符
        replacements = {
            '✓': '[OK]',
            '✗': '[FAIL]',
            '═': '=',
            '─': '-',
            '│': '|',
            '├': '+',
            '└': '+',
            '→': '->',
        }
        for unicode_char, ascii_char in replacements.items():
            message = message.replace(unicode_char, ascii_char)
        print(message)
