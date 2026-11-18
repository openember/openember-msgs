#include "openember/msgs/actuator/v1/actuator.pb.h"
#include "openember/msgs/actuator/v2/actuator.pb.h"

#include <google/protobuf/descriptor.h>
#include <iostream>
#include <fstream>
#include <regex>
#include <set>
#include <stdexcept>

namespace a = openember::msgs::actuator::v2;
using Field = google::protobuf::FieldDescriptor;

void Check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

void CheckField(const google::protobuf::Descriptor* descriptor, int number,
                const char* name, Field::Type type, bool repeated = false) {
    const auto* field = descriptor->FindFieldByNumber(number);
    Check(field && field->name() == name && field->type() == type &&
          field->is_repeated() == repeated, "wire field changed");
}

int main(int argc, char** argv) {
    try {
        Check(argc == 2, "Nanopb options path required");
        std::ifstream options(argv[1]);
        Check(options.good(), "cannot open Nanopb options");
        std::set<std::string> sizes, counts;
        const std::regex rule(R"(^([^ #]+) (max_size|max_count):([1-9][0-9]*)$)");
        for (std::string line; std::getline(options, line);) {
            std::smatch match;
            if (std::regex_match(line, match, rule)) {
                (match[2] == "max_size" ? sizes : counts).insert(match[1]);
            }
        }
        // Every variable-size field must have an explicit static-memory bound.
        const auto* file = a::JointCommand::descriptor()->file();
        for (int i = 0; i < file->message_type_count(); ++i) {
            const auto* message = file->message_type(i);
            for (int j = 0; j < message->field_count(); ++j) {
                const auto* field = message->field(j);
                if (field->type() == Field::TYPE_STRING)
                    Check(sizes.count(field->full_name()) == 1, "missing string bound");
                if (field->is_repeated())
                    Check(counts.count(field->full_name()) == 1, "missing repeated bound");
            }
        }
        // Protect the distinct wire namespace and absence of implicit management.
        Check(a::JointCommand::descriptor()->full_name() ==
              "openember.msgs.actuator.v2.JointCommand", "wrong wire namespace");
        Check(a::JointCommand::descriptor()->field_count() == 3,
              "target message must remain separate from management");
        CheckField(a::JointCommand::descriptor(), 1, "context", Field::TYPE_MESSAGE);
        CheckField(a::JointCommand::descriptor(), 2, "lease", Field::TYPE_MESSAGE);
        CheckField(a::JointCommand::descriptor(), 3, "joints", Field::TYPE_MESSAGE, true);
        CheckField(a::CommandContext::descriptor(), 3, "controller_session", Field::TYPE_STRING);
        CheckField(a::CommandContext::descriptor(), 7, "clock_domain", Field::TYPE_STRING);
        CheckField(a::CommandContext::descriptor(), 9, "expires_time_ns", Field::TYPE_UINT64);
        CheckField(a::ControllerFeedback::descriptor(), 11,
                   "last_accepted_command_sequence", Field::TYPE_UINT64);
        CheckField(a::ControllerFeedback::descriptor(), 12,
                   "last_applied_command_sequence", Field::TYPE_UINT64);
        CheckField(a::ControllerFeedback::descriptor(), 22, "backend_failure", Field::TYPE_BOOL);
        CheckField(a::ControllerFeedback::descriptor(), 23, "fault_reason", Field::TYPE_ENUM);
        CheckField(a::ControllerFeedback::descriptor(), 24, "lease_expires_time_ns", Field::TYPE_UINT64);

        // Unset fields cannot accidentally announce authority or valid samples.
        a::ControllerFeedback feedback;
        Check(feedback.state() == a::CONTROLLER_STATE_UNSPECIFIED &&
              feedback.owner_generation() == 0 && !feedback.backend_failure() &&
              feedback.fault_reason() == a::REJECTION_REASON_UNSPECIFIED &&
              feedback.lease_expires_time_ns() == 0, "unsafe feedback default");
        a::ControlResult result;
        Check(result.code() == a::CONTROL_RESULT_CODE_UNSPECIFIED && !result.has_lease(),
              "unset result must not imply acceptance");
        a::ControlRequest request;
        Check(request.operation() == a::CONTROL_OPERATION_UNSPECIFIED,
              "unset operation must not arm");
        a::JointFeedback sample;
        Check(!sample.position_valid() && !sample.velocity_valid() && !sample.effort_valid() &&
              !sample.temperature_valid() && !sample.voltage_valid() && !sample.fault_valid() &&
              !sample.receive_fresh() && sample.sample_sequence() == 0,
              "unset sample must remain unknown");

        // Identity, authority and deadlines survive a complete topology round trip.
        a::JointCommand command;
        auto* context = command.mutable_context();
        context->set_robot_id("sim-robot"); context->set_controller_id("joints");
        context->set_controller_session("reset-epoch-2");
        context->set_model_revision("sha256:fixture");
        context->set_producer_session("producer-3"); context->set_sequence(42);
        context->set_clock_domain("host-monotonic:boot-fixture");
        context->set_issued_time_ns(100); context->set_expires_time_ns(200);
        command.mutable_lease()->set_generation(7);
        command.mutable_lease()->set_token("private-fixture");
        for (int i = 0; i < 32; ++i) {
            auto* joint = command.add_joints();
            joint->set_joint_name("joint_" + std::to_string(i));
            joint->set_mode(a::JOINT_CONTROL_MODE_MIT);
            joint->set_velocity_radps(-0.5); joint->set_stiffness_nm_per_rad(20);
            joint->set_damping_nms_per_rad(1); joint->set_max_effort_nm(10);
        }
        a::JointCommand decoded;
        Check(decoded.ParseFromString(command.SerializeAsString()), "target decode failed");
        Check(decoded.context().SerializeAsString() == context->SerializeAsString() &&
              decoded.lease().generation() == 7 && decoded.lease().token() == "private-fixture" &&
              decoded.joints_size() == 32 && decoded.joints(31).velocity_radps() == -0.5,
              "target lost context, authority or topology");

        *request.mutable_context() = *context;
        request.set_request_id("arm-request");
        request.set_operation(a::CONTROL_OPERATION_ARM);
        *request.mutable_lease() = command.lease();
        a::ControlRequest request_copy;
        Check(request_copy.ParseFromString(request.SerializeAsString()) &&
              request_copy.request_id() == "arm-request" &&
              request_copy.operation() == a::CONTROL_OPERATION_ARM &&
              request_copy.lease().generation() == 7, "management lost request association");
        result.set_request_id(request.request_id()); result.set_request_sequence(42);
        result.set_operation(request.operation()); result.set_code(a::CONTROL_RESULT_CODE_ACCEPTED);
        a::ControlResult result_copy;
        Check(result_copy.ParseFromString(result.SerializeAsString()) &&
              result_copy.code() != a::CONTROL_RESULT_CODE_COMPLETED && !result_copy.has_lease(),
              "queued management result conflated with completion or lease grant");

        // Zero is a valid measurement; absent temperature/voltage remain unknown.
        sample.set_joint_name("joint_0"); sample.set_sample_sequence(9);
        sample.set_receive_time_ns(150); sample.set_receive_fresh(true);
        sample.set_position_rad(0); sample.set_position_valid(true);
        *feedback.add_joints() = sample;
        feedback.set_controller_session("reset-epoch-2");
        feedback.set_owner_generation(7); feedback.set_owner_producer_session("producer-3");
        feedback.set_last_accepted_command_sequence(42);
        feedback.set_last_applied_command_sequence(41);
        feedback.set_simulation(true); feedback.set_physics_step(12);
        feedback.set_simulation_time_ns(12000000);
        feedback.set_backend_failure(true);
        feedback.set_fault_reason(a::REJECTION_REASON_BACKEND_FAILURE);
        feedback.set_lease_expires_time_ns(200);
        a::ControllerFeedback feedback_copy;
        Check(feedback_copy.ParseFromString(feedback.SerializeAsString()), "feedback decode failed");
        Check(feedback_copy.joints(0).position_valid() && feedback_copy.joints(0).position_rad() == 0 &&
              !feedback_copy.joints(0).temperature_valid() && !feedback_copy.joints(0).voltage_valid() &&
              feedback_copy.last_applied_command_sequence() == 41 && feedback_copy.physics_step() == 12 &&
              feedback_copy.backend_failure() && feedback_copy.fault_reason() == a::REJECTION_REASON_BACKEND_FAILURE &&
              feedback_copy.lease_expires_time_ns() == 200,
              "feedback conflated validity, application or time");

        // Existing v1 field numbers and management flags retain their old shape.
        const auto* legacy = openember::msgs::actuator::v1::JointCommand::descriptor();
        CheckField(legacy, 3, "command_time_monotonic_ns", Field::TYPE_UINT64);
        CheckField(legacy, 5, "enable", Field::TYPE_BOOL);
        CheckField(legacy, 6, "disable", Field::TYPE_BOOL);
        CheckField(legacy, 7, "emergency_stop", Field::TYPE_BOOL);
        CheckField(legacy, 8, "clear_fault", Field::TYPE_BOOL);
        CheckField(legacy, 9, "joints", Field::TYPE_MESSAGE, true);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
