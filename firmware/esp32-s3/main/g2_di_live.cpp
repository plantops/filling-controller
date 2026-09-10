// SP01 G2 physical DI live diagnostic.
//
// Purpose: make the eight real isolated digital inputs easy to test with one
// dry-contact jumper. Physical outputs are forced to the safe latch and are
// never commanded ON by this build.
//
// Bench wiring for this gate (Waveshare passive/dry-contact topology):
//   INPUT DGND ---- dry jumper/switch ---- DI1..DI8, one at a time
//   INPUT COM  ---- leave floating for this dry-contact test
//   machine actuator wiring disconnected

#include <cstdint>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr char kTag[] = "g2di";

constexpr gpio_num_t kI2cSda = GPIO_NUM_42;
constexpr gpio_num_t kI2cScl = GPIO_NUM_41;
constexpr std::uint8_t kTcaAddr = 0x20;
constexpr std::uint8_t kTcaRegOutput = 0x01;
constexpr std::uint8_t kTcaRegConfig = 0x03;

constexpr gpio_num_t kDiPins[8] = {
    GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6,  GPIO_NUM_7,
    GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_10, GPIO_NUM_11,
};

const char* const kDiNames[8] = {
    "hopper.feeder_running",
    "downstream.conveyor_ready",
    "machine.motor_running",
    "process.initiative",
    "cycle.fill_position",
    "bag.present",
    "position.discharge_ref_a",
    "position.discharge_ref_b",
};

i2c_master_bus_handle_t g_i2c_bus = nullptr;
i2c_master_dev_handle_t g_tca = nullptr;

[[noreturn]] void halt(const char* what, esp_err_t err) {
    ESP_LOGE(kTag, "%s: %s", what, esp_err_to_name(err));
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}

void force_outputs_safe() {
    i2c_master_bus_config_t bus_cfg{};
    bus_cfg.i2c_port = I2C_NUM_0;
    bus_cfg.sda_io_num = kI2cSda;
    bus_cfg.scl_io_num = kI2cScl;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&bus_cfg, &g_i2c_bus);
    if (err != ESP_OK) halt("i2c_new_master_bus", err);

    err = i2c_master_probe(g_i2c_bus, kTcaAddr, 100);
    if (err != ESP_OK) halt("TCA9554 probe", err);

    i2c_device_config_t dev_cfg{};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = kTcaAddr;
    dev_cfg.scl_speed_hz = 100000;
    err = i2c_master_bus_add_device(g_i2c_bus, &dev_cfg, &g_tca);
    if (err != ESP_OK) halt("TCA9554 add", err);

    // Load the OFF latch before enabling the expander pins as outputs.
    const std::uint8_t off[2] = {kTcaRegOutput, 0x00};
    err = i2c_master_transmit(g_tca, off, sizeof(off), 100);
    if (err != ESP_OK) halt("TCA9554 output safe", err);

    const std::uint8_t cfg[2] = {kTcaRegConfig, 0x00};
    err = i2c_master_transmit(g_tca, cfg, sizeof(cfg), 100);
    if (err != ESP_OK) halt("TCA9554 config", err);

    ESP_LOGI(kTag, "OUTPUTS SAFE: TCA9554 latch=0x00; no DO command in this build");
}

void init_inputs() {
    for (int i = 0; i < 8; ++i) {
        gpio_config_t cfg{};
        cfg.pin_bit_mask = 1ULL << static_cast<unsigned>(kDiPins[i]);
        cfg.mode = GPIO_MODE_INPUT;
        cfg.pull_up_en = GPIO_PULLUP_ENABLE;
        cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        cfg.intr_type = GPIO_INTR_DISABLE;
        const esp_err_t err = gpio_config(&cfg);
        if (err != ESP_OK) halt("DI gpio_config", err);
    }
}

std::uint8_t read_di_raw() {
    std::uint8_t bits = 0;
    for (int i = 0; i < 8; ++i) {
        if (gpio_get_level(kDiPins[i])) bits |= static_cast<std::uint8_t>(1U << i);
    }
    return bits;
}

void print_change(std::uint8_t before, std::uint8_t now) {
    const std::uint8_t changed = static_cast<std::uint8_t>(before ^ now);
    ESP_LOGI(kTag, "DI RAW 0x%02x -> 0x%02x  changed=0x%02x", before, now, changed);
    for (int i = 0; i < 8; ++i) {
        const std::uint8_t mask = static_cast<std::uint8_t>(1U << i);
        if ((changed & mask) == 0) continue;
        // Unwired/open is pulled high. Closing INPUT DGND -> DIx makes the
        // corresponding raw GPIO bit low on this board input stage.
        const bool closed = (now & mask) == 0;
        ESP_LOGI(kTag, "DI%d %-28s %s", i + 1, kDiNames[i], closed ? "CLOSED" : "OPEN");
    }
}

}  // namespace

extern "C" void app_main(void) {
    ESP_LOGI(kTag, "================================================");
    ESP_LOGI(kTag, "SP01 G2 DI LIVE TEST");
    ESP_LOGI(kTag, "machine/actuator wiring disconnected");
    ESP_LOGI(kTag, "dry contact: INPUT DGND <-> one DIx at a time");
    ESP_LOGI(kTag, "leave INPUT COM floating for this test");
    ESP_LOGI(kTag, "expected all open: raw=0xFF");
    ESP_LOGI(kTag, "================================================");

    force_outputs_safe();
    init_inputs();

    std::uint8_t stable = read_di_raw();
    std::uint8_t candidate = stable;
    int candidate_count = 0;
    ESP_LOGI(kTag, "DI LIVE START raw=0x%02x", stable);

    std::uint64_t last_hb_us = static_cast<std::uint64_t>(esp_timer_get_time());

    for (;;) {
        const std::uint8_t now = read_di_raw();

        if (now != candidate) {
            candidate = now;
            candidate_count = 1;
        } else if (candidate != stable) {
            ++candidate_count;
            // 3 x 20 ms = about 60 ms debounce; enough for a hand jumper.
            if (candidate_count >= 3) {
                const std::uint8_t before = stable;
                stable = candidate;
                candidate_count = 0;
                print_change(before, stable);
            }
        }

        const std::uint64_t t = static_cast<std::uint64_t>(esp_timer_get_time());
        if (t - last_hb_us >= 5000000ULL) {
            ESP_LOGI(kTag, "HB up=%llu ms raw=0x%02x",
                     static_cast<unsigned long long>(t / 1000ULL), stable);
            last_hb_us = t;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
