#include "sp01/tlb485.hpp"

#include "esp_log.h"
#include "freertos/task.h"

#include <array>
#include <cmath>
#include <cstring>

namespace sp01 {
namespace {

constexpr char kTag[] = "tlb485";
constexpr int kTx = 17;
constexpr int kRx = 18;
constexpr int kRts = 21;

// Laumas TLB-family Modbus holding registers, converted from 40001 notation
// to zero-based Modbus addresses.
constexpr std::uint16_t kRegCommand = 5;      // 40006
constexpr std::uint16_t kRegStatus = 6;       // 40007
constexpr std::uint16_t kRegDivisionUnit = 13; // 40014
constexpr std::uint16_t kRegCalibrationSample = 36; // 40037..40038

constexpr std::uint16_t kStatusFaultMask = 0x003F; // documented bits 0..5
constexpr std::uint16_t kStatusNetNegative = 1U << 8;
constexpr std::uint16_t kStatusStable = 1U << 11;

std::uint16_t crc16(const std::uint8_t* data, std::size_t length) noexcept {
    std::uint16_t crc = 0xFFFF;
    for (std::size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1U) ? static_cast<std::uint16_t>((crc >> 1U) ^ 0xA001U)
                             : static_cast<std::uint16_t>(crc >> 1U);
        }
    }
    return crc;
}

std::uint8_t decimals_for_division(std::uint8_t code) noexcept {
    if (code <= 6) return 0;
    if (code <= 9) return 1;
    if (code <= 12) return 2;
    if (code <= 15) return 3;
    if (code <= 18) return 4;
    return 0xFF;
}

float pow10u(std::uint8_t n) noexcept {
    float v = 1.0F;
    while (n-- > 0) v *= 10.0F;
    return v;
}

}  // namespace

esp_err_t Tlb485::init(const Tlb485Config& config) noexcept {
    config_ = config;
    bus_mutex_ = xSemaphoreCreateMutex();
    if (!bus_mutex_) return ESP_ERR_NO_MEM;

    uart_config_t uart_cfg{};
    uart_cfg.baud_rate = config_.baud;
    uart_cfg.data_bits = UART_DATA_8_BITS;
    uart_cfg.parity = UART_PARITY_DISABLE;
    uart_cfg.stop_bits = UART_STOP_BITS_1;
    uart_cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_cfg.source_clk = UART_SCLK_DEFAULT;

    (void)uart_driver_delete(config_.uart);
    esp_err_t err = uart_driver_install(config_.uart, 256, 256, 0, nullptr, 0);
    if (err != ESP_OK) return err;
    if ((err = uart_param_config(config_.uart, &uart_cfg)) != ESP_OK) return err;
    if ((err = uart_set_pin(config_.uart, kTx, kRx, kRts, UART_PIN_NO_CHANGE)) != ESP_OK) return err;
    if ((err = uart_set_mode(config_.uart, UART_MODE_RS485_HALF_DUPLEX)) != ESP_OK) return err;

    uart_flush_input(config_.uart);
    ESP_LOGI(kTag, "RS485 UART%d GPIO%d/%d RTS%d %d 8N1 slave=%u",
             static_cast<int>(config_.uart), kTx, kRx, kRts, config_.baud, config_.slave);

    err = load_metadata();
    if (err != ESP_OK) {
        portENTER_CRITICAL(&mux_);
        diagnostics_.last_error = err;
        diagnostics_.comm_errors++;
        portEXIT_CRITICAL(&mux_);
        ESP_LOGW(kTag, "TLB metadata not available yet: %s", esp_err_to_name(err));
    }
    return ESP_OK;
}

esp_err_t Tlb485::read_exact(std::uint8_t* data, std::size_t size, int timeout_ms) noexcept {
    std::size_t offset = 0;
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    while (offset < size) {
        const TickType_t now = xTaskGetTickCount();
        if (now >= deadline) return ESP_ERR_TIMEOUT;
        const int n = uart_read_bytes(config_.uart,
                                      data + offset,
                                      size - offset,
                                      deadline - now);
        if (n < 0) return ESP_FAIL;
        offset += static_cast<std::size_t>(n);
    }
    return ESP_OK;
}

esp_err_t Tlb485::read_holding(std::uint16_t start,
                               std::uint16_t count,
                               std::uint16_t* out) noexcept {
    if (!out || count == 0 || count > 16) return ESP_ERR_INVALID_ARG;
    std::array<std::uint8_t, 8> req{};
    req[0] = config_.slave;
    req[1] = 0x03;
    req[2] = static_cast<std::uint8_t>(start >> 8);
    req[3] = static_cast<std::uint8_t>(start & 0xFF);
    req[4] = static_cast<std::uint8_t>(count >> 8);
    req[5] = static_cast<std::uint8_t>(count & 0xFF);
    const std::uint16_t crc = crc16(req.data(), 6);
    req[6] = static_cast<std::uint8_t>(crc & 0xFF);
    req[7] = static_cast<std::uint8_t>(crc >> 8);

    uart_flush_input(config_.uart);
    if (uart_write_bytes(config_.uart, req.data(), req.size()) != static_cast<int>(req.size())) return ESP_FAIL;
    if (uart_wait_tx_done(config_.uart, pdMS_TO_TICKS(50)) != ESP_OK) return ESP_ERR_TIMEOUT;

    const std::size_t expected = 5U + 2U * count;
    std::array<std::uint8_t, 37> resp{};
    esp_err_t err = read_exact(resp.data(), expected, 100);
    if (err != ESP_OK) return err;
    if (resp[0] != config_.slave) return ESP_ERR_INVALID_RESPONSE;
    if (resp[1] & 0x80U) return ESP_ERR_INVALID_RESPONSE;
    if (resp[1] != 0x03 || resp[2] != 2U * count) return ESP_ERR_INVALID_RESPONSE;
    const std::uint16_t got_crc = static_cast<std::uint16_t>(resp[expected - 2]) |
                                  static_cast<std::uint16_t>(resp[expected - 1] << 8);
    if (crc16(resp.data(), expected - 2) != got_crc) return ESP_ERR_INVALID_CRC;

    for (std::uint16_t i = 0; i < count; ++i) {
        out[i] = static_cast<std::uint16_t>(resp[3 + 2 * i] << 8) | resp[4 + 2 * i];
    }
    return ESP_OK;
}

esp_err_t Tlb485::write_single(std::uint16_t address, std::uint16_t value) noexcept {
    std::array<std::uint8_t, 8> req{};
    req[0] = config_.slave;
    req[1] = 0x06;
    req[2] = static_cast<std::uint8_t>(address >> 8);
    req[3] = static_cast<std::uint8_t>(address & 0xFF);
    req[4] = static_cast<std::uint8_t>(value >> 8);
    req[5] = static_cast<std::uint8_t>(value & 0xFF);
    const std::uint16_t crc = crc16(req.data(), 6);
    req[6] = static_cast<std::uint8_t>(crc & 0xFF);
    req[7] = static_cast<std::uint8_t>(crc >> 8);

    uart_flush_input(config_.uart);
    if (uart_write_bytes(config_.uart, req.data(), req.size()) != static_cast<int>(req.size())) return ESP_FAIL;
    if (uart_wait_tx_done(config_.uart, pdMS_TO_TICKS(50)) != ESP_OK) return ESP_ERR_TIMEOUT;
    std::array<std::uint8_t, 8> resp{};
    esp_err_t err = read_exact(resp.data(), resp.size(), 100);
    if (err != ESP_OK) return err;
    if (std::memcmp(req.data(), resp.data(), 6) != 0) return ESP_ERR_INVALID_RESPONSE;
    const std::uint16_t got_crc = static_cast<std::uint16_t>(resp[6]) |
                                  static_cast<std::uint16_t>(resp[7] << 8);
    return crc16(resp.data(), 6) == got_crc ? ESP_OK : ESP_ERR_INVALID_CRC;
}

esp_err_t Tlb485::write_pair(std::uint16_t start,
                             std::uint16_t first,
                             std::uint16_t second) noexcept {
    std::array<std::uint8_t, 13> req{};
    req[0] = config_.slave;
    req[1] = 0x10;
    req[2] = static_cast<std::uint8_t>(start >> 8);
    req[3] = static_cast<std::uint8_t>(start & 0xFF);
    req[4] = 0;
    req[5] = 2;
    req[6] = 4;
    req[7] = static_cast<std::uint8_t>(first >> 8);
    req[8] = static_cast<std::uint8_t>(first & 0xFF);
    req[9] = static_cast<std::uint8_t>(second >> 8);
    req[10] = static_cast<std::uint8_t>(second & 0xFF);
    const std::uint16_t crc = crc16(req.data(), 11);
    req[11] = static_cast<std::uint8_t>(crc & 0xFF);
    req[12] = static_cast<std::uint8_t>(crc >> 8);

    uart_flush_input(config_.uart);
    if (uart_write_bytes(config_.uart, req.data(), req.size()) != static_cast<int>(req.size())) return ESP_FAIL;
    if (uart_wait_tx_done(config_.uart, pdMS_TO_TICKS(50)) != ESP_OK) return ESP_ERR_TIMEOUT;
    std::array<std::uint8_t, 8> resp{};
    esp_err_t err = read_exact(resp.data(), resp.size(), 100);
    if (err != ESP_OK) return err;
    if (resp[0] != config_.slave || resp[1] != 0x10 || resp[2] != req[2] || resp[3] != req[3] ||
        resp[4] != 0 || resp[5] != 2) return ESP_ERR_INVALID_RESPONSE;
    const std::uint16_t got_crc = static_cast<std::uint16_t>(resp[6]) |
                                  static_cast<std::uint16_t>(resp[7] << 8);
    return crc16(resp.data(), 6) == got_crc ? ESP_OK : ESP_ERR_INVALID_CRC;
}

esp_err_t Tlb485::load_metadata() noexcept {
    std::uint16_t reg = 0;
    const esp_err_t err = read_holding(kRegDivisionUnit, 1, &reg);
    if (err != ESP_OK) return err;
    const std::uint8_t unit = static_cast<std::uint8_t>(reg >> 8);
    const std::uint8_t division = static_cast<std::uint8_t>(reg & 0xFF);
    const std::uint8_t decimals = decimals_for_division(division);
    if (decimals == 0xFF) return ESP_ERR_NOT_SUPPORTED;

    portENTER_CRITICAL(&mux_);
    diagnostics_.unit_code = unit;
    diagnostics_.division_code = division;
    diagnostics_.decimals = decimals;
    diagnostics_.metadata_valid = true;
    portEXIT_CRITICAL(&mux_);
    ESP_LOGI(kTag, "TLB metadata unit=%u division=%u decimals=%u", unit, division, decimals);
    return ESP_OK;
}

float Tlb485::register_weight_to_kg(std::uint32_t magnitude, bool negative) const noexcept {
    const float display = static_cast<float>(magnitude) / pow10u(diagnostics_.decimals);
    float kg = display;
    switch (diagnostics_.unit_code) {
        case 0: kg = display; break;
        case 1: kg = display / 1000.0F; break;
        case 2: kg = display * 1000.0F; break;
        default: return NAN;
    }
    return negative ? -kg : kg;
}

bool Tlb485::kg_to_register_weight(float kg, std::uint32_t& raw) const noexcept {
    if (!diagnostics_.metadata_valid || kg < 0.0F || !std::isfinite(kg)) return false;
    float display = kg;
    switch (diagnostics_.unit_code) {
        case 0: display = kg; break;
        case 1: display = kg * 1000.0F; break;
        case 2: display = kg / 1000.0F; break;
        default: return false;
    }
    const double scaled = static_cast<double>(display) * pow10u(diagnostics_.decimals);
    if (scaled < 0.0 || scaled > 4294967295.0) return false;
    raw = static_cast<std::uint32_t>(std::llround(scaled));
    return true;
}

esp_err_t Tlb485::poll_once(std::uint64_t now_us) noexcept {
    if (!bus_mutex_ || xSemaphoreTake(bus_mutex_, pdMS_TO_TICKS(150)) != pdTRUE) return ESP_ERR_TIMEOUT;
    esp_err_t result = ESP_OK;

    if (!diagnostics_.metadata_valid) {
        result = load_metadata();
    }

    std::uint16_t regs[5]{};
    if (result == ESP_OK) result = read_holding(kRegStatus, 5, regs);

    if (result == ESP_OK) {
        const std::uint16_t status = regs[0];
        const std::uint32_t net_magnitude = (static_cast<std::uint32_t>(regs[3]) << 16U) | regs[4];
        const float kg = register_weight_to_kg(net_magnitude, (status & kStatusNetNegative) != 0);

        WeightSnapshot next{};
        next.net_kg = kg;
        next.sample_time_us = now_us;
        next.quality = ((status & kStatusFaultMask) != 0 || !std::isfinite(kg))
                           ? WeightQuality::Fault
                           : WeightQuality::Good;
        next.stable = (status & kStatusStable) != 0;

        portENTER_CRITICAL(&mux_);
        next.sequence = snapshot_.sequence + 1;
        snapshot_ = next;
        diagnostics_.polls_ok++;
        diagnostics_.last_error = ESP_OK;
        portEXIT_CRITICAL(&mux_);
    } else {
        portENTER_CRITICAL(&mux_);
        diagnostics_.last_error = result;
        diagnostics_.comm_errors++;
        portEXIT_CRITICAL(&mux_);
    }

    xSemaphoreGive(bus_mutex_);
    return result;
}

WeightSnapshot Tlb485::snapshot() const noexcept {
    portENTER_CRITICAL(&mux_);
    const WeightSnapshot copy = snapshot_;
    portEXIT_CRITICAL(&mux_);
    return copy;
}

Tlb485Diagnostics Tlb485::diagnostics() const noexcept {
    portENTER_CRITICAL(&mux_);
    const Tlb485Diagnostics copy = diagnostics_;
    portEXIT_CRITICAL(&mux_);
    return copy;
}

esp_err_t Tlb485::calibration_zero() noexcept {
    if (!config_.calibration_writes) return ESP_ERR_NOT_ALLOWED;
    if (!bus_mutex_ || xSemaphoreTake(bus_mutex_, pdMS_TO_TICKS(250)) != pdTRUE) return ESP_ERR_TIMEOUT;
    const esp_err_t err = write_single(kRegCommand, 100);
    xSemaphoreGive(bus_mutex_);
    return err;
}

esp_err_t Tlb485::calibration_span(float reference_kg) noexcept {
    if (!config_.calibration_writes) return ESP_ERR_NOT_ALLOWED;
    std::uint32_t raw = 0;
    if (!kg_to_register_weight(reference_kg, raw)) return ESP_ERR_INVALID_ARG;
    if (!bus_mutex_ || xSemaphoreTake(bus_mutex_, pdMS_TO_TICKS(250)) != pdTRUE) return ESP_ERR_TIMEOUT;
    esp_err_t err = write_pair(kRegCalibrationSample,
                               static_cast<std::uint16_t>(raw >> 16U),
                               static_cast<std::uint16_t>(raw & 0xFFFFU));
    if (err == ESP_OK) err = write_single(kRegCommand, 101);
    xSemaphoreGive(bus_mutex_);
    return err;
}

}  // namespace sp01
