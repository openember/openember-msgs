#include "openember/msgs/power/v1/power.pb.h"
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <memory>
#include <stdexcept>
#include <iostream>
void Check(bool ok) { if (!ok) throw std::runtime_error("power wire contract failed"); }
int main() {
    namespace p = openember::msgs::power::v1;
    try {
        for (const auto* descriptor : {p::BatteryState::descriptor(), p::PowerState::descriptor()}) {
            const bool battery = descriptor == p::BatteryState::descriptor();
            const int old_fields = battery ? 9 : 7;
            const char* battery_names[] = {"header","battery_id","sample_time_monotonic_ns","sequence",
                "voltage_v","current_a","temperature_celsius","state_of_charge_percent","availability"};
            const char* power_names[] = {"header","power_id","sample_time_monotonic_ns","sequence","availability","message","metrics"};
            using F = google::protobuf::FieldDescriptor;
            const F::Type battery_types[] = {F::TYPE_MESSAGE,F::TYPE_STRING,F::TYPE_UINT64,F::TYPE_UINT64,
                F::TYPE_DOUBLE,F::TYPE_DOUBLE,F::TYPE_DOUBLE,F::TYPE_DOUBLE,F::TYPE_ENUM};
            const F::Type power_types[] = {F::TYPE_MESSAGE,F::TYPE_STRING,F::TYPE_UINT64,F::TYPE_UINT64,F::TYPE_ENUM,F::TYPE_STRING,F::TYPE_MESSAGE};
            for (int i=0;i<old_fields;++i) {
                const auto* field=descriptor->FindFieldByNumber(i+1);
                Check(field && field->name()==(battery?battery_names[i]:power_names[i]) &&
                    field->type()==(battery?battery_types[i]:power_types[i]) &&
                    field->is_repeated()==(!battery && i==6));
            }
            google::protobuf::FileDescriptorProto file;
            file.set_name("legacy_power.proto"); file.set_package("legacy_power"); file.set_syntax("proto3");
            file.add_dependency(descriptor->file()->name());
            file.add_dependency(openember::msgs::common::v1::Header::descriptor()->file()->name());
            auto* message = file.add_message_type(); message->set_name("Legacy");
            for (int i = 1; i <= old_fields; ++i) descriptor->FindFieldByNumber(i)->CopyTo(message->add_field());
            google::protobuf::DescriptorPool pool(google::protobuf::DescriptorPool::generated_pool());
            const auto* built = pool.BuildFile(file); Check(built != nullptr);
            google::protobuf::DynamicMessageFactory factory(&pool);
            std::unique_ptr<google::protobuf::Message> old(factory.GetPrototype(built->message_type(0))->New());
            old->GetReflection()->SetString(old.get(), old->GetDescriptor()->FindFieldByNumber(2), "old");
            if (battery) {
                p::BatteryState current; Check(current.ParseFromString(old->SerializeAsString()));
                Check(!current.voltage_valid() && !current.current_valid() && !current.temperature_valid() &&
                    !current.state_of_charge_valid() && !current.connected_known() && !current.receive_fresh());
                current.set_voltage_valid(true); current.set_voltage_v(0); current.set_current_a(-.5);
                current.set_current_valid(true); current.set_clock_domain("session");
                current.set_receive_time_monotonic_ns(42); current.set_receive_fresh(true);
                Check(old->ParseFromString(current.SerializeAsString()));
                p::BatteryState again; Check(again.ParseFromString(old->SerializeAsString()));
                Check(again.battery_id() == "old" && again.voltage_valid() && again.voltage_v() == 0 &&
                    again.current_a() == -.5 && again.clock_domain() == "session" && again.receive_fresh());
            } else {
                p::PowerState current; Check(current.ParseFromString(old->SerializeAsString()));
                Check(!current.receive_fresh() && current.receive_time_monotonic_ns() == 0);
                current.set_clock_domain("session"); current.set_receive_time_monotonic_ns(42);
                Check(old->ParseFromString(current.SerializeAsString()));
                p::PowerState again; Check(again.ParseFromString(old->SerializeAsString()));
                Check(again.power_id() == "old" && again.clock_domain() == "session" && again.receive_time_monotonic_ns() == 42);
            }
        }
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
