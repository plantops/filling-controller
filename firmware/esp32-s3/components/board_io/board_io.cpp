#include "sp01/board_io.hpp"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"

#include <array>
#include <cstddef>

namespace sp01 {
namespace {

constexpr char kTag[] = "board_io";
constexpr std::uint8_t kTca9554Address = 0x20;
constexpr std::uint8_t kRegOutput = 0x01;
constexpr std::uint8_t kRegConfig = 0x03;
constexpr gpio_num_t kI2cSda = GPIO_NUM_42;
constexpr gpio_num_t kI2cScl = GPIO_NUM_41;
constexpr std::array<gpio_num_t, 8> kDiPins = {
    GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7,
    GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_10, GPIO_NUM_11,
};

}  // namespace

esp_err_t BoardIo::init(std::uint8_t di_invert_mask,
                        std::uint8_t do_invert_mask) noexcept {
    di_invert_mask_ = di_invert_mask;
    do_invert_mask_ = do_invert_mask;

    for (const auto pin : kDiPins) {
        gpio_config_t cfg{};
        cfg.pin_bit_mask = 1ULL << static_cast<unsigned>(pin);
        cfg.mode = GPIO_MODE_INPUT;
        cfg.pull_up_en = GPIO_PULLUP_ENABLE;
        cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        cfg.intr_type = GPIO_INTR_DISABLE;
        ESP_RETURN_ON_ERROR(gpio_config(&cfg), kTag, "DI gpio config");
    }

    i2c_master_bus_config_t bus_cfg{};
    bus_cfg.i2c_port = I2C_NUM_0;
    bus_cfg.sda_io_num = kI2cSda;
    bus_cfg.scl_io_num = kI2cScl;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &bus_), kTag, "I2C init");

    i2c_device_config_t dev_cfg{};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = kTca9554Address;
    dev_cfg.scl_speed_hz = 100000;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus_, &dev_cfg, &dev_),
                        kTag, "TCA9554 add");

    // TCA9554 powers up as inputs. Preload the safe output latch before
    // switching its pins to output mode, so initialization cannot pulse loads.
    const std::uint8_t safe_physical = do_invert_mask_;
    std::uint8_t out_cmd[2] = {kRegOutput, safe_physical};
    ESP_RETURN_ON_ERROR(i2c_master_transmit(dev_, out_cmd, sizeof(out_cmd), 20),
                        kTag, "preload safe DO");

    std::uint8_t cfg_cmd[2] = {kRegConfig, 0x00};
    ESP_RETURN_ON_ERROR(i2c_master_transmit(dev_, cfg_cmd, sizeof(cfg_cmd), 20),
                        kTag, "enable DO");

    last_physical_byte_ = safe_physical;
    ESP_LOGI(kTag, "8DI GPIO4..11; 8DO TCA9554@0x20; outputs safe");
    return ESP_OK;
}

esp_err_t BoardIo::read_inputs(InputImage& image) noexcept {
    for (std::size_t i = 0; i < kDiPins.size(); ++i) {
        const bool raw = gpio_get_level(kDiPins[i]) != 0;
        const bool invert = (di_invert_mask_ & (1U << i)) != 0;
        image.di[i] = raw ^ invert;
    }
    return ESP_OK;
}

esp_err_t BoardIo::write_physical_byte(std::uint8_t value, bool force) noexcept {
    if (!dev_) return ESP_ERR_INVALID_STATE;
    if (!force && value == last_physical_byte_) return ESP_OK;
    std::uint8_t cmd[2] = {kRegOutput, value};
    const esp_err_t err = i2c_master_transmit(dev_, cmd, sizeof(cmd), 20);
    if (err == ESP_OK) last_physical_byte_ = value;
    return err;
}

esp_err_t BoardIo::commit_outputs(const OutputImage& image) noexcept {
    std::uint8_t physical = 0;
    for (std::size_t i = 0; i < image.channels.size(); ++i) {
        const bool invert = (do_invert_mask_ & (1U << i)) != 0;
        const bool level = image.channels[i] ^ invert;
        if (level) physical |= static_cast<std::uint8_t>(1U << i);
    }
    return write_physical_byte(physical);
}

esp_err_t BoardIo::force_safe() noexcept {
    return write_physical_byte(do_invert_mask_, true);
}

}  // namespace sp01
