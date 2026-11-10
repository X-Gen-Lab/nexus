# 平台维护检查

本目录维护精确依赖来源、选中配置和公共接口版本的检查。
厂商库存及原始许可证保留，未选中的系列不进入默认准备、编译和 SDK。

```sh
python tools/maintenance/capabilities.py --check
python tools/maintenance/compatibility.py
python tools/maintenance/ownership.py
```

`capabilities.py --assembly <assembly.toml>` 调用真实 resolver，输出器件、板卡、
后端、模式、资源 owner 和明确限制。配置通过不代表模型、链接或 HIL 已执行。
`consumer_profiles.py` 是真实 SDK 消费者配置的唯一列表，由消费者验证器复用。
来源检查工具验证 gitlink、源码和许可证，不下载或自动修改来源锁。
