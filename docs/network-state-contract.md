# Network state v1

核心 `network_manager` 在机器人命名空间发布 `/network/state`，类型为 `openember.msgs.network.v1.NetworkStateSnapshot`。这是单一主机、单一网络命名空间的只读完整列表；不发布设备密码或 NetworkManager 连接机密，状态本身不携带写请求；可选本地管理 RPC 使用独立请求/结果。

- 消费者检查机器人标识、`source_node=network_manager`、非空主机/实例、递增序号及采样单调时间，用本地接收时间和 `stale_after_ms` 判断过期。跨主机单调时间不能直接相减。
- `online` 表示服务在线；`interfaces_valid` / `addresses_valid` 表示这次查询是否成功。查询失败不等于没有接口。只有有效完整快照才能用空列表删除旧接口。
- 每个接口索引、名称唯一，最多 64 项；每接口最多 16 个 IPv4/IPv6 地址。字符串/数组边界与 nanopb options 一致。线上消息最多 64 KiB，服务将 protobuf 快照限制在 60 KiB。
- 同一主机的新实例替换旧实例后拒绝迟到旧实例；实例退役集合有界，达到上限需要重启消费者。身份选择不会自动在不同主机间漂移；多计算机部署应显式配置 `host_id`。
- 管理 UP、运行状态、载波、地址各自独立；不以任一字段断言互联网可达性。IPv6 tentative / deprecated / duplicate_address 需保留，不宣称这些地址可以通信。
- 流量为内核累计 64 位计数，接口重建会重置。`statistics.valid=false` 表示不可用，不能解释为 0。
- `backend=linux-rtnetlink`。可选 NetworkManager 元数据通过 `manager_available`、`manager_fresh` 和独立采样时间表达，不能用内核快照更新掩盖旧连接数据。元数据不可用不影响内核观测。
- `configuration_supported=true` 表示配置后端已启用且元数据新鲜；并非系统权限保证。保存连接、SSID、角色、自动连接及 IP 设置均为非机密元数据。GetSecrets 的内容不进入快照。

## 本地管理 RPC（contract 2）

`/network/rpc` 接受 `NetworkRequest`，返回 `NetworkReply`；没有写操作的广播 topic。状态查询、队列提交和结果查询立即返回；耗时操作由服务工作线程执行。queued/running 为执行中，awaiting_confirmation 等待保留确认，complete/failed/rolled_back/unknown/not_found 分别表达完成、失败、恢复、无法确认和未知请求。

请求使用最多 63 字节 request_id、私有本地进程 token、host_id/service_instance 和同机单调时钟执行期限。相同 ID 不重复执行；换参数必须换 ID。进程重启或丢失结果不能自动重发写操作，先查询当前状态。结果最多保留五分钟，不是跨进程事务日志。

支持 ethernet、scan、connect、enable、disconnect、hotspot、stop_hotspot、autoconnect、confirm，以及 status/result 查询。save 区分临时内存更新与保存连接配置。Wi-Fi Personal 密码只存在请求与后端调用中，禁止日志/广播/结果回显。确认针对原 request_id；网页重新登录同一账号可继续确认。

管理链路使用 NM checkpoint 与核心 ConfirmedChange 协作恢复；NetworkManager 创建 checkpoint 失败时不更改该接口。具体恢复由后端负责，不在消息层引入控制权租约或通用事务框架。
