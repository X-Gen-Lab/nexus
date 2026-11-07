# AT24C02 同步 EEPROM 组件

`Nexus::AT24C02` 是可选 C11 组件，使用已有固定地址、同步 polling
`nx_i2c_endpoint_t`。它不创建 task、allocator、厂商 Handle、总线扫描、GPIO 恢复
或产品布局，未选代码不进入镜像。

软件参考精确为 AT24C02C：256 字节、8 字节物理页、A0～A2 接地、七位地址 `0x50`。
启明原理图仅标 AT24C02，实际后缀、焊接和 WP 接线仍需检查。公共 endpoint 不提供
地址内省，调用者必须提供精确已准入地址。初始化只检查方法和存储分离，不做 I/O，
不证明身份或器件在线。仅异步 endpoint 会被拒绝，不能让局部数组跨异步借用。

## 读写与完成条件

读使用一次字地址写加 repeated START 读，最后 STOP 必须结清；完整 256 字节范围
可由现有两消息合同表达。非法范围、空 span、溢出、控制对象别名或无限 deadline
在 I/O 前拒绝；已发出的读取失败可能留下部分输出。

写按每个 8 字节页边界拆分。每次同步交换只复制至多 8 字节 payload 加 1 字节地址
到局部存储；确认该页写周期结束后，才计入已确认字节并发送下一页。一个有限绝对
deadline 覆盖全部页和探测。页完成不等于数据回读、掉电原子性或耐久证明；WP 必须
为低，产品负责验证、回读与布局。

公共 I2C 不允许零字节地址探测，故就绪检查使用数据手册的一字节当前地址读，并以
已结清 STOP 结束。手册第 8 节允许按所需读／写方向 acknowledge polling，第 9 节
定义当前地址读。它不写 EEPROM，但推进私有地址指针；同地址所有用户必须串行，
其他使用者不能依赖该指针的中间状态。

仅重试本组件已发送页的排空阶段 NACK。ARBITRATION、IO、BUSY 等保留实际错误，
不伪装成器件忙。非零 `max_ready_probes` 即使在时钟停止前进时也限制尝试次数；
耗尽返回 BUSY，不伪造就绪。时钟必须有效且单调；观察到回退或耗尽后，须完全空闲
的软件 stop 再初始化，改变数值 epoch 不能证明待定操作的连续性。

## 借用、清理与恢复

全部操作是 task-only，由一个执行者串行调用。每个 endpoint 调用在所有返回路径
结束对 payload 和栈描述符的访问。endpoint、ops、总线存储、时钟保持存活，并与
器件、payload、result 分离；此层只能验证已知控制范围，完整 opaque context 范围
仍由装配保证。

`progress.confirmed_bytes` 只计先前已完成页；`progress.uncertain` 表示已发出但结果
未明的最后一页。错误和 timeout 无法撤销 EEPROM 脉冲。`write_pending` 保留期间，
后续数据访问和软件 stop 返回 BUSY。使用独立清理预算调用 `nx_at24c02_sync()`，
它只观察就绪、不重放数据，也不修正旧 progress 或证明所写字节正确。不提供物理写
取消。

不允许并发或 callback 重入；重入 read／sync／init／stop 返回 BUSY。stop 只释放
完全排空的本组件软件绑定，不停止 bus owner 或改变 Board 电源。外层先会合所有
用户，再撤回 endpoint 和 clock。

GoogleTest／GoogleMock 经公共同步 endpoint 执行真实 C 组件，覆盖全范围、页回绕
防护、NACK、意外错误、时钟、别名、部分进度和 stop／重入。主机行为、ARM／SDK
链接与实板验收分开；不建立物理写时序、数据保持、焊接或 HIL 资格。
