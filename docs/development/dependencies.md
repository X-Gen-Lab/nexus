---
id: development.dependencies
type: how-to
audience: maintainer
source_language: zh_CN
---

# 维护依赖来源与精确选版

外部 Git 源先由 X-Gen-Lab 托管，Nexus 以精确 gitlink 和锁选择版本。官方来源、
实际获取地址、选中提交、许可与本地修改分别记录，不以版本名代替源码身份。

## 按实际任务准备

`tools/maintenance/source_catalogue.py` 维护来源与已审供应方映射，package 声明
其硬件依赖，TOML 选择后端与组件。CMake 仍拥有实际翻译单元和链接图，不增加另一
份源码清单。未选 vendor 库存保留，但不进入默认准备、构建或 SDK 输出。

```sh
python tools/dev/dev.py prepare --scope preset \
  --preset stm32-qiming-baremetal --allow-network
```

已有外部应用可按其 TOML 运行 `prepare --scope assembly --assembly <path>`；Native
明确加 `--test-support`。`--scope native` 则仍表示平台维护的完整宿主测试图，包含
GoogleTest／GoogleMock和维护测试实际使用的运行依赖。

准备器默认离线核对来源、真实 gitlink、HEAD、工作区、递归子模块与别名身份。
仅显式 `--allow-network` 才允许获取选中闭包，不静默改 origin 或选择更新版本。
源码消费和发布使用同一锁；安装 SDK 通过自己的完整 manifest 校验原字节。

## 更新依赖

1. 固定上游变更及许可证，选择组织托管的精确提交，并核对保留引用与获取方式。
2. 明确本地补丁、递归来源、alias 和所选平台／后端／组件受影响范围。
3. 更新实际 gitlink 与匹配锁，执行最小失败回归和真实构建上下文。
4. 对受影响的 Native、ARM 与独立 SDK 消费执行相应门禁，保留原失败和源码身份。
5. 普通提交与评审通过后合并；既有标签与旧产物保持原始身份。

嵌套来源使用所属父提交的真实 `.gitmodules` 与 gitlink。不能用路径、子模块名、
组织 fork URL 或上游同名 tag 互相替代。未经验证的目录、脏源码与缺失许可证必须
拒绝；依赖变更不自动扩大 MCU、模式或物理支持。

Native 最小消费与完整维护图是不同选择，维护范围不会因教程优化而削弱。
工具准备缓存可复用，历史成功报告不作为新源码验收。发布范围见
[SDK 发布](sdk-publishing.md)，代码接口／schema 代际见
[SDK 格式合同](../reference/contracts/sdk-format.md)。
