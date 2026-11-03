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
