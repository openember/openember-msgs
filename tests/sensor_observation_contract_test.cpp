#include "openember/msgs/sensor/v1/sensor.pb.h"
#include "openember/msgs/sensor/v1/camera.pb.h"
#include "openember/msgs/sensor/v1/point_cloud.pb.h"
#include "openember/msgs/sensor/v1/lidar.pb.h"
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

namespace s = openember::msgs::sensor::v1;
void Check(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }

void GnssCompatibility() {
    // Independent old-reader layout, frozen at the original thirteen fields.
    google::protobuf::FileDescriptorProto file;
    file.set_name("legacy_gnss.proto"); file.set_package("legacy_test"); file.set_syntax("proto3");
    file.add_dependency(s::GnssFix::descriptor()->file()->name());
    file.add_dependency(openember::msgs::common::v1::Header::descriptor()->file()->name());
    auto* old = file.add_message_type(); old->set_name("GnssFix");
    const char* names[] = {"header", "sensor_id", "frame_id", "sample_time_monotonic_ns", "sequence",
        "fix_type", "latitude_deg", "longitude_deg", "altitude_m", "velocity_enu_mps",
        "satellites_used", "horizontal_accuracy_m", "vertical_accuracy_m"};
    using F = google::protobuf::FieldDescriptorProto;
    const F::Type types[] = {F::TYPE_MESSAGE, F::TYPE_STRING, F::TYPE_STRING, F::TYPE_UINT64,
        F::TYPE_UINT64, F::TYPE_ENUM, F::TYPE_DOUBLE, F::TYPE_DOUBLE, F::TYPE_DOUBLE,
        F::TYPE_MESSAGE, F::TYPE_UINT32, F::TYPE_DOUBLE, F::TYPE_DOUBLE};
    for (int i = 0; i < 13; ++i) {
        const auto* current = s::GnssFix::descriptor()->FindFieldByNumber(i + 1);
        Check(current && current->name() == names[i] && int(current->type()) == types[i],
              "legacy GNSS layout changed");
        auto* field = old->add_field(); field->set_number(i + 1); field->set_name(names[i]);
        field->set_type(types[i]); field->set_label(F::LABEL_OPTIONAL);
        if (i == 0) field->set_type_name(".openember.msgs.common.v1.Header");
        if (i == 5) field->set_type_name(".openember.msgs.sensor.v1.GnssFixType");
        if (i == 9) field->set_type_name(".openember.msgs.sensor.v1.Vector3");
    }
    Check(s::GNSS_FIX_TYPE_UNSPECIFIED == 0 && s::GNSS_FIX_TYPE_NO_FIX == 1 &&
          s::GNSS_FIX_TYPE_FIX_2D == 2 && s::GNSS_FIX_TYPE_FIX_3D == 3 &&
          s::GNSS_FIX_TYPE_RTK_FLOAT == 4 && s::GNSS_FIX_TYPE_RTK_FIXED == 5, "fix enum changed");
    google::protobuf::DescriptorPool pool(google::protobuf::DescriptorPool::generated_pool());
    const auto* built = pool.BuildFile(file);
    Check(built != nullptr, "legacy GNSS descriptor failed");
    google::protobuf::DynamicMessageFactory factory(&pool);
    std::unique_ptr<google::protobuf::Message> legacy(
        factory.GetPrototype(built->message_type(0))->New());
    const auto* reflection = legacy->GetReflection();
    reflection->SetString(legacy.get(), legacy->GetDescriptor()->FindFieldByNumber(2), "antenna");
    reflection->SetEnumValue(legacy.get(), legacy->GetDescriptor()->FindFieldByNumber(6), 3);
    reflection->SetDouble(legacy.get(), legacy->GetDescriptor()->FindFieldByNumber(7), 31.2);
    s::GnssFix extended;
    Check(extended.ParseFromString(legacy->SerializeAsString()), "old GNSS could not be read");
    Check(extended.sensor_id() == "antenna" && extended.latitude_deg() == 31.2 &&
          !extended.position_valid() && !extended.altitude_valid() && !extended.velocity_valid() &&
          !extended.satellites_used_valid() && !extended.horizontal_accuracy_valid() &&
          !extended.vertical_accuracy_valid() && extended.clock_domain().empty() &&
          extended.altitude_reference() == s::ALTITUDE_REFERENCE_UNSPECIFIED &&
          extended.position_covariance_m2_size() == 0, "legacy GNSS silently gained validity");
    extended.set_position_valid(true); extended.set_altitude_valid(true);
    extended.set_altitude_reference(s::ALTITUDE_REFERENCE_WGS84_ELLIPSOID);
    extended.set_clock_domain("host-boot-session"); extended.set_receive_time_monotonic_ns(2000);
    extended.set_sample_clock_domain("receiver-session"); extended.set_sample_time_monotonic_ns(1000);
    extended.set_position_covariance_type(s::COVARIANCE_TYPE_DIAGONAL);
    for (int i = 0; i < 9; ++i) extended.add_position_covariance_m2(i % 4 == 0 ? 0.01 : 0);
    Check(legacy->ParseFromString(extended.SerializeAsString()), "old reader rejected extensions");
    Check(reflection->GetDouble(*legacy, legacy->GetDescriptor()->FindFieldByNumber(7)) == 31.2,
          "old reader lost latitude");
    s::GnssFix restored;
    Check(restored.ParseFromString(legacy->SerializeAsString()) && restored.position_valid() &&
          restored.altitude_reference() == s::ALTITUDE_REFERENCE_WGS84_ELLIPSOID &&
          restored.receive_time_monotonic_ns() == 2000 && restored.sample_clock_domain() == "receiver-session" &&
          restored.position_covariance_m2_size() == 9, "unknown-field roundtrip lost GNSS extension");
}

void ObservationRoundtrip() {
    s::Image image;
    Check(!image.data_valid() && !s::CameraInfo().calibration_valid() &&
          !s::PointCloud().data_valid() && !s::LaserScan().data_valid(), "empty observation is valid");
    auto* meta = image.mutable_metadata();
    meta->set_sensor_id("rgbd_front"); meta->set_stream_id("depth");
    meta->set_frame_id("front_depth_optical"); meta->set_sequence(42);
    meta->set_sample_time_monotonic_ns(123); meta->set_sample_clock_domain("device-session");
    meta->set_receive_time_monotonic_ns(456); meta->set_clock_domain("host-session");
    meta->mutable_header()->set_timestamp_unix_ns(789);
    image.set_width(2); image.set_height(2); image.set_step(6); // 2 padding bytes per row.
    image.set_encoding("16UC1"); image.set_content(s::IMAGE_CONTENT_DEPTH);
    image.set_depth_unit_m(.001); image.set_is_bigendian(true);
    image.set_data(std::string("\0\1\0\2\xff\xff\0\3\0\4\xff\xff", 12));
    image.set_data_valid(true); image.set_calibration_revision("cal-1");
    s::Image decoded;
    Check(decoded.ParseFromString(image.SerializeAsString()) && decoded.data() == image.data() &&
          decoded.is_bigendian() && decoded.step() == 6 && decoded.depth_unit_m() == .001 &&
          decoded.calibration_revision() == "cal-1", "image endian/padding/binary payload changed");
    Check(decoded.metadata().sample_time_monotonic_ns() == 123 &&
          decoded.metadata().receive_time_monotonic_ns() == 456 &&
          decoded.metadata().header().timestamp_unix_ns() == 789 &&
          decoded.metadata().sample_clock_domain() != decoded.metadata().clock_domain(),
          "acquisition, receive and publication clocks conflated");
    s::PointCloud cloud;
    *cloud.mutable_metadata() = *meta; cloud.mutable_metadata()->set_stream_id("points");
    cloud.set_height(1); cloud.set_width(1); cloud.set_point_step(12); cloud.set_row_step(16);
    for (int i = 0; i < 3; ++i) {
        auto* field = cloud.add_fields(); field->set_name(std::string(1, "xyz"[i]));
        field->set_offset(i * 4); field->set_datatype(s::POINT_FIELD_TYPE_FLOAT32); field->set_count(1);
    }
    cloud.set_data(std::string(16, '\0')); cloud.set_data_valid(true); cloud.set_is_dense(false);
    s::PointCloud points;
    Check(points.ParseFromString(cloud.SerializeAsString()) && points.data().size() == 16 &&
          points.point_step() == 12 && points.row_step() == 16 && points.data_valid() && !points.is_dense(),
          "point cloud padding or density semantics lost");
    s::LaserScan scan;
    *scan.mutable_metadata() = *meta; scan.mutable_metadata()->set_stream_id("scan");
    scan.set_angle_min_rad(-.1); scan.set_angle_max_rad(.1); scan.set_angle_increment_rad(.1);
    scan.set_scan_time_s(.1); scan.set_time_increment_s(.001);
    scan.set_range_min_m(.05); scan.set_range_max_m(10);
    scan.add_ranges_m(std::numeric_limits<float>::quiet_NaN());
    scan.add_ranges_m(std::numeric_limits<float>::infinity()); scan.add_ranges_m(1.5f);
    scan.set_data_valid(true);
    s::LaserScan rays;
    Check(rays.ParseFromString(scan.SerializeAsString()) && rays.ranges_m_size() == 3 &&
          std::isnan(rays.ranges_m(0)) && std::isinf(rays.ranges_m(1)) && rays.ranges_m(2) == 1.5f &&
          rays.intensities_size() == 0, "scan missing-return values or optional intensity changed");
}

int main() {
    try { GnssCompatibility(); ObservationRoundtrip(); return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
