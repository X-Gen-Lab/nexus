# 实际测试报告验证

`tools/dev/dev.py test --preset native-debug` 删除旧 JUnit，调用 CTest 的
`--no-tests=error` 和绝对 `--output-junit`，保留实际退出码，再按开始时间核对新报告。

`junit.py` 拒绝缺失、符号链接、陈旧、格式错误、空、全跳过、失败／错误、矛盾计数、
重复或无名用例。部分跳过仍标为跳过。报告摘要不标识执行源码／配置，候选资格另绑
这些身份与原始日志。仓库根检查已有报告或运行验证器合同：

```sh
python scripts/validation/junit.py --report build/native-debug/ctest-results.xml
python -m unittest discover -s tests/contracts -p test_junit.py
```

Native、生产寄存器模型、ARM 链接／资源和物理 HIL 分别建立证据。
workflow 存在不表示执行发生，验证器不会制造结果。
