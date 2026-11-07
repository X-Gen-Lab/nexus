# Wait ABI 成本模型

`scripts/ci/os_wait_cost.py` 比较生产四 pointer `nx_wait_port_t` 与实验二 pointer
face＋共享只读 ops；实验布局只存在本 C fixture，不改变生产 ABI。

从当前检出使用维护 ARM GNU 执行：

```sh
python scripts/ci/os_wait_cost.py --output build/os-wait-cost-fresh
```

输出须全新；旧证据、symlink 目标拒绝，不删除原件。工具记录精确源码／编译器、
argv／exit／raw log、48 个 ELF、size／nm、分派反汇编与 ABI。缺工具、符号、表可写、
执行不完整、输入变化都失败。

模型覆盖 M0+／M4／无 DSP M33、Os／O2、1／2／4／8 face 与两种布局；CPU／enum
参数来自生产 resolver。地址为软件参考，不新增 SoC／Board／startup，不跑硬件。

默认保留直接 callback，单一 notification 的 arm／wait／wake 无须额外 table load。
共享布局每 ARM face 省8 B RAM，但加12 B readonly table 和分派 load；多实例也可能
减少总 Flash，必须看所选 profile 和实际 ELF。分配量／指令数不证明周期、WCET／
jitter。应用 spill、临时 by-value port、LTO 和布局仍需真实镜像测量。
