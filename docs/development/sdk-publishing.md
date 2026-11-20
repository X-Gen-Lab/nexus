---
id: development.sdk-publishing
type: how-to
audience: maintainer
source_language: zh_CN
---

# 导出与验收 Source SDK

本页面向维护者。普通应用从已选源码或 SDK 消费，不执行整套 SDK 发布验证。
先冻结干净、已提交的平台源码，再选择 package、后端和可选组件。

## 选择交付范围

默认导出是精简 F4 裸机源码包；Native 模型和中间件须明确选入。包内容来源于
package 导出声明、选中依赖和许可记录，CMake 继续拥有源码／链接图。
完整维护消费者计划还需要 FreeRTOS、Native 和下列组件；从平台根执行：

```sh
python tools/dev/dev.py prepare --scope sdk --backend freertos --host-support \
  --component network --component network-loaned-rx --component tls \
  --component mqtt --component mcuboot --component st-usb-device \
  --component st-usb-host --allow-network
python cmake/package/package_source_sdk.py --source . --output /tmp/nexus-sdk-export \
  --package stm32f4 --backend freertos --host-support \
  --component network --component network-loaned-rx --component tls \
  --component mqtt --component mcuboot --component st-usb-device \
  --component st-usb-host
```

输出路径必须属于本次任务且尚未存在。制作器核对 clean source、gitlink、选中递归
依赖与许可，记录精确 source revision／tree。默认包不能借完整计划的名义跳过缺失
消费者；小型包的完整性与其产品消费范围分别验收。

## 搬迁并执行实际消费者

```sh
mv /tmp/nexus-sdk-export /tmp/nexus-sdk-relocated
python cmake/package/package_source_sdk.py \
  --verify /tmp/nexus-sdk-relocated/share/nexus/src
python cmake/package/verify_consumers.py \
  --prefix /tmp/nexus-sdk-relocated --output build/sdk-consumers \
  --parallel 2
python cmake/package/package_source_sdk.py \
  --verify /tmp/nexus-sdk-relocated/share/nexus/src
```

当前完整计划由 `tools/maintenance/consumer_profiles.py` 维护。读取新生成的
`verification.json`、每条命令退出码、Native 运行结果、MCU ELF／BIN／map 和
资源报告。`--parallel` 是该调用的总 worker 预算；同时执行其他工作时由协调者
分配预算。不要缓存成功报告、过滤完整验收或把缺失执行补成通过。

SDK prefix 与验证输出必须是独立、无 symlink 别名的规范路径，不互相包含。路径
拒绝不清除无关目录；已接纳的验证器只管理自己的输出。

## 发布收条

收条至少关联平台 source revision／tree、profile、依赖、manifest、实际消费者范围、
命令与退出码、产物及失败原件。消费前后完整性须一致；制作可下载归档时再验证
归档成员及校验和。软件结果、文档来源、版本标签和物理记录保持各自身份。

维护源码不得依赖开发 fixture opt-in 获得正式包资格。`--development-fixture`
生成的包为 nonpublishable；许可、来源、接口代际等错误不因 opt-in 放宽。
签名密钥、产品分区、制造数据和产品发布审批留在所属外部工程。

示例仓先更新对应 gitlink、依赖锁、schema 与模板摘要，再执行其正常门禁。平台与
示例的提交分别记录。验收工具与公开 API 见
[文档与维护工具参考](../reference/documentation-tools.md)，实际消费见
[使用 Source SDK](../guides/use-source-sdk.md)。
