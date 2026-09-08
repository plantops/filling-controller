#pragma once

#include "driver/uart.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sp01/model.hpp"

#include <cstddef>
#include <cstdint>

namespace sp01 {

struct Tlb485Config {
    uart_port_t uart{UART_NUM_1};
    int baud{9600};
    std::uint8_t slave{1};
    bool calibration_writes{false};
};

struct Tlb485Diagnostics {
    std::uint32_t polls_ok{0};
    std::uint32_t comm_errors{0};
    esp_err_t last_error{ESP_OK};
    std::uint8_t unit_code{0xFF};
    std::uint8_t division_code{0xFF};
    std::uint8_t decimals{0};
    bool metadata_valid{false};
};

class Tlb485 {
public:
    esp_err_t init(const Tlb485Config& config) noexcept;
    esp_err_t poll_once(std::uint64_t now_us) noexcept;

    [[nodiscard]] WeightSnapshot snapshot() const noexcept;
    [[nodiscard]] Tlb485Diagnostics diagnostics() const noexcept;

    esp_err_t calibration_zero() noexcept;
    esp_err_t calibration_span(float reference_kg) noexcept;

private:
    Tlb485Config config_{};
    mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    SemaphoreHandle_t bus_mutex_{nullptr};
    WeightSnapshot snapshot_{};
    Tlb485Diagnostics diagnostics_{};

    esp_err_t read_holding(std::uint16_t start,
                           std::uint16_t count,
                           std::uint16_t* out) noexcept;
    esp_err_t write_single(std::uint16_t address, std::uint16_t value) noexcept;
    esp_err_t write_pair(std::uint16_t start,
                         std::uint16_t first,
                         std::uint16_t second) noexcept;
    esp_err_t read_exact(std::uint8_t* data, std::size_t size, int timeout_ms) noexcept;
    esp_err_t load_metadata() noexcept;
    [[nodiscard]] float register_weight_to_kg(std::uint32_t magnitude, bool negative) const noexcept;
    [[nodiscard]] bool kg_to_register_weight(float kg, std::uint32_t& raw) const noexcept;
};

}  // namespace sp01
