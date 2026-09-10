#pragma once

#include <cstdint>

#include "esp_err.h"
#include "sdkconfig.h"

namespace sp01 {

#ifdef CONFIG_SP01_VIRTUAL_IO

// Published each controller tick so the web UI can render machine state.
struct VioStatus {
    std::uint8_t do_bits{0};
    const char* state{"-"};
    const char* fault{"-"};
    const char* mode{"-"};
    const char* disposition{"UNDECIDED"};
    float weight_kg{0.0F};
    float target_kg{0.0F};
    std::uint32_t cycle_id{0};
    bool broken_bag_sim{false};
    bool reject_window{false};
    std::uint64_t broken_bag_detected_us{0};
};

// Pending operator commands, consumed by the virtual controller task.
enum class VioCommand : std::uint8_t {
    None,
    Reset,
    ClearFault,
    BrokenBagOn,
    BrokenBagOff,
    RejectWindow,
};

esp_err_t vio_start();

std::uint8_t vio_di();
void vio_publish(const VioStatus& status);
VioCommand vio_take_command();
void vio_set_ip(const char* ip);

// Starts the virtual bench controller task.
void vio_control_start();

// Supplied by the application.
bool vio_link_up();

#endif  // CONFIG_SP01_VIRTUAL_IO

}  // namespace sp01
