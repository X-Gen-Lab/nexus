# OS 后端接入探针

`native.json` 是[后端接入合同](../../../docs/design/os-backend-integration.md)的可执行
示例，选择已有真实 Native GoogleTest，不声明新 kernel，不做运行时后端注册。
从仓库根先正常 configure，再用全新输出运行：

```sh
python tools/dev/dev.py configure --preset native-debug
python scripts/ci/os_backend_probe.py \
  --contract tests/contracts/os_backend_probe/native.json \
  --build-dir build/native-debug \
  --target nexus_os_wait_test \
  --output build/backend-probe-native-001
```

输出必须不存在，重跑换目录，不删除／覆写旧证据。合同覆盖多个注册 target 时重复
`--target`，每目标无过滤执行全部用例，核对 fresh XML／发现身份，再用 GNU size／
nm／readelf 测真实 ELF。

MCU 后端针对已配置生产 firmware build 另提供：

- `--firmware-build <build-directory>`
- `--firmware-target <reviewed-cmake-target>`
- `--firmware <linked-elf-inside-that-build>`
- `--cpu-resolution <generated-resolved-cpu.json-or-resolved.json>`
- `--compiler <arm-none-eabi-gcc>`

既有 resolver 必须已认识该后端，不能靠新 JSON 或源码图接入未注册 RTOS。
kernel 声明包含检出内 source lock、必须保留的静态 API、真实 scheduler case group，
不提供其他 RTOS 的占位声明。

report 保存 command exit、原始流摘要、source／build／tool、实际 cases 和 ELF。
host 标记 `host_elf_not_mcu_budget`，MCU 标记
`linked_mcu_elf_not_physical_timing`，均不证明板卡、周期、clean candidate 或产品。
`python -m unittest discover -s scripts/ci -p test_os_backend_probe.py`
覆盖声明、注册、coverage、CPU／ABI 和资源拒绝合同。
