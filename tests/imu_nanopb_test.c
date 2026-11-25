#include "openember/msgs/sensor/v1/sensor.pb.h"
#include "openember/msgs/sensor/v1/camera.pb.h"
#include "openember/msgs/sensor/v1/lidar.pb.h"
#include "openember/msgs/sensor/v1/point_cloud.pb.h"
#include "openember/msgs/power/v1/power.pb.h"
#include <pb_encode.h>
#include <pb_decode.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    /* These payloads must stay callbacks, not multi-megabyte static MCU buffers. */
    _Static_assert(sizeof(((openember_msgs_sensor_v1_Image*)0)->data) == sizeof(pb_callback_t), "image payload callback");
    _Static_assert(sizeof(((openember_msgs_sensor_v1_PointCloud*)0)->data) == sizeof(pb_callback_t), "cloud payload callback");
    _Static_assert(sizeof(((openember_msgs_sensor_v1_LaserScan*)0)->ranges_m) == sizeof(pb_callback_t), "scan payload callback");
    /* Explicit false in oneof remains a real command, distinct from absence. */
    openember_msgs_power_v1_PowerControlRequest command = openember_msgs_power_v1_PowerControlRequest_init_zero;
    openember_msgs_power_v1_PowerControlRequest command_copy = openember_msgs_power_v1_PowerControlRequest_init_zero;
    pb_byte_t command_bytes[1024];
    if (command.which_target != 0) return 20;
    strcpy(command.power_id, "pmu"); strcpy(command.resource_id, "motor");
    strcpy(command.service_session, "session"); strcpy(command.clock_domain, "host");
    strcpy(command.request_id, "id"); command.deadline_monotonic_ns = 123;
    command.which_target = openember_msgs_power_v1_PowerControlRequest_channel_enabled_tag;
    command.target.channel_enabled = false;
    pb_ostream_t command_output = pb_ostream_from_buffer(command_bytes, sizeof(command_bytes));
    if (!pb_encode(&command_output, openember_msgs_power_v1_PowerControlRequest_fields, &command)) return 21;
    pb_istream_t command_input = pb_istream_from_buffer(command_bytes, command_output.bytes_written);
    if (!pb_decode(&command_input, openember_msgs_power_v1_PowerControlRequest_fields, &command_copy)) return 22;
    if (command_copy.which_target != openember_msgs_power_v1_PowerControlRequest_channel_enabled_tag ||
        command_copy.target.channel_enabled || command_copy.deadline_monotonic_ns != 123 ||
        strcmp(command_copy.resource_id, "motor")) return 23;
    openember_msgs_sensor_v1_ImuSample sample = openember_msgs_sensor_v1_ImuSample_init_zero;
    openember_msgs_sensor_v1_ImuSample decoded = openember_msgs_sensor_v1_ImuSample_init_zero;
    if (sample.orientation_valid || sample.acceleration_valid || sample.angular_velocity_valid) return 1;
    if (sizeof(sample.clock_domain) != 128 || sizeof(sample.reference_frame_id) != 64 ||
        sizeof(sample.mounting_revision) != 64) return 2;
    strcpy(sample.sensor_id, "imu"); strcpy(sample.frame_id, "sensor");
    strcpy(sample.reference_frame_id, "reference"); strcpy(sample.mounting_revision, "mount");
    memset(sample.clock_domain, 's', sizeof(sample.clock_domain) - 1);
    sample.has_orientation = true;
    sample.orientation.w = .5; sample.orientation.x = .5; sample.orientation.y = .5; sample.orientation.z = .5;
    sample.orientation_valid = true; sample.sequence = 42;
    sample.sample_time_monotonic_ns = 17; sample.receive_time_monotonic_ns = 1000;
    sample.has_acceleration_mps2 = true; sample.acceleration_mps2.z = 9.80665; sample.acceleration_valid = true;
    sample.has_angular_velocity_radps = true; sample.angular_velocity_valid = true;
    sample.temperature_celsius = 30; sample.temperature_valid = true;
    pb_byte_t bytes[1024];
    pb_ostream_t stream = pb_ostream_from_buffer(bytes, sizeof(bytes));
    if (!pb_encode(&stream, openember_msgs_sensor_v1_ImuSample_fields, &sample)) return 3;
    pb_istream_t input = pb_istream_from_buffer(bytes, stream.bytes_written);
    if (!pb_decode(&input, openember_msgs_sensor_v1_ImuSample_fields, &decoded)) return 4;
    if (!decoded.orientation_valid || !decoded.has_orientation || decoded.orientation.x != .5 ||
        !decoded.acceleration_valid || !decoded.angular_velocity_valid || decoded.magnetic_field_valid ||
        !decoded.temperature_valid || decoded.sequence != 42 || decoded.sample_time_monotonic_ns != 17 ||
        decoded.receive_time_monotonic_ns != 1000 || strcmp(decoded.clock_domain, sample.clock_domain) != 0) return 5;
    openember_msgs_power_v1_BatteryState battery = openember_msgs_power_v1_BatteryState_init_zero;
    openember_msgs_power_v1_BatteryState result = openember_msgs_power_v1_BatteryState_init_zero;
    if (battery.voltage_valid || battery.connected_known || battery.receive_fresh || sizeof(battery.clock_domain) != 128) return 6;
    battery.current_a = -.5; battery.current_valid = true; battery.voltage_valid = true;
    battery.receive_time_monotonic_ns = 42; battery.receive_fresh = true;
    strcpy(battery.clock_domain, "power-session");
    stream = pb_ostream_from_buffer(bytes, sizeof(bytes));
    if (!pb_encode(&stream, openember_msgs_power_v1_BatteryState_fields, &battery)) return 7;
    input = pb_istream_from_buffer(bytes, stream.bytes_written);
    if (!pb_decode(&input, openember_msgs_power_v1_BatteryState_fields, &result)) return 8;
    if (!result.current_valid || result.current_a != -.5 || !result.voltage_valid || result.voltage_v != 0 ||
        result.connected_known || !result.receive_fresh || result.receive_time_monotonic_ns != 42 ||
        strcmp(result.clock_domain, "power-session")) return 9;
    openember_msgs_power_v1_PowerState power = openember_msgs_power_v1_PowerState_init_zero;
    openember_msgs_power_v1_PowerState power_result = openember_msgs_power_v1_PowerState_init_zero;
    if (sizeof(power.clock_domain) != 128 || power.receive_fresh) return 10;
    strcpy(power.clock_domain, "power-session"); power.receive_time_monotonic_ns = 43; power.receive_fresh = true;
    stream = pb_ostream_from_buffer(bytes, sizeof(bytes));
    if (!pb_encode(&stream, openember_msgs_power_v1_PowerState_fields, &power)) return 11;
    input = pb_istream_from_buffer(bytes, stream.bytes_written);
    if (!pb_decode(&input, openember_msgs_power_v1_PowerState_fields, &power_result)) return 12;
    if (!power_result.receive_fresh || power_result.receive_time_monotonic_ns != 43 || strcmp(power_result.clock_domain, "power-session")) return 13;
    openember_msgs_sensor_v1_GnssFix fix = openember_msgs_sensor_v1_GnssFix_init_zero;
    openember_msgs_sensor_v1_GnssFix fix_copy = openember_msgs_sensor_v1_GnssFix_init_zero;
    if (fix.position_valid || fix.altitude_valid || fix.velocity_valid ||
        sizeof(fix.clock_domain) != 128 || sizeof(fix.sample_clock_domain) != 128 ||
        sizeof(fix.position_covariance_m2) / sizeof(fix.position_covariance_m2[0]) != 9) return 24;
    fix.fix_type = openember_msgs_sensor_v1_GnssFixType_GNSS_FIX_TYPE_FIX_3D;
    fix.position_valid = true; /* (0,0) is a legal position when explicitly valid. */
    fix.altitude_reference = openember_msgs_sensor_v1_AltitudeReference_ALTITUDE_REFERENCE_WGS84_ELLIPSOID;
    fix.position_covariance_type = openember_msgs_sensor_v1_CovarianceType_COVARIANCE_TYPE_DIAGONAL;
    fix.position_covariance_m2_count = 9;
    fix.position_covariance_m2[0] = .01; fix.position_covariance_m2[4] = .01; fix.position_covariance_m2[8] = .04;
    strcpy(fix.clock_domain, "gnss-host-session"); fix.receive_time_monotonic_ns = 42;
    stream = pb_ostream_from_buffer(bytes, sizeof(bytes));
    if (!pb_encode(&stream, openember_msgs_sensor_v1_GnssFix_fields, &fix)) return 25;
    input = pb_istream_from_buffer(bytes, stream.bytes_written);
    if (!pb_decode(&input, openember_msgs_sensor_v1_GnssFix_fields, &fix_copy)) return 26;
    if (!fix_copy.position_valid || fix_copy.altitude_valid || fix_copy.latitude_deg != 0 ||
        fix_copy.position_covariance_m2_count != 9 || fix_copy.position_covariance_m2[8] != .04 ||
        strcmp(fix_copy.clock_domain, fix.clock_domain) || fix_copy.receive_time_monotonic_ns != 42) return 27;
    puts("bounded Nanopb sensor and power roundtrip passed");
    return 0;
}
