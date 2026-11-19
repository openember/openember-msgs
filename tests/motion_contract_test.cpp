#include "openember/msgs/motion/v1/motion.pb.h"
#include "openember/msgs/actuator/v2/actuator.pb.h"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/unknown_field_set.h>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <stdexcept>
#include <string>
namespace m = openember::msgs::motion::v1;
using Field = google::protobuf::FieldDescriptor;
void Check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void FieldIs(const google::protobuf::Descriptor* d, int tag, const char* name, Field::Type type) {
    const auto* f = d->FindFieldByNumber(tag);
    Check(f && f->name() == name && f->type() == type, "motion wire field changed");
}
void Bounds(const char* filename) {
    std::ifstream input(filename); Check(input.good(), "missing options file");
    std::map<std::string,int> sizes, counts;
    const std::regex rule(R"(^([^ #]+) (max_size|max_count):([1-9][0-9]*)$)");
    for (std::string line; std::getline(input,line);) {
        std::smatch match;
        if (!std::regex_match(line,match,rule)) continue;
        auto& bounds = match[2] == "max_size" ? sizes : counts;
        Check(bounds.emplace(match[1],std::stoi(match[3])).second, "duplicate static bound");
    }
    const auto* file = m::MotionCommand::descriptor()->file();
    for (int i=0; i<file->message_type_count(); ++i) {
        const auto* message = file->message_type(i);
        for (int j=0; j<message->field_count(); ++j) {
            const auto* field = message->field(j);
            if (field->type() == Field::TYPE_STRING) {
                const auto& name = field->name();
                const int expected = name == "message" ? 160 :
                    name == "clock_domain" || name == "model_revision" || name == "configuration_revision" ? 128 : 64;
                Check(sizes.count(field->full_name()) && sizes.at(field->full_name()) == expected, "missing/wrong motion string bound");
            }
            if (field->is_repeated())
                Check(counts.count(field->full_name()) && counts.at(field->full_name()) ==
                      (field->name() == "modes" ? 3 : 32), "missing/wrong motion collection bound");
            Check(field->type() != Field::TYPE_BYTES && !field->is_map(), "unbounded opaque motion payload introduced");
        }
    }
}
m::MotionCapabilities Capabilities(const std::string& robot, bool lateral, bool stand) {
    m::MotionCapabilities caps; caps.set_contract_version(1);
    caps.set_robot_id(robot); caps.set_motion_id("base"); caps.set_motion_session("motion-epoch");
    caps.set_model_revision("sha256:model"); caps.set_configuration_revision("sha256:config");
    caps.set_clock_domain("host-monotonic:boot"); caps.set_body_heading_frame_id("base_heading");
    caps.set_max_command_duration_ns(200000000); caps.set_max_lease_duration_ns(3000000000);
    caps.set_command_timeout_ns(100000000); caps.set_command_loss_response(m::COMMAND_LOSS_RESPONSE_STOP_AND_HOLD);
    caps.set_stop_timeout_ns(1000000000); caps.set_state_stale_after_ns(500000000);
    caps.set_sequence(1); caps.set_publish_time_ns(100);
    for (auto mode : {m::MOTION_MODE_PASSIVE, m::MOTION_MODE_HOLD, m::MOTION_MODE_LOCOMOTION}) {
        auto* supported = caps.add_modes(); supported->set_mode(mode); supported->set_availability(m::AVAILABILITY_AVAILABLE);
    }
    auto* limits = caps.mutable_velocity_limits();
    auto set = [](m::AxisLimits* axis, bool supported, double minimum, double maximum) {
        axis->set_supported(supported); axis->set_minimum(minimum); axis->set_maximum(maximum);
    };
    set(limits->mutable_vx_mps(),true,-1,2);
    set(limits->mutable_vy_mps(),lateral,lateral ? -.4 : 0,lateral ? .4 : 0);
    set(limits->mutable_yaw_rate_radps(),true,-1.3,1.3);
    if (stand) {
        auto* action = caps.add_actions(); action->set_action_id("openember.motion.stand.v1");
        action->set_availability(m::AVAILABILITY_AVAILABLE); action->set_max_duration_ns(20000000000);
        action->set_completion_mode(m::MOTION_MODE_HOLD);
    }
    auto* recovery = caps.add_actions(); recovery->set_action_id("openember.motion.recover.v1");
    recovery->set_availability(m::AVAILABILITY_NOT_SUPPORTED); recovery->set_reason(m::REJECTION_REASON_NOT_SUPPORTED);
    recovery->set_message("no validated recovery implementation");
    return caps;
}
int main(int argc, char** argv) {
    try {
        Check(argc == 2, "Nanopb options path required"); Bounds(argv[1]);
        Check(m::MotionCommand::descriptor()->full_name() == "openember.msgs.motion.v1.MotionCommand", "namespace changed");
        Check(m::MotionCommand::descriptor()->field_count() == 5, "management leaked into velocity command");
        FieldIs(m::MotionCommand::descriptor(),1,"context",Field::TYPE_MESSAGE);
        FieldIs(m::MotionCommand::descriptor(),2,"lease",Field::TYPE_MESSAGE);
        FieldIs(m::MotionCommand::descriptor(),3,"frame_id",Field::TYPE_STRING);
        FieldIs(m::MotionCommand::descriptor(),4,"velocity",Field::TYPE_MESSAGE);
        FieldIs(m::MotionCommand::descriptor(),5,"mode_generation",Field::TYPE_UINT64);
        FieldIs(m::MotionState::descriptor(),30,"mode_generation",Field::TYPE_UINT64);
        FieldIs(m::MotionState::descriptor(),31,"fault_reason",Field::TYPE_ENUM);
        FieldIs(m::MotionResult::descriptor(),16,"mode_generation",Field::TYPE_UINT64);
        FieldIs(m::CommandContext::descriptor(),3,"motion_session",Field::TYPE_STRING);
        FieldIs(m::CommandContext::descriptor(),5,"configuration_revision",Field::TYPE_STRING);
        FieldIs(m::CommandContext::descriptor(),8,"sequence",Field::TYPE_UINT64);
        FieldIs(m::CommandContext::descriptor(),11,"expires_time_ns",Field::TYPE_UINT64);
        FieldIs(m::MotionState::descriptor(),11,"actuation_state",Field::TYPE_ENUM);
        FieldIs(m::MotionState::descriptor(),17,"last_accepted_command_sequence",Field::TYPE_UINT64);
        FieldIs(m::MotionState::descriptor(),18,"last_consumed_command_sequence",Field::TYPE_UINT64);
        FieldIs(m::MotionResult::descriptor(),8,"status",Field::TYPE_ENUM);
        Check(m::MOTION_MODE_UNSPECIFIED == 0 && m::MOTION_MODE_DISABLED == 1 && m::MOTION_MODE_LOCOMOTION == 4 &&
              m::MOTION_MODE_ACTION == 5 && m::MOTION_MODE_WAITING_ASSISTANCE == 6, "mode values changed");
        Check(m::RESULT_STATUS_ACCEPTED == 1 && m::RESULT_STATUS_SUCCEEDED == 3 && m::RESULT_STATUS_TIMED_OUT == 7,
              "result values changed");
        m::MotionState state;
        Check(!state.online() && state.mode() == m::MOTION_MODE_UNSPECIFIED &&
              state.actuation_state() == m::ACTUATION_STATE_UNSPECIFIED && state.readiness() == m::READINESS_UNSPECIFIED &&
              !state.owner_generation() && !state.mode_generation(), "default status grants readiness or authority");
        m::MotionCommand command;
        Check(!command.has_context() && !command.has_lease() && !command.has_velocity(), "default command appears intentional");
        command.mutable_velocity();
        Check(command.SerializeAsString() == std::string("\x22\x00",2), "explicit zero velocity presence lost");
        m::MotionCommand zero; Check(zero.ParseFromString(command.SerializeAsString()) && zero.has_velocity() &&
              zero.velocity().vx_mps() == 0, "zero target conflated with missing target");
        auto* context = command.mutable_context();
        context->set_robot_id("p05w"); context->set_motion_id("base"); context->set_motion_session("motion-epoch");
        context->set_model_revision("sha256:model"); context->set_configuration_revision("sha256:config");
        context->set_source_id("f710"); context->set_producer_session("remote-start-2"); context->set_sequence(42);
        context->set_clock_domain("host-monotonic:boot"); context->set_issued_time_ns(100); context->set_expires_time_ns(200);
        command.mutable_lease()->set_generation(7); command.mutable_lease()->set_token("motion-only-token");
        command.set_mode_generation(9);
        command.set_frame_id("base_heading"); command.mutable_velocity()->set_vx_mps(-.7);
        command.mutable_velocity()->set_vy_mps(.4); command.mutable_velocity()->set_yaw_rate_radps(1.3);
        m::MotionCommand copy; Check(copy.ParseFromString(command.SerializeAsString()), "velocity roundtrip failed");
        Check(copy.context().SerializeAsString() == context->SerializeAsString() && copy.lease().generation() == 7 &&
              copy.velocity().vx_mps() == -.7 && copy.velocity().vy_mps() == .4 && copy.velocity().yaw_rate_radps() == 1.3 &&
              copy.frame_id() == "base_heading" && copy.mode_generation() == 9, "SI values or identity lost");
        Check(m::MotionLease::descriptor() != openember::msgs::actuator::v2::ControlLease::descriptor(), "lease types aliased");
        m::MotionRequest request; Check(request.intent_case() == m::MotionRequest::INTENT_NOT_SET, "unset request has intent");
        *request.mutable_context() = *context; *request.mutable_lease() = command.lease(); request.set_request_id("request-42");
        request.mutable_control()->set_operation(m::CONTROL_OPERATION_ARM);
        request.mutable_mode()->set_mode(m::MOTION_MODE_LOCOMOTION);
        Check(!request.has_control() && request.has_mode(), "mode request implicitly contains arm");
        request.mutable_action()->set_action_id("openember.motion.stand.v1");
        Check(!request.has_mode() && request.has_action(), "action and mode intent combined");
        m::MotionRequest action_copy; Check(action_copy.ParseFromString(request.SerializeAsString()) && action_copy.has_action() &&
              action_copy.action().action_id() == "openember.motion.stand.v1", "action ID changed");
        request.mutable_cancel()->set_target_request_id("request-42"); request.set_request_id("cancel-43");
        m::MotionRequest cancel_copy; Check(cancel_copy.ParseFromString(request.SerializeAsString()) && !cancel_copy.has_action() &&
              cancel_copy.cancel().target_request_id() == "request-42", "cancel correlation lost");
        const auto* intent = m::MotionRequest::descriptor()->FindOneofByName("intent");
        Check(intent && intent->field_count() == 4 && intent->field(0)->number() == 4 && intent->field(3)->number() == 7,
              "request intent isolation changed");
        for (const auto& caps : {Capabilities("differential",false,false), Capabilities("omni",true,false),
                                 Capabilities("quadruped",true,true), Capabilities("humanoid",true,true)}) {
            m::MotionCapabilities decoded; Check(decoded.ParseFromString(caps.SerializeAsString()), "capabilities roundtrip failed");
            Check(decoded.robot_id() == caps.robot_id() && decoded.modes_size() == 3 && decoded.velocity_limits().has_vx_mps() &&
                  decoded.velocity_limits().has_vy_mps() && decoded.velocity_limits().has_yaw_rate_radps(), "capability fields lost");
            Check(decoded.velocity_limits().vy_mps().supported() == (caps.robot_id() != "differential"), "lateral capability guessed from shape");
            const auto& recovery = decoded.actions(decoded.actions_size()-1);
            Check(recovery.availability() == m::AVAILABILITY_NOT_SUPPORTED && !recovery.max_duration_ns() &&
                  recovery.completion_mode() == m::MOTION_MODE_UNSPECIFIED, "unsupported recovery advertised executable");
        }
        Check(m::MotionCapabilities::descriptor()->FindFieldByName("joints") == nullptr &&
              m::MotionCapabilities::descriptor()->FindFieldByName("robot_type") == nullptr, "morphology dependency introduced");
        m::MotionResult result; Check(result.status() == m::RESULT_STATUS_UNSPECIFIED && !result.has_lease(), "default result grants success");
        result.set_request_id("request-42"); result.set_request_sequence(42); result.set_status(m::RESULT_STATUS_ACCEPTED);
        m::MotionResult result_copy; Check(result_copy.ParseFromString(result.SerializeAsString()) &&
              result_copy.status() != m::RESULT_STATUS_SUCCEEDED && !result_copy.has_lease() && result_copy.request_sequence() == 42,
              "acceptance conflated with success");
        // Protobuf preserves unknown values. Receivers must explicitly reject them
        // on control paths; this wire test is NOT a runtime authorization test.
        request.mutable_mode()->set_mode(static_cast<m::MotionMode>(12345));
        request.GetReflection()->MutableUnknownFields(&request)->AddVarint(100,9);
        m::MotionRequest unknown; Check(unknown.ParseFromString(request.SerializeAsString()) &&
              !m::MotionMode_IsValid(unknown.mode().mode()) &&
              unknown.GetReflection()->GetUnknownFields(unknown).field_count() == 1, "unknown data preservation changed");
        state.set_sequence(19); state.set_mode(m::MOTION_MODE_ACTION); state.set_active_request_id("request-42");
        state.set_active_action_id("openember.motion.stand.v1"); state.set_actuation_state(m::ACTUATION_STATE_UNKNOWN);
        state.set_simulation(true); state.set_paused(true); state.set_physics_step(200); state.set_simulation_time_ns(200000000);
        m::MotionState state_copy; Check(state_copy.ParseFromString(state.SerializeAsString()) && state_copy.paused() &&
              state_copy.sequence() == 19 && state_copy.physics_step() == 200 && state_copy.actuation_state() == m::ACTUATION_STATE_UNKNOWN,
              "unknown enable state/simulation clock lost");
        std::cout << "MOTION_WIRE_CONTRACT passed morphology_examples=4 runtime_validation=not_implemented\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
