# Native TLS／MQTT 互通夹具

Linux 测试将 C11 TLS／coreMQTT 接到真实 Mosquitto2.0.22。POSIX nonblocking
socket 只是测试 byte-stream，不执行 lwIP、MAC／PHY、MCU entropy 或 Board，
不授予发布／实板资格。

client.c 用维护 ABI／build-options／严格告警／target-policy 链接
`Nexus::CoreMQTTStorage`、`Nexus::TLSStorage`，显式接 loopback port、case 和
fixture dir。一名 executor 以绝对单调 deadline／有限预算推进 TLS handshake／
service／close、MQTT process／keepalive，无隐藏 worker。host getrandom／UTC
不是产品 entropy／trusted time。

## 显式准备与执行

runner 要本地受管 Docker、OpenSSL3、已准备的不可变 image，不自动 pull host 工具
或增加固件依赖。准备 image 时保持已有 proxy／CA／registry 设置：

```sh
env -u DOCKER_HOST -u DOCKER_CONTEXT -u DOCKER_TLS \
    -u DOCKER_TLS_VERIFY -u DOCKER_CERT_PATH \
    docker --host=unix:///var/run/docker.sock pull \
    eclipse-mosquitto@sha256:09b62ff32c2e7b64d4b431b9a89236073289b68db273de2b089edeb9b0d92b5c
python -B -m tools.testing.mqtt_tls_interop \
    --client /absolute/path/to/native-client \
    --output-root /absolute/path/to/new-output-directory
```

output parent 必须已存在，每次新建私有目录和两天有效的 P256 CA／server／client
测试凭据。私钥仅留 host，不能进入 Git、SDK、固件或制造凭据。只把 broker 自有
CA／cert／key／config 只读挂载，container 用 caller UID／GID、移除 capability、
只读 filesystem、有界资源，只在127.0.0.1公开随机端口。

三个真实用例：

- mTLS1.2 ECDSA、清洁会话 CONNECT／SUBACK、QoS1 发布／回显／PUBACK、UNSUBACK、
  DISCONNECT 和本地接纳 close-notify；本地关闭不证明对端已读最后一条 alert。
- QoS1 frame drain 后真实 socket shutdown，要求 sticky IO、重试时零 transport call；
  不确定 delivery 的语义恢复仍归应用。
- 缺 client identity，必须由公共 TLS diagnostic 观察真实 peer fatal alert，不能以
  timeout、entropy／IO 错误冒充；broker 原始日志保留 certificate 拒绝原因。

报告绑定 image digest／ID、broker binary hash／version、OpenSSL、client、凭据和
命令收条。清理一次快照核对完整 container ID／精确 name／owner label，随后仅按
不可变 ID 操作，未知归属不触碰。受控重复 cancel 延后到有界撤回结束；SIGKILL 或
Docker 失联不保证清理，`owner.json` 留唯一恢复身份。不全局 stop／prune、不按名字
删除。primary／cleanup 失败分开保留，均使执行失败。

Python 合同覆盖 create client 撤回后清理、重复 cancel，只建立 host-tool 证据：

```sh
python -B -m unittest tools.testing.test_mqtt_tls_interop
```

[完整链路夹具](../lwip_mqtt_interop/README.md)用 `--transport lwip-test-relay` 和
独立 helper 运行5例真实 lwIP／TCP／TLS／MQTT，不改变默认3例的范围。

## 外部仓库持有的 client

不属于已审 POSIX／N09 的程序必须显式选择：

```sh
python -B -m tools.testing.mqtt_tls_interop \
    --transport external-client \
    --client /absolute/path/to/external-wrapper \
    --output-root /absolute/path/to/new-output-directory
```

复用 broker、测试CA、有界进程和清理，调用 clean／fault／missing-client 三普通
bundle，按既有 positional args 传 port／bundle／fixture dir。不附 N09 PCAP 或
stdout counter，不标成 POSIX，无插件注册表、第二 runner 或新 evidence schema。

公开结果只评估 broker／进程编排，外部 transport／TLS／协议为 `not assessed`。
exit0 不证明认证、网络、MQTT、MCU／PHY；外部 wrapper 的扩展场景需自有精确 binary、
config、child command 和协议断言，三个 bundle 不代替子结果。开发探针可标记明确
未提交工具源码但不资格化。最终交付用含选项的新 clean／approved SDK pin，工具
覆盖不能升级 SDK 身份；私有源码和原始证据留在私有仓库。
