#include "openember/msgs/sensor/v1/sensor.pb.h"
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <iostream>
#include <memory>
#include <stdexcept>

void Check(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
int main() {
    namespace sensor = openember::msgs::sensor::v1;
    try {
        // Freeze the old reader to fields 1..9, using an independent descriptor
        // and wire parser. No extension field is visible through this reader.
        google::protobuf::FileDescriptorProto file;
        file.set_name("legacy_imu_test.proto"); file.set_package("legacy_test"); file.set_syntax("proto3");
        file.add_dependency(sensor::ImuSample::descriptor()->file()->name());
        file.add_dependency(openember::msgs::common::v1::Header::descriptor()->file()->name());
        auto* old_type = file.add_message_type(); old_type->set_name("ImuSample");
        const char* names[] = {"header", "sensor_id", "frame_id", "sample_time_monotonic_ns", "sequence",
            "acceleration_mps2", "angular_velocity_radps", "magnetic_field_tesla", "temperature_celsius"};
        using Field = google::protobuf::FieldDescriptorProto;
        const Field::Type types[] = {Field::TYPE_MESSAGE, Field::TYPE_STRING, Field::TYPE_STRING,
            Field::TYPE_UINT64, Field::TYPE_UINT64, Field::TYPE_MESSAGE, Field::TYPE_MESSAGE,
            Field::TYPE_MESSAGE, Field::TYPE_DOUBLE};
        for (int i = 0; i < 9; ++i) {
            const auto* current = sensor::ImuSample::descriptor()->FindFieldByNumber(i + 1);
            Check(current && current->name() == names[i] && static_cast<int>(current->type()) == types[i],
                  "legacy field number/name/type changed");
            auto* field = old_type->add_field(); field->set_number(i + 1); field->set_name(names[i]);
            field->set_type(types[i]); field->set_label(Field::LABEL_OPTIONAL);
            if (i == 0) field->set_type_name(".openember.msgs.common.v1.Header");
            if (i >= 5 && i <= 7) field->set_type_name(".openember.msgs.sensor.v1.Vector3");
        }
        google::protobuf::DescriptorPool pool(google::protobuf::DescriptorPool::generated_pool());
        const auto* legacy_file = pool.BuildFile(file);
        Check(legacy_file != nullptr, "legacy descriptor failed");
        google::protobuf::DynamicMessageFactory factory(&pool);
        std::unique_ptr<google::protobuf::Message> legacy(factory.GetPrototype(legacy_file->message_type(0))->New());
        const auto* reflection = legacy->GetReflection();
        reflection->SetString(legacy.get(), legacy->GetDescriptor()->FindFieldByNumber(2), "old-imu");
        reflection->SetUInt64(legacy.get(), legacy->GetDescriptor()->FindFieldByNumber(5), 42);
        sensor::ImuSample current;
        Check(current.ParseFromString(legacy->SerializeAsString()), "new reader rejected legacy wire");
        Check(current.sensor_id() == "old-imu" && current.sequence() == 42 && !current.orientation_valid() &&
            !current.has_orientation() && current.clock_domain().empty() && current.receive_time_monotonic_ns() == 0 &&
            !current.acceleration_valid() && !current.angular_velocity_valid() && !current.magnetic_field_valid() &&
            !current.temperature_valid(), "legacy data silently gained validity");
        current.mutable_orientation()->set_w(.5); current.mutable_orientation()->set_x(.5);
        current.mutable_orientation()->set_y(.5); current.mutable_orientation()->set_z(.5);
        current.set_orientation_valid(true); current.set_reference_frame_id("world");
        current.set_clock_domain("boot-1"); current.set_mounting_revision("mount-1");
        current.set_receive_time_monotonic_ns(1000); current.set_acceleration_valid(true);
        current.set_angular_velocity_valid(true); current.set_temperature_valid(true);
        Check(legacy->ParseFromString(current.SerializeAsString()), "legacy reader rejected new wire");
        Check(reflection->GetString(*legacy, legacy->GetDescriptor()->FindFieldByNumber(2)) == "old-imu",
              "legacy common fields lost");
        sensor::ImuSample roundtrip;
        Check(roundtrip.ParseFromString(legacy->SerializeAsString()) && roundtrip.orientation_valid() &&
            roundtrip.orientation().x() == .5 && roundtrip.clock_domain() == "boot-1" &&
            roundtrip.receive_time_monotonic_ns() == 1000 && roundtrip.acceleration_valid() &&
            !roundtrip.magnetic_field_valid(), "C++ unknown-field roundtrip lost extension");
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
