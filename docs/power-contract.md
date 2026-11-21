# Power v1 validity and receipt contract

BatteryState fields 1–9 and PowerState fields 1–7 are unchanged. New fields are
append-only. No board-specific commands, battery count or switching policy is added.

- Battery measurement validity is explicit per field. Zero is a valid value;
  absent validity flags from a legacy sender mean unknown, not trustworthy zero.
- `connected_known=false` means unknown presence regardless of `connected`.
- `sample_time_monotonic_ns=0` means unknown sample time. Never fill it with host receipt time.
- `receive_time_monotonic_ns` is host receipt of that resource's last successful
  observation. Re-publication does not change it or the sample sequence.
- `clock_domain` identifies the source clock/session. Monotonic values are not
  comparable across hosts or sessions without an explicit mapping.
- `receive_fresh` is the producer's assessment at publication, not a permanent
  guarantee. Consumers also need their own arrival deadline and must not let
  duplicate sample sequences renew measurement age. Header wall time describes
  publication, not a new measurement.
- PMU/BMS gateways can return old downstream measurements in a new response.
  Receipt freshness does not establish downstream sample freshness or control safety.
- Validity flags describe the retained measurement. A stale retained value may
  remain valid numerically while `receive_fresh=false` and availability is degraded.

Applications must interpret field validity, availability, session and freshness
together. Existing readers cannot enforce the added semantics merely by accepting
the wire format; migrate control consumers explicitly. PowerCommand remains a
legacy string envelope and is not authorization to expose raw hardware commands.

Nanopb clock domains are bounded to 128 bytes including the terminator. Existing
PowerState metrics limit remains 16. Tests cover old/new C++ wire readers and
bounded Nanopb battery/power serialization (in the shared Nanopb roundtrip test).

## Typed resource control

PowerResourceCapability and PowerCapabilities describe channels, optional fans/lights and battery IDs without prescribing board layout. PowerResourceState carries observed level with explicit validity; unknown is not off. Channel targets are boolean; fan/light targets are normalized [0,1], with non-variable resources restricted to off/on. Capabilities are session-bound and are not proof of authorization.

PowerControlRequest uses a oneof target: a missing target is invalid, while an explicit false channel target requests off. Receivers reject nonfinite values, unknown operations, unsupported resources and invalid bounds. The existing PowerCommand string envelope is unchanged and must not be interpreted as a raw hardware escape hatch.

Control requests bind power ID, service session, clock domain, request ID and deadline. The deadline is in the advertised service monotonic domain; a remote client's unrelated clock cannot be used directly. Transport/clock mapping for remote hosts is outside this contract. Authentication identity and readiness come from trusted receiver context, never Header fields. Turning on a readiness-gated resource requires a fresh positive readiness decision. Channel state is separate from actuator arming.

Receivers distinguish Rejected, Accepted, Confirmed and Unknown. ACK/echo alone establishes at most Accepted; Confirmed requires actual feedback. Timeout or an exception after a possible write is Unknown, not proof that nothing happened. Unknown and Accepted commands must not be retried as new operations automatically. Device drivers recheck deadlines before sending.

Executed requests reserve their IDs within the authenticated principal and service session. Replays with unchanged content do not repeat device operations; changing content with the same ID is a conflict. Expired original requests cannot execute. The P0 core stores bounded session-wide history without eviction: request_capacity describes its configured limit (default 256, at most 4096), and exhaustion rejects new operations as Busy. Session rotation and long-running orchestration are not yet implemented. Restarting with an empty history requires a new service session; it must not restore previous target intent automatically.

Identifiers use 1–127 ASCII bytes from letters, digits and `_-.:/@`; Nanopb storage is 128 bytes including NUL. There are at most 64 resources and 16 battery IDs per capabilities message. Result text is bounded to 255 bytes, with 256 bytes Nanopb storage. Existing BatteryState and PowerState field numbers and limits are unchanged.

These messages define no default Topic or control RPC endpoint. P0 provides the C++ Mock adapter only; publication metadata, authenticated transport, bounded queues, readiness aggregation and real driver integration remain owner responsibilities.

## 操作权限与关闭协调

类型明确的控制契约版本标记更新为 2。`PowerResourceCapability.disable_requires_ready` 使用新增字段 6 描述正常关闭是否也需要产品协调；现有字段编号和旧 PowerCommand 不变。`enable_requires_ready` 与该字段独立，电机供电可以要求开关两种操作都先获得许可。它们是能力描述，不是当前调用者已经获得权限或整机已经就绪的证明。

服务端按已认证 principal、资源和操作分别授权，具有开启权限不等于具有关闭权限，Web 登录也不等于获得供电控制权。就绪许可由可信整机管理适配器提供并在发送前重新检查，不加入用户可声明的 `system_ready` 字段。重复请求返回历史结果前仍检查当前操作权限；查询结果不会触发新的供电操作。
