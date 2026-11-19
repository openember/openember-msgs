#include "openember/msgs/alerts/v1/alerts.pb.h"
#include <google/protobuf/unknown_field_set.h>
#include <stdexcept>
#include <iostream>
namespace w = openember::msgs::alerts::v1;
void Check(bool x) { if (!x) throw std::runtime_error("alerts wire contract"); }
int main() {
    try {
        Check(w::SEVERITY_UNSPECIFIED == 0 && w::TRANSITION_UNSPECIFIED == 0);
        Check(w::TRANSITION_RAISED == 1 && w::TRANSITION_REMINDER == 2 && w::TRANSITION_CLEARED == 3 && w::TRANSITION_UNKNOWN == 4);
        w::AlertEvent event; event.set_publisher_instance("instance-1"); event.set_revision(3);
        event.mutable_header()->set_timestamp_unix_ns(9000000000);
        auto* o = event.mutable_observation();
        o->mutable_key()->set_session("control-1"); o->set_sequence(100);
        o->set_sample_time_ns(10); o->set_clock_domain("simulation");
        o->set_transition(w::TRANSITION_UNKNOWN); o->set_valid(false);
        o->set_value(0); o->set_unit("rad"); o->set_reference_frame("world");
        event.GetReflection()->MutableUnknownFields(&event)->AddVarint(100, 42);
        w::AlertEvent decoded; Check(decoded.ParseFromString(event.SerializeAsString()));
        Check(decoded.revision() == 3 && decoded.observation().sequence() == 100 &&
            decoded.observation().sample_time_ns() == 10 && decoded.header().timestamp_unix_ns() == 9000000000 &&
            decoded.observation().transition() == w::TRANSITION_UNKNOWN && !decoded.observation().valid() &&
            decoded.GetReflection()->GetUnknownFields(decoded).field_count() == 1);
        w::AlertState state; state.set_online(true); state.set_revision(4); state.set_stale_after_ms(3000);
        w::AlertState again; Check(again.ParseFromString(state.SerializeAsString()));
        Check(again.online() && again.revision() == 4 && again.observations_size() == 0);
        Check(!w::AlertState().online());
        Check(w::AlertEvent::descriptor()->FindFieldByName("observation")->number() == 4);
        Check(w::AlertState::descriptor()->FindFieldByName("observations")->is_repeated());
        std::cout << "alerts wire contract OK\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
