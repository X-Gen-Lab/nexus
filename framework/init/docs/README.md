# Init 组件文档

当前公开功能是显式初始化注册表与外部固件元数据；没有接管 main 的 startup wrapper。

- [使用说明](USER_GUIDE.md)：显式运行、错误与生命周期。
- [设计](DESIGN.md)：链接段顺序与一次执行语义。
- [移植](PORTING_GUIDE.md)：平台 startup 与组件的边界。
- [测试说明](TEST_GUIDE.md)：真实注册与失败回归。
- [故障排查](TROUBLESHOOTING.md)：缺段、失败、RTOS 上下文。
- [变更](CHANGELOG.md)：破坏性 API 删除。

外部固件启动采用 `runtime/nx_runtime.h`，通用平台不拥有产品任务、服务顺序或健康策略。
