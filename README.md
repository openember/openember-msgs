# openember-msgs

Language-neutral protocol definitions shared by OpenEmber and emberlite.

This repository contains the stable wire contract for core runtime
communication. OpenEmber can generate C++ protobuf bindings from these files.
emberlite can generate pure C bindings with Nanopb from the same `.proto`
source.

## Design Goals

- Keep one clean protocol line for OpenEmber and emberlite.
- Use Protobuf as the schema and wire format.
- Keep message definitions friendly to Nanopb and constrained devices.
- Let Zenoh keys handle routing, while Protobuf messages handle structure.
- Version packages explicitly with `v1`, `v2`, and so on.

## Packages

```text
proto/openember/msgs/common/v1
proto/openember/msgs/node/v1
proto/openember/msgs/lifecycle/v1
proto/openember/msgs/diagnostics/v1
proto/openember/msgs/parameter/v1
proto/openember/msgs/log/v1
proto/openember/msgs/device/v1
proto/openember/msgs/runtime/v1
```

`common/v1` defines shared types such as `Header`, `Status`, `QosProfile`,
`HealthState`, `KeyValue`, and `Metric`.

`node/v1` defines node discovery and introspection messages:
`NodeInfo`, `NodeHeartbeat`, `NodeQuery`, and `NodeRegistrySnapshot`.

`lifecycle/v1` defines lifecycle state transitions and command responses.

`diagnostics/v1` defines health and diagnostic status reports.

`parameter/v1` defines runtime parameter get/set/event messages.

`log/v1` defines structured log records.

`device/v1` defines LPIO-oriented device inventory and state messages.

`runtime/v1` defines process start/stop/event messages for launch managers.

## Zenoh Key Convention

Zenoh keys should remain stable and human-readable. Suggested core keys:

```text
openember/<robot_id>/<namespace>/nodes/<node>/info
openember/<robot_id>/<namespace>/nodes/<node>/heartbeat
openember/<robot_id>/<namespace>/nodes/<node>/lifecycle
openember/<robot_id>/<namespace>/topics/<topic>
openember/<robot_id>/<namespace>/services/<service>
openember/<robot_id>/<namespace>/diagnostics/<node>
openember/<robot_id>/<namespace>/logs/<node>
openember/<robot_id>/<namespace>/devices/<device>
openember/<robot_id>/<namespace>/runtime/processes/<process>
```

The Zenoh key selects the stream or service. The Protobuf message declares the
payload structure. Avoid a mandatory global envelope unless a future bridge or
recording format truly needs one.

## Building C++ Bindings

The default CMake configuration only exposes the `.proto` files. Enable
generated C++ bindings when needed:

```bash
cmake -S . -B build -DOPENEMBER_MSGS_BUILD_CPP=ON
cmake --build build
```

Consumers can also run `protoc` directly:

```bash
protoc -I proto --cpp_out generated/cpp \
  proto/openember/msgs/common/v1/common.proto
```

## Generating C Bindings With Nanopb

Nanopb options live in:

```text
nanopb/openember_msgs.options
```

Use the same `.proto` files with Nanopb:

```bash
python3 generator/nanopb_generator.py \
  -I proto \
  -f nanopb/openember_msgs.options \
  proto/openember/msgs/common/v1/common.proto
```

The options file caps string lengths and repeated field counts so generated C
types can be allocated statically by emberlite.

## Compatibility Rules

- Do not rename packages or existing fields.
- Do not reuse field numbers.
- Reserve removed field numbers and names in future edits.
- Add fields only with new numbers.
- Put breaking changes in a new package version, for example `node/v2`.
- Avoid `Any`, `map`, unbounded repeated fields, and deep nesting in core
  messages.

## Current Scope

This repository intentionally defines core runtime messages only. Robot-specific
sensor, actuator, vision, navigation, or agent payloads should be added later as
separate packages once the runtime protocol is stable.
