#include "sp01/controller_explain.hpp"

namespace sp01 {
namespace {

std::uint16_t bit_of(Block b) noexcept {
    return static_cast<std::uint16_t>(1U << static_cast<std::uint16_t>(b));
}

void set_block(Explain& e, Block b) noexcept {
    e.blocks = static_cast<std::uint16_t>(e.blocks | bit_of(b));
}

bool weight_fresh(std::uint64_t now_us, const WeightSnapshot& w,
                  std::uint64_t stale_us) noexcept {
    if (w.quality != WeightQuality::Good) return false;
    if (w.sample_time_us > now_us) return false;
    return (now_us - w.sample_time_us) <= stale_us;
}

float forward_distance(float from_deg, float to_deg) noexcept {
    float d = to_deg - from_deg;
    while (d < 0.0F) d += 360.0F;
    while (d >= 360.0F) d -= 360.0F;
    return d;
}

}  // namespace

const char* block_name(Block block) noexcept {
    switch (block) {
        case Block::FeederNotRunning: return "FEEDER_NOT_RUNNING";
        case Block::DownstreamNotReady: return "DOWNSTREAM_NOT_READY";
        case Block::MotorNotRunning: return "MOTOR_NOT_RUNNING";
        case Block::NoInitiative: return "NO_INITIATIVE";
        case Block::WaitingFillPosition: return "WAITING_FILL_POSITION";
        case Block::WaitingBag: return "WAITING_BAG";
        case Block::WeightStale: return "WEIGHT_STALE";
        case Block::WeightFault: return "WEIGHT_FAULT";
        case Block::WeightMoving: return "WEIGHT_MOVING";
        case Block::PositionInvalid: return "POSITION_INVALID";
        case Block::WaitingPushAngle: return "WAITING_PUSH_ANGLE";
        case Block::Faulted: return "FAULTED";
        case Block::Count: break;
    }
    return "UNKNOWN";
}

const char* block_text_en(Block block) noexcept {
    switch (block) {
        case Block::FeederNotRunning: return "Hopper feeder is not running";
        case Block::DownstreamNotReady: return "Downstream conveyor not ready";
        case Block::MotorNotRunning: return "Machine main motor not running";
        case Block::NoInitiative: return "No process initiative";
        case Block::WaitingFillPosition: return "Waiting for fill position";
        case Block::WaitingBag: return "Waiting for a bag on the spout";
        case Block::WeightStale: return "Weight reading is stale";
        case Block::WeightFault: return "Weight system fault";
        case Block::WeightMoving: return "Waiting for the weight to settle";
        case Block::PositionInvalid: return "Shaft position not decoded";
        case Block::WaitingPushAngle: return "Waiting for the push angle";
        case Block::Faulted: return "Controller is faulted";
        case Block::Count: break;
    }
    return "";
}

const char* block_text_vi(Block block) noexcept {
    switch (block) {
        case Block::FeederNotRunning: return "Cấp liệu phễu chưa chạy";
        case Block::DownstreamNotReady: return "Băng tải phía sau chưa sẵn sàng";
        case Block::MotorNotRunning: return "Động cơ chính của máy chưa chạy";
        case Block::NoInitiative: return "Chưa có lệnh chạy quy trình";
        case Block::WaitingFillPosition: return "Đang chờ vị trí nạp";
        case Block::WaitingBag: return "Đang chờ bao vào vòi";
        case Block::WeightStale: return "Số cân quá cũ, không cập nhật";
        case Block::WeightFault: return "Hệ cân báo lỗi";
        case Block::WeightMoving: return "Đang chờ cân ổn định";
        case Block::PositionInvalid: return "Chưa giải mã được vị trí trục";
        case Block::WaitingPushAngle: return "Đang chờ tới góc đẩy bao";
        case Block::Faulted: return "Bộ điều khiển đang lỗi";
        case Block::Count: break;
    }
    return "";
}

Explain explain_controller(const ControllerSnapshot& snapshot,
                           const ControllerConfig& config,
                           const InputImage& inputs,
                           const WeightSnapshot& weight,
                           const PositionSnapshot& position,
                           std::uint64_t now_us) noexcept {
    Explain e{};

    const bool feeder = input(inputs, Di::HopperFeederRunning);
    const bool downstream = input(inputs, Di::DownstreamConveyorReady);
    const bool motor = input(inputs, Di::MachineMotorRunning);
    const bool initiative = input(inputs, Di::ProcessInitiative);
    const bool auto_mode = snapshot.mode == OperationMode::Auto;

    // Permissive: AUTO needs all four; MANUAL is feeder plus initiative only.
    e.permissive = auto_mode ? (feeder && downstream && motor && initiative)
                             : (feeder && initiative);

    if (!feeder) set_block(e, Block::FeederNotRunning);
    if (!initiative) set_block(e, Block::NoInitiative);
    if (auto_mode && !downstream) set_block(e, Block::DownstreamNotReady);
    if (auto_mode && !motor) set_block(e, Block::MotorNotRunning);

    e.weight_ready = weight_fresh(now_us, weight, config.weight_stale_us);
    if (weight.quality == WeightQuality::Fault) {
        set_block(e, Block::WeightFault);
    } else if (!e.weight_ready) {
        set_block(e, Block::WeightStale);
    }

    // The desired output mask comes from the FSM itself, never from a table
    // kept somewhere else.
    e.desired_mask = 0;
    for (std::size_t i = 0; i < snapshot.outputs.channels.size(); ++i) {
        if (snapshot.outputs.channels[i]) {
            e.desired_mask = static_cast<std::uint8_t>(e.desired_mask | (1U << i));
        }
    }

    e.push_angle_deg = snapshot.push_angle_deg;
    e.angle_to_push_deg = -1.0F;

    switch (snapshot.state) {
        case State::WaitPermissive:
            e.next_state = auto_mode ? State::WaitFillPosition : State::BagAcquire;
            break;
        case State::WaitFillPosition:
            e.next_state = State::BagAcquire;
            if (!input(inputs, Di::FillPosition)) {
                set_block(e, Block::WaitingFillPosition);
            }
            break;
        case State::BagAcquire:
            e.next_state = State::BagVerify;
            if (!input(inputs, Di::BagPresent)) set_block(e, Block::WaitingBag);
            break;
        case State::BagVerify:
            e.next_state = State::TareReady;
            break;
        case State::TareReady:
            e.next_state = State::CoarseFill;
            break;
        case State::CoarseFill:
            e.next_state = State::FineFill;
            break;
        case State::FineFill:
            e.next_state = State::Cutoff;
            break;
        case State::Cutoff:
            e.next_state = State::Settle;
            break;
        case State::Settle:
            e.next_state = auto_mode ? State::WaitDischarge : State::Complete;
            if (!weight.stable) set_block(e, Block::WeightMoving);
            break;
        case State::RejectWait:
        case State::WaitDischarge:
            e.next_state = State::Push;
            if (!position.valid) {
                set_block(e, Block::PositionInvalid);
            } else {
                set_block(e, Block::WaitingPushAngle);
                e.angle_to_push_deg =
                    forward_distance(position.angle_deg, snapshot.push_angle_deg);
            }
            break;
        case State::Push:
            e.next_state = State::Complete;
            break;
        case State::Complete:
            e.next_state = auto_mode ? State::WaitFillPosition : State::WaitPermissive;
            break;
        case State::Fault:
            e.next_state = State::WaitPermissive;
            set_block(e, Block::Faulted);
            break;
    }

    e.ready = e.blocks == 0;
    return e;
}

}  // namespace sp01
