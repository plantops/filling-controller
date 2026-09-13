#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "sp01/model.hpp"

#include <cstdint>

namespace sp01 {

class BoardIo {
public:
    BoardIo() = default;

    esp_err_t init(std::uint8_t di_invert_mask = 0,
                   std::uint8_t do_invert_mask = 0) noexcept;
    esp_err_t read_inputs(InputImage& image) noexcept;
    esp_err_t commit_outputs(const OutputImage& image) noexcept;
    esp_err_t force_safe() noexcept;

    // Semantic DO image corresponding to the last successful TCA9554 write.
    // This is commanded output state, not field-actuator feedback.
    OutputImage last_commanded_outputs() const noexcept;

private:
    std::uint8_t di_invert_mask_{0};
    std::uint8_t do_invert_mask_{0};
    std::uint8_t last_physical_byte_{0xFF};
    i2c_master_bus_handle_t bus_{nullptr};
    i2c_master_dev_handle_t dev_{nullptr};

    esp_err_t write_physical_byte(std::uint8_t value, bool force = false) noexcept;
};

}  // namespace sp01
