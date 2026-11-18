# 关节控制契约 v2

状态：MC0 的关节协议设计基线，已提供 schema、C++ 生成及 wire 测试。OpenEmber 核心已有 [运行时校验与控制状态机](../../openember/docs/design/actuator-control-guard.md)、[消息适配](../../openember/docs/design/actuator-control-adapter.md) 和 [单 worker 调度/优先停止](../../openember/docs/design/actuator-control-loop.md)，已接产品 MuJoCo 做进程内闭环；**尚无 v2 HI 网络端点**。本文的“必须”仍是端到端验收要求，不能据此声称现有 v1 已获得这些保护。高层速度、运动模式与策略接口另行设计。

## 版本与接入

协议为 `openember.msgs.actuator.v2`，定义于 `proto/openember/msgs/actuator/v2/actuator.proto`。保留 v1 的全部字段和行为；禁止把 v2 内容发送到 v1 topic，或将 v2 自动降级为 v1。构建目标属性 `OPENEMBER_ACTUATOR_CONTRACT_VERSION=2` 仅证明消息可生成，不证明 HI 支持 v2。

建议分别使用 `<controller>/v2/capabilities`、`command`、`feedback`、`control` 服务与 `control_result` topic。产品需在 NodeInfo 声明 `actuator.control.v2`，查询到 `contract_version=2` 和完整能力后才发送目标。无能力、未知版本、字段缺失均拒绝控制。具体 namespace 由产品确定，仿真和真实设备必须隔离。

控制权令牌用于排除旧命令，不是用户认证或传输加密。部署仍须在接入层限制哪些调用者能获取控制权或调用停止接口。反馈、日志、广播能力不包含令牌。

## 身份、时钟与有效期

`CommandContext` 中的 robot/controller/session/model_revision 必须与当前能力完全一致，所有字符串非空。`model_revision` 是控制器公布的拓扑、单位、坐标、限值和映射版本标识，配置变化必须更新。生产者每次进程启动生成新的 `producer_session`，不得复用。

首版只接受同机在线控制：`clock_domain` 为 `host-monotonic:<boot-id>`，时间来自该主机 `CLOCK_MONOTONIC`，单位 ns，不使用 Unix 时间或仿真时间。发送端必须取得本机 boot-id 并核对接收端公布的时钟域，不能直接复制远端字符串冒充同机时钟。跨主机、跨启动及离线加速网络控制暂不支持；未知域必须拒绝。

接收/实际应用时须满足 `0 < issued_time_ns <= now < expires_time_ns`，且 `expires-issued <= max_command_duration_ns`。所有加减先校验溢出；未来时间、过期、零时间、过长期限均拒绝，无隐式填入接收时间。服务请求同样受期限约束，排队操作完成前重新检查上下文、期限和控制权。

控制器启动或重置生成新的 `controller_session`，清除目标、序号与租约，进入 DISARMED。仿真 reset 同样轮换 session，策略 history 随之清空。真实后端重连也不得恢复旧控制。

STOPPED 为当前 session 的终态，拒绝新的控制请求和目标；重新启动须更换 session。UNSPECIFIED 不是可控制状态，不能视为 DISARMED。

每个租约世代有独立的目标序号与管理请求序号流，均从非零值严格递增；不比较两条流的序号。序号不允许回绕，耗尽后重新获取租约。失败校验不能推进有效序号或刷新 watchdog。获取新租约使旧世代所有序号关联失效，反馈的 accepted/applied 序号及 accepted 时间归零。

## 控制权与管理操作

`ControlRequest` 一次只能指定一个非 UNSPECIFIED 操作。`request_id` 在生产者会话内唯一，用于结果关联。通过上下文、权限、基本形状和重放入口校验后，相同 ID、完全相同请求的重试返回已有结果，不再执行、不续租；相同 ID 不同内容拒绝。入口校验直接拒绝且未执行的请求不要求缓存。只保证在请求有效期内缓存幂等结果；缓存淘汰后的过期请求仍须拒绝，不能再次执行。结果按 robot/controller/session/producer/request_id/request_sequence/operation 共同关联。重试返回的 state/lease 到期时间属于原执行结果，当前状态以反馈为准。

| 操作 | 前置条件与结果 |
|---|---|
| ACQUIRE | DISARMED、无持有者；请求中 lease 为空，duration 为正且不超过能力上限。返回非零 generation、不可预测 token、绝对到期时间；获取本身不使能。 |
| RENEW | 当前持有者、generation/token/producer 全部匹配，旧租约未过期；duration 合法。成功后到期时间为接收 now + duration，不刷新目标 watchdog。 |
| RELEASE | 当前持有者；清除目标、撤销租约并禁用输出，完成后 DISARMED。 |
| ARM | 当前持有者、DISARMED、后端就绪且无锁存故障。进入 ARMED，首个有效完整目标到来前输出仍禁用；从完成 ARM 起计 command_timeout，未及时收到目标则锁存 FAULT。 |
| DISARM | 经接入层授权的调用者可执行，不要求持有者令牌；禁用输出、清除目标、撤销租约。不能解除已有 FAULT/EMERGENCY_STOP 锁存。 |
| EMERGENCY_STOP | 经接入层授权，不要求令牌；停止、撤销租约并锁存 EMERGENCY_STOP。独立现场急停仍须由后端提供。 |
| CLEAR_FAULT | 仅 FAULT/EMERGENCY_STOP，原因已消除且后端允许复位；不要求已被撤销的租约，但要求复位授权。issued_time 必须严格晚于本次故障锁存时刻，旧请求按 REPLAY 拒绝。完成后 DISARMED，不使能、不获取控制权。 |

DISARM/EMERGENCY_STOP/CLEAR_FAULT 仍要求有效上下文、期限及请求去重；其管理序号以 `(controller_session, producer_session)` 单独追踪，不能被非持有者用高序号挤掉持有者流。其他操作使用租约内管理序号；ACQUIRE 使用获取请求流 `(controller_session, producer_session)`。不适用的 lease/duration 必须为空/零，禁止含混解释。

租约到期、目标超时、所有权变更均清除待执行目标并禁用输出。租约到期或异常丢失进入 FAULT；显式 RELEASE/DISARM 正常进入 DISARMED（已有锁存状态除外）。generation 在同一 controller_session 内严格递增，撤销的 token 永不重新生效；世代溢出必须停止并更换 session。

服务可以直接返回 COMPLETED，也可先返回 ACCEPTED，再由结果 topic 报告 COMPLETED/FAILED；ACCEPTED 只表示操作排队，不表示后端已使能或已复位。REJECTED 表示未开始，FAILED 表示执行未完成且输出必须回到禁用/锁存故障。失败结果不返回 token。停止操作不能因普通请求队列满而丢弃，具体独立停止路径在 HI 实现中验收。

## 周期目标与模式

`JointCommand` 只携带目标，没有 enable/disable/clear 布尔值。控制器仅在 ARMED、租约未过期时接收当前持有者命令。每条必须包含能力中的全部关节，各名称恰好一次，顺序任意；缺失、重复、额外关节或超过 32 项整条拒绝。不存在 partial update。任何一项不合法，整条不应用、不刷新 watchdog。

所有数值必须有限，未使用字段必须为零，未知枚举拒绝。支持模式按每关节能力判断，不能仅因枚举存在就接受。

| 模式 | 使用的数值字段 |
|---|---|
| DISABLED | 全部为零，该关节禁用输出；不改变整机控制权。 |
| POSITION | position、max_effort；后端需公布支持，否则拒绝。 |
| VELOCITY | velocity、max_effort。 |
| EFFORT | effort、max_effort。 |
| MIT | position、velocity、effort、stiffness、damping、max_effort。`τ = effort + Kp*(position-q) + Kd*(velocity-dq)`。 |

非 DISABLED 的 `0 < max_effort <= capability.max_effort`；effort 绝对值不超过本条 max_effort，velocity 绝对值不超过 max_velocity，Kp/Kd 非负且不超过能力。非连续关节的位置在闭区间限值内；连续轮轴不套用腿轴位置限位，MIT 位置误差采用连续展开坐标的直接差值，不隐式取最短角。速度、力矩和增益限值适用于连续轴。所有单位为 SI，名称和坐标由 model_revision 固定。

语法/限值越界拒绝整条，正常 PD 求和造成的输出力矩则限幅到本条 max_effort 和后端硬限制的较小值。接收端必须分别记录 accepted 与 applied 序号，入队不等于实际写出。执行前再次校验期限、session、租约及状态，不能回放过期积压目标。只有完全合法的目标可刷新目标 watchdog，管理请求、心跳和被拒绝目标均不能刷新。

首次 ARM 后及运行中的有效目标失联阈值由 `command_timeout_ns` 给出。当前目标只能执行到目标 expires、租约 expires、watchdog 到期中的最早时刻；任一到期清除目标、禁用输出并锁存故障。具体电机停止动作由后端定义和验证，不能用零力矩报文等价替代硬件禁用。

## 能力与反馈

所有能力限值须有限且合法：duration/timeout/反馈最大年龄为正，min<=max，速度/力矩上限为正，增益上限非负，模式集合非空且无重复/未知项。关节名称唯一，1–32 项。能力在一个 session 内不可变，变化即新 session。首版 `contract_version` 必须等于 2。

`ControllerFeedback.sequence` 是状态发布序号，可以在暂停/无新样本时增加；`publish_time_ns` 不能代表采样时间。每个 `JointFeedback.sample_sequence` 和 `receive_time_ns` 仅在该关节取得新物理反馈时增加/更新，不能由整组发布刷新。sample_sequence=0 表示尚无样本。接收时间在公布的主机时钟域内，消费者自己计算年龄并同时检查 `receive_fresh` 和逐字段 valid；`now-receive_time > max_feedback_age_ns`、未来时间或无样本均不可用于控制。后台持续发布不代表每个关节都健康。

有效值可以为零；无效数值字段发送零，消费者不使用。未模拟/未测得的温度、电压和电气故障分别声明 invalid，不能填有效零值。effort 是实际测得/求解的关节力矩，不是请求目标或 actuator 未经 gear 换算的 ctrl。fault_code=0 只有 fault_valid=true 才表示已知无故障。持续禁用不意味着物理位置/速度反馈无效。

accepted/applied 序号仅属于反馈中 owner_generation + owner_producer_session，不能跨所有者关联；无持有者时两者和 accepted_time 均为零。`last_accepted_command_time_ns` 为接收方接受时刻，不能替代命令原始 issued/expires 校验。拒绝计数在 session 内累计，reason 无拒绝时为 UNSPECIFIED。

反馈字段 22 `backend_failure` 锁存后端操作失败，23 `fault_reason` 保留期限/时钟/后端等原因，24 `lease_expires_time_ns` 在主机时钟域中表示当前租约期限；无持有者为零。禁用/急停失败不能报告已完成，也不能因反馈默认值为 false 就推断硬件已停止。新字段采用新增编号，不改动旧字段。

仿真另报告 physics_step/simulation_time_ns。暂停时不更新逐关节样本、物理步和仿真时间；只允许发布状态。进入暂停撤销租约、清目标并禁用输出，恢复后需重新获取与 ARM。暂停/单步期间拒绝网络 ARM/目标；单步仅供本地调试。reset 更换 session，物理步/时间与样本序号归零；第一个实际物理步后才产生新采样。真实后端 simulation=false、paused=false、仿真步/时间为零。离线加速控制时钟后续单独设计。

## 尺寸和验收

Nanopb options 为所有字符串和 repeated 设定静态上限；C++ 接收端也必须校验这些上限，禁止截断身份字符串。max_size 包含末尾 NUL：普通标识及 token 最多 63 字节，clock_domain/model_revision 最多 127 字节，message 最多 159 字节。标识使用非空 ASCII 且不得含 NUL；joint_name 按模型约定。模式最多 5 项，关节最多 32 项。

本仓库自动化验证 wire/default/字段隔离和生成；核心与 HI 的离线测试另覆盖消息适配、命令校验、控制权和状态转换。HI 网络端点/仿真尚未接通，以下端到端行为仍须验收后才可开放 v2 控制：

| 组别 | 必测行为 |
|---|---|
| 身份/期限 | 旧 session、错误机器人/模型、跨 boot 时钟、零/未来/到期时间、溢出、排队后过期拒绝 |
| 租约 | 双持有者冲突、过期续租、旧 generation/token、换主后积压目标、幂等重试与冲突 request_id |
| 目标 | 重复/乱序、缺失/重复关节、未知模式、NaN/Inf、超限、连续轴、不合法目标不刷新 watchdog |
| 生命周期 | 默认禁用、获取不使能、ARM 无目标超时、accepted 不冒充完成、掉线/退出停止、故障不得自动解锁 |
| 反馈 | 零值有效、温压未知、单轴停更、聚合发布不刷新样本、accepted 与 applied 区分 |
| 仿真 | 暂停撤权、恢复重新 ARM、reset 旧命令失效、单步不接受网络目标、真实/仿真隔离 |

运行现有协议测试：

```bash
cmake -S . -B build/contract -DOPENEMBER_MSGS_BUILD_CPP=ON -DOPENEMBER_MSGS_BUILD_TESTS=ON
cmake --build build/contract
ctest --test-dir build/contract --output-on-failure
```
