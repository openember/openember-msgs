#include "openember/msgs/sensor/v1/sensor.pb.h"
#include <pb_encode.h>
#include <pb_decode.h>
#include <stdio.h>
#include <string.h>

int main(void) {
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
    puts("bounded Nanopb IMU roundtrip passed");
    return 0;
}
