# 公共运动控制契约 motion/v1

状态：协议定义与 wire 测试基线。消息位于 `proto/openember/msgs/motion/v1/motion.proto`；产品侧已提供运行时校验、统一 Controller 和本机 Link 开发版服务，见[产品实现](../../openember-quadruped/docs/motion-link-control.md)；本消息仓库不自动注册服务或授予设备权限。本文中的“必须”是后续实现与验收要求，Protobuf 解码成功不代表已经通过这些检查。

## 范围与不同机器人形态

本契约描述机器人高层运动意图，不包含腿数、关节数量、CAN 路由、模型 tensor 或机型姿态。四足、人形、轮足、差速轮式和全向轮式机器人可以使用相同的消息与控制流程，具体支持范围由能力和当前准入条件决定，不通过机器人形态字符串猜测。公共协议兼容不代表 OpenEmber 当前已有相应驱动、策略或整机验证。

第一版支持地面机器人的平面速度、模式管理及无参数具名动作。六自由度运动、机体姿态/高度目标、足端轨迹、全身关节轨迹、操作臂任务和导航目标不在本版范围内，需要实际需求、坐标和生命周期契约后再以独立类型扩展；不能把这些数值塞进 action_id、message 或未知字段。

| 产品能力示例 | vx | vy | yaw_rate | 具名动作示例 |
| --- | --- | --- | --- | --- |
| 差速轮式 | 支持，按底盘限值 | 不支持，必须为零 | 支持，按底盘限值 | 可以没有站坐动作 |
| 全向轮式 | 支持 | 支持 | 支持 | 按产品声明 |
| 四足/轮足 | 按策略能力 | 按策略能力 | 按策略能力 | 正常站立、坐下；翻身独立声明 |
| 人形 | 按已验证步态能力 | 可能支持，也可能不支持 | 按步态能力 | 正常起立、下蹲或恢复按各自能力声明 |

这些是协议表达示例，不是当前软件的支持清单。当前 P05W 无经验证的翻身实现，应将恢复能力声明为 NOT_SUPPORTED；普通站立不代表能从倒置姿态翻身。

## 消息分工

| 消息 | 职责 |
| --- | --- |
| MotionCommand | 完整的物理单位速度目标；不获取控制权、不使能、不切换模式 |
| MotionRequest | 单一管理、模式、动作或取消意图，带独立 request_id |
| MotionResult | 关联原请求的接受、执行和终态结果 |
| MotionCapabilities | 支持/当前可用的模式、动作、速度范围、时限、停止行为与版本 |
| MotionState | 当前控制模式、授权/使能状态、请求执行情况、拒绝原因及状态心跳 |

`MotionRequest.intent` 使用 oneof 隔离 ControlIntent、ModeIntent、ActionIntent 和 CancelIntent。空意图、未知意图及 UNSPECIFIED 操作不能解释为默认操作。周期速度和管理请求使用独立序号流；模式与动作共用管理流，不能将速度包当作动作完成通知。

## 坐标、单位与速度能力

`PlanarVelocity` 中 vx/vy 单位为 m/s，yaw_rate 为 rad/s。命令参考系为能力公布的 `body_heading_frame_id`：原点随机器人基座，z 轴沿重力反方向，x/y 为机体名义前进方向对应的水平朝向轴，x 向前、y 向左；从上向下看逆时针转向为正。它不是地图坐标系，也不是随 roll/pitch 倾斜的三维机体坐标系。产品必须说明基座与名义前向的安装变换，负责转换成其策略或底盘所用坐标；大姿态下朝向不可可靠确定时，普通速度运动不可准入，已准入的动态动作由自身阶段规则处理。

`MotionCommand.frame_id` 必须与能力逐字相同，不支持接收端猜测 frame 或自动接受 world/map 坐标。velocity 子消息必须存在，且始终表示全部三个轴；没有部分更新或沿用上一包未给出的轴。子消息存在且三个数值为零是有效零速目标，缺少子消息则非法。数值均须有限。

速度能力中的三个 AxisLimits 都必须存在。supported=false 时 minimum=maximum=0，对应命令必须严格为零；supported=true 时必须满足 finite(minimum/maximum)、minimum<=0<=maximum 且 minimum<maximum。单向运动可将一端设为零。正负方向可不对称；除非另行明确，min/max 是闭区间，超限或非零的不支持轴整包拒绝，不能静默丢弃横移或将超限请求截断后声称已接受。

输入设备的归一化、死区和档位缩放属于客户端/产品输入适配，发送时已经是物理 SI 意图。产品控制器在能力范围内执行平滑与动态约束；速度目标不是实测速度或必然能达到的速度。P05W 的满杆前进1.0 m/s与最终上限2.0 m/s可以同时成立，公共能力描述后者，手柄 profile 描述前者。动态耦合约束、接触及模式进入条件仍可导致 UNAVAILABLE/INVALID_STATE，单轴落在范围内不保证任意三轴组合都可执行。

## 身份、时钟和会话

CommandContext 的 robot_id、motion_id、motion_session、model_revision、configuration_revision、source_id、producer_session 和 clock_domain 均必填；sequence>0，0<issued_time_ns<=now<expires_time_ns，并要求 expires-issued 不超过 max_command_duration_ns。该上限同时约束管理请求的准入期限，不是动作执行时限。接收和实际消费/开始执行前都重新检查，入队不能冻结有效期。

首版在线控制限制同机、相同主机单调时钟域，clock_domain 必须包含 host boot 身份；跨主机不能直接比较 monotonic 数值，也不能用收到旧包后重新启动 TTL 的方式绕过原期限。远程客户端未来通过受控网关转换为本机命令，远端期限与权限须独立设计。本版不支持仿真加速时钟直接替代在线授权时钟。

motion_session 是高层控制服务的执行代次，必须区别于生产者 session 和 HI 的 controller_session。服务重启、仿真 reset、切换后端或模型、静态限制/配置修订变化均产生新 motion_session，清除目标和授权并保持未使能；不能复用旧 token。model_revision 固定模型/坐标约定，configuration_revision 固定本轮运行参数，均逐字匹配。动态 availability 的正常变化不强制更换 session，但开始和执行期间仍检查当前准入；不能借此静默修改静态限值。

sequence 在同一 motion_session 和生产者内严格递增，周期命令与管理意图分别追踪；非持有者不得用高序号挤掉持有者的流。source_id 由接入配置关联控制来源，producer_session 标识该来源的一次运行；这些字符串和租约 token 本身不是身份认证。端点必须把 source、权限及可使用的 robot/motion 与经过授权的连接绑定，不接受客户端自行声明优先级。

## 控制权与使能

MotionLease 只控制高层意图的唯一持有者，由高层服务生成并维护。携带 token 的 command/request 传输也须限制订阅/读取权限，不能向任意观察者广播凭证。HI actuator/v2 的低层租约归产品控制 owner 私有；两个租约不能互换，外部客户端不能获取低层 token 或直接发布关节目标绕过仲裁。高层服务持续负责 HI 续租、有效目标和设备保护。

| ControlIntent | 前置条件与完成行为 |
| --- | --- |
| ACQUIRE | 无持有者、DISABLED；或 WAITING_ASSISTANCE 且人工处置重新准入条件已满足。请求 lease 为空，duration 为正且不超过能力。返回新 generation/token 和绝对期限，仅获得控制权，不使能或站立。 |
| RENEW | 当前 source/producer/generation/token 匹配且租约尚有效，duration 合法。到期时间为接收 now+duration；续租不刷新周期目标 watchdog。 |
| RELEASE | 当前持有者；解除使能、清除目标并撤销租约，停止动作完成后再报告成功。不能清除已有锁存故障。 |
| ARM | 当前持有者、DISABLED、readiness=READY，必要输入与输出后端就绪。完成后进入产品已验证的 PASSIVE 模式，不隐式站立、恢复或行走；不支持安全 PASSIVE 的实现不能宣称符合本版 ARM 流程。 |
| DISARM | 经授权的停止调用者，不要求持有者 token；解除使能、清目标、取消活动请求并撤销租约。已有锁存故障和人工处置要求不能被此操作清掉。 |
| EMERGENCY_STOP | 经授权的急停调用者，不要求 token；优先停止、撤销租约并锁存 EMERGENCY_STOP。现场独立急停仍由设备路径保证。 |
| CLEAR_FAULT | 仅 FAULT/EMERGENCY_STOP，原因已消除、具有复位权限，且 issued_time 严格晚于锁存时刻。完成后 DISABLED，不获取租约、不使能、不恢复旧请求。 |

后端无法确认解除使能时，actuation_state 必须为 UNKNOWN，不能用默认值证明已经安全停机。不使用 lease 的操作必须省略该子消息，非 ACQUIRE/RENEW 的 requested_lease_duration_ns 必须为零。generation 在 motion_session 内严格递增，token 不可预测且不得截断；溢出停止并更换 session。优先停止必须独立于普通队列拥塞；不能因排队中的 ARM/动作晚完成而再次使能。解除使能或急停后端失败不能报告成功。

高层租约到期立即清除意图、取消执行并解除使能，终止原授权；后续续租或新速度不得自行恢复。故障是否锁存按具体原因决定并报告，不能只因倾角变化统一锁死。停止、反馈/授权失效优先于同帧模式或动作成功。没有可用执行者、默认值或状态心跳都不能成为使能依据。

## 模式和动作

MotionMode 表达高层控制状态，actuation_state 独立报告 DISARMED、ARMED 或 UNKNOWN；它不是策略内部 stand/locomotion 子模型名称。DISABLED、FAULT、EMERGENCY_STOP、STOPPED 和当前采用解除使能响应的 WAITING_ASSISTANCE 均不产生新的运动输出。PASSIVE 仅表示产品声明的被动/阻尼控制，可以处于未使能或已使能；HOLD 是产品验证的静止保持，不承诺固定六自由度位姿，也不自动执行站立。LOCOMOTION 接收周期速度；ACTION 表示正在执行具名技能。

ModeIntent 只允许请求 PASSIVE、HOLD、LOCOMOTION，且必须由能力声明支持、当前 AVAILABLE，并具有有效租约、使能和新鲜必要输入；其余枚举是状态报告，客户端不能用 SetMode 伪造 FAULT、清急停、直接进入 ACTION 或跳过人工处置。能力中的 modes 只列这三个可请求模式。普通动作执行中不接受替代模式/动作请求，返回 BUSY；显式 cancel、DISARM 和急停保留独立路径。

MotionState/MotionResult 的 mode_generation 是本 motion_session 内的模式/技能执行代次，有效状态从1开始，零不提供速度准入依据。每次实际离开或重新准入速度模式、开始/结束动作、进入人工处置或其他输出执行边界均严格递增并清除旧速度；溢出停止并更换 session。LOCOMOTION 完成准入后公布当前非零代次，每个 MotionCommand 必须携带相同的 mode_generation。这样即使仍持有同一租约，切到 HOLD 再切回后收到的旧速度也会被拒绝，不能只清空本地队列却接受网络中延迟的旧包。精确重试不推进代次；没有实际转换的同模式请求也不能重置 watchdog。

进入 LOCOMOTION 的请求在产品进入条件满足后才成功；从成功时起等待首个有效周期命令，之后依据 command_timeout_ns 检查指令流。切到非速度模式立即丢弃旧速度，重新进入 LOCOMOTION 必须使用新的有效命令，不能沿用积压包或旧 history。零速是减速到零的意图，不等于解除使能；切换请求需在过渡/停止实际完成后才报告成功。

ActionIntent 仅接受能力中逐字匹配的 action_id，执行前检查当前 availability 和动作自身姿态/输入条件。缺失的 action_id 等同 NOT_SUPPORTED，显式 NOT_SUPPORTED 同样拒绝；UNAVAILABLE 表示有实现但当前不能执行，不能降级成另一个动作。所有动作都要求当前持有者和已使能；不会因为请求 stand 就偷偷 ARM。

预留公共动作标识及语义为 `openember.motion.stand.v1`（在该动作声明的正常支撑姿态下站立）、`openember.motion.sit_down.v1`（坐下/降低到机型定义的支撑姿态）、`openember.motion.recover.v1`（从动作明确支持的倒地起始姿态恢复）。它们不是必选项；特别是 stand 不表示 recover。产品专用动作使用自己拥有的命名域和版本，例如 `vendor.robot.crouch.v1`；未协商的自由字符串、模型路径或代码入口不允许执行。

AVAILABLE/UNAVAILABLE 的动作须公布正的 max_duration_ns 和成功 completion_mode；completion_mode 只允许 DISABLED、PASSIVE、HOLD，并验证相应能力。NOT_SUPPORTED 的动作 duration 为零、completion_mode=UNSPECIFIED。动作内部负责规划/反馈完成判据，成功回到声明的静止状态，不自动恢复旧行走；恢复技能仍由同一输出 owner 执行。最大动作时限从实际开始时按主机单调时间累计，执行期间继续要求租约、反馈和能力可用，不能以周期请求重发延长。

CancelIntent 的 target_request_id 必须指向本持有者当前正在执行的动作。有效取消优先于同帧动作完成；本版取消执行后解除使能并撤销租约，原动作报告 CANCELLED，取消请求在停机确认后报告 SUCCEEDED。目标不存在、已终结或不是动作则拒绝，不得取消其他来源的请求。取消不等于坐下，正常坐下应请求相应具名动作。动作超时报告 TIMED_OUT，并停止输出、解除使能；后端失败则按实际故障处理。

## 指令丢失、检测和人工处置

有效速度只允许使用到目标 expires、最近有效目标接收时间+command_timeout、租约 expires 中的最早时刻。管理请求、心跳、重复包或非法目标均不能刷新周期 watchdog。命令过期/停更立即丢弃旧意图，不再继续执行它；command_loss_response 指定经验证的 STOP_AND_HOLD 或 DISARM。STOP_AND_HOLD 只在租约仍有效且 HOLD 能力/反馈可用时，以新的零速停止计划在 stop_timeout_ns 内完成，再进入 HOLD；不能把旧目标视为仍有效。无法停止或保持时解除使能并报告失败/故障，租约过期始终解除使能。停止完成后新速度不会自动重启 LOCOMOTION，需新的显式模式请求。

倾角、低高度、接触等检测通过独立 alerts 协议上报；运动状态报告产品实际选择的处理。不存在“收到任意 warning 就停机”或“条件清除自动使能”的公共规则。WAITING_ASSISTANCE 表示需要明确处置，重新获取控制权还需该机型的准入条件；本版状态不推断机器人是否具有翻身能力。攀爬、翻转、恢复等技能可以经过大倾角，具体响应属于具备能力的产品动作上下文。

## 请求结果、重试与状态

ACCEPTED 只表示已排队，RUNNING 表示已开始；SUCCEEDED 必须有实际模式、动作或后端操作完成的依据。REJECTED 表示未获准进入执行队列；FAILED、CANCELLED、TIMED_OUT 是已接受请求的终态，已接受但开始前期限耗尽也须报告 TIMED_OUT 而非继续执行。状态转换后原活动请求必须有唯一终态；不能先成功又因相同完成回调取消。动作与管理可直接返回终态，也可异步报告中间状态。

幂等键为 `(motion_session, source_id, producer_session, request_id)`，同时保存原管理 sequence 和完整意图指纹。相同内容重试返回当前/已记录结果，不重新执行、不续租或延长动作；相同 ID 携带不同 payload/sequence/lease 是 REPLAY。经过授权、上下文相同的精确重试可在原期限过后只读取既有结果，不能启动过期操作；缓存不存在时按期限/重放规则拒绝。缓存和队列必须有界，容量不足拒绝新操作，不能淘汰正在执行的请求使旧请求重新有效；淘汰已完成记录后仍须保留必要重放防线。

只有成功 ACQUIRE/RENEW 的授权请求回复可携带 lease；ACCEPTED、失败和普通结果的 lease 为空。公开结果 topic 始终清除 token；异步授权完成后，客户端可对原请求做精确重试，通过受控回复取得已记录结果。旧回复中的 token/到期时间不能覆盖更新状态或恢复过期授权。任何公共状态、能力、日志和告警都不得公布 token。

MotionState 的 sequence/publish_time 是状态发布心跳，不是 IMU/关节采样时间。消费者检查 session、严格递增状态序号、主机时钟及 state_stale_after_ns，过期/未知/离线状态不能用于准入。readiness=READY 是必要平台条件已满足，不代表所有动作当前可用；UNSPECIFIED、未测数据和心跳不能伪装 READY。actuation_state 仅在后端确认后报告 ARMED/DISARMED，未测或失效时报告 UNKNOWN，缺省 UNSPECIFIED 不提供准入依据；动作完成以结果为准，不能从界面图像推断。

last_accepted_command_sequence 表示完整验证并接受的速度包，last_consumed_command_sequence 表示控制循环实际消费的意图，不表示关节目标已通过 HI 或电机已经执行。相关序号只属于当前 owner_generation/source/producer；无持有者时这些字段、accepted_time、lease_expires 均为零，拒绝计数可在 session 内累计。fault_reason 独立记录实际故障原因，不能用 last_rejection_reason 冒充故障；FAULT 必须提供非零 fault_reason，非故障时保持 UNSPECIFIED（急停执行失败等实际后端故障除外）。MotionState 不包含实测机体速度，后续里程计/估计状态使用独立测量契约。

仿真以 simulation、paused、physics_step、simulation_time_ns 单独报告。在线授权和期限仍使用主机时钟；暂停撤销授权、清目标并解除使能，恢复不自动执行旧指令，reset 更换 motion_session。状态发布可以继续，但暂停不刷新物理采样。真实后端的仿真字段保持 false/0。

## 能力、边界和演进

contract_version 必须等于1；基本身份/参考系/修订字段非空，时限为正，command_timeout<=max_command_duration，stop_timeout<=max_lease_duration，state_stale_after_ns 为正。支持 LOCOMOTION 时三个速度轴描述必须存在；不支持 LOCOMOTION 时 velocity_limits 省略。STOP_AND_HOLD 必须有支持的 HOLD 能力。模式、动作标识唯一，动态 availability 为 AVAILABLE 时 reason=UNSPECIFIED；UNAVAILABLE/NOT_SUPPORTED 须有明确非零原因和说明，NOT_SUPPORTED 使用 NOT_SUPPORTED 原因。能力序号/发布时间使动态 availability 可判新鲜，但消费者仍须在请求时接受接收器的最终判断。

所有身份、frame、request/action ID 和 token 为非空可打印 ASCII，禁止 NUL 和控制字符；用于端点路径的 robot_id/motion_id 进一步限制为字母、数字、下划线、点和连字符，禁止注入路径分隔符；message 是诊断文本，可为空且不用于控制分支。Nanopb max_size 包含末尾 NUL：普通标识/token 最多63字节，clock/model/configuration revision 最多127字节，message 最多159字节；可请求模式最多3项，动作描述最多32项。C++/其他语言必须同样检查，禁止截断标识。所有 duration 加法和 sequence/generation 递增检查溢出；超限拒绝或停止并更换 session，不回绕。v1 的完整消息不包含关节数量，因此本身不受 actuator/v2 当前32轴边界影响；人形等实际接入若超过该低层能力，仍需独立扩展和验收 actuator 契约。

未知枚举不能作为默认动作。状态/能力消费者可保留和忽略未知非关键字段，但未知能力不能解释为可用；控制接收器须拒绝未协商的未知字段（包含嵌套意图）及未知枚举，避免老实现忽略新控制约束。新增功能通过明确版本/能力门控实施，已有字段编号与含义不改变；不通过在 v1 偷加某款机器人专有字段扩展技能。

## 端点建议与后续验收

建议命名为 `/robots/<robot_id>/motion/<motion_id>/command`（topic）、`request`（受控 service）、`results`（去除 token 的结果 topic）、`state` 和 `capabilities`（带心跳的 topic）。产品当前采用逻辑 `/motion/base/*`，由 OpenEmber transport 加上 robot_id 前缀，实际 key 与认证加密封装见[实现说明](../../openember-quadruped/docs/motion-link-control.md)。上述 `/robots/<id>` 是建议逻辑形式，不要求在已有机器人路由前缀下重复嵌入；跨机器 QoS、通用鉴权仍待后续落实。公共 topic 上的 source 或 namespace 不等于访问控制。速度采用最新完整意图，不能按积压 FIFO 回放；模式/动作请求和停止需要有界且有明确回执的执行路径。

本仓库验证生成、wire 字段/默认值、oneof 隔离、未知字段/枚举保留、不同形态能力表达和 Nanopb options 完整边界；不将这些测试当作运行时拒绝或真实运动的证据。后续先实现框架/产品适配器及 Mock 测试，再连接当前四足模式机和 MuJoCo：

| 验收组 | 必须验证 |
| --- | --- |
| 形态/能力 | 差速不接收非零 vy；全向/四足/人形只接受声明能力；缺失恢复明确拒绝；无关节顺序依赖 |
| 验证/期限 | 缺少 context/lease/velocity、旧模式代次、未知意图/枚举/字段、非有限值、错 frame/修订/会话、未来/过期/溢出、入队后过期均拒绝且不刷新 watchdog |
| 控制权 | 多来源争用、旧 token、过期续租、令牌隔离、授权/状态分离、获取/ARM/动作分别请求 |
| 请求生命周期 | 精确重试、冲突 ID、已排队请求过期、完成/取消/超时竞争、最终结果与实际后端状态一致 |
| 停止与输入 | 停止优先、失联/租约到期、STOP_AND_HOLD 超时回退、反馈失效、无隐式重新使能 |
| 仿真与迁移 | 暂停/reset 撤权、旧 history/命令失效、真实/仿真隔离；P05W 控制行为显式适配，不把现有隐式 ARM 路径冒充公共契约 |

本次验证：独立配置 `/tmp/openember-msgs-motion-contract`，启用 `OPENEMBER_MSGS_BUILD_CPP=ON` 与 `OPENEMBER_MSGS_BUILD_TESTS=ON`，生成并构建全部 C++ 消息后，motion、alerts、IMU、power、actuator 5项 CTest 全部通过。本轮未执行 Nanopb C 生成/编译、板端内存预算、运行时控制校验、Link 端点或实际机器人验证。
