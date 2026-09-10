// SP01 G2 physical DO sequential diagnostic.
//
// Purpose: prove DO1..DO8 one at a time with a small external dummy load.
// Machine actuators must remain disconnected.
//
// Sequence after boot:
//   - all outputs OFF for 10 s (safe-start observation window)
//   - DO1 ON 500 ms, all OFF 2 s
//   - DO2 ON 500 ms, all OFF 2 s
//   - ... DO8
//   - repeat forever
//
// Board output stage is open-collector/sinking. A field-side dummy load and
// external DC supply are required for physical evidence.

#include <cstdint>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr char kTag[] = "g2do";
constexpr gpio_num_t kI2cSda = GPIO_NUM_42;
constexpr gpio_num_t kI2cScl = GPIO_NUM_41;
constexpr std::uint8_t kTcaAddr = 0x20;
constexpr std::uint8_t kTcaRegOutput = 0x01;
constexpr std::uint8_t kTcaRegConfig = 0x03;

constexpr TickType_t kBootSafeMs = pdMS_TO_TICKS(10000);
constexpr TickType_t kOnMs = pdMS_TO_TICKS(500);
constexpr TickType_t kOffGapMs = pdMS_TO_TICKS(2000);

const char* const kDoNames[8] = {
    "scanner.down",
    "bag_detect_air",
    "bag.push",
    "dosing.valve_a",
    "dosing.valve_b",
    "dosing.valve_c",
    "filling.motor",
    "spout.aeration",
};

i2c_master_bus_handle_t g_bus = nullptr;
i2c_master_dev_handle_t g_tca = nullptr;

[[noreturn]] void halt(const char* what, esp_err_t err) {
    ESP_LOGE(kTag, "%s: %s", what, esp_err_to_name(err));
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}

void write_outputs(std::uint8_t bits) {
    const std::uint8_t cmd[2] = {kTcaRegOutput, bits};
    const esp_err_t err = i2c_master_transmit(g_tca, cmd, sizeof(cmd), 100);
    if (err != ESP_OK) halt("TCA9554 write", err);
}

void init_outputs_safe() {
    i2c_master_bus_config_t bus_cfg{};
    bus_cfg.i2c_port = I2C_NUM_0;
    bus_cfg.sda_io_num = kI2cSda;
    bus_cfg.scl_io_num = kI2cScl;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&bus_cfg, &g_bus);
    if (err != ESP_OK) halt("i2c_new_master_bus", err);

    err = i2c_master_probe(g_bus, kTcaAddr, 100);
    if (err != ESP_OK) halt("TCA9554 probe", err);

    i2c_device_config_t dev_cfg{};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = kTcaAddr;
    dev_cfg.scl_speed_hz = 100000;
    err = i2c_master_bus_add_device(g_bus, &dev_cfg, &g_tca);
    if (err != ESP_OK) halt("TCA9554 add", err);

    // Preload all-OFF before enabling TCA9554 pins as outputs.
    write_outputs(0x00);

    const std::uint8_t cfg[2] = {kTcaRegConfig, 0x00};
    err = i2c_master_transmit(g_tca, cfg, sizeof(cfg), 100);
    if (err != ESP_OK) halt("TCA9554 config", err);

    write_outputs(0x00);
}

}  // namespace

extern "C" void app_main(void) {
    ESP_LOGI(kTag, "================================================");
    ESP_LOGI(kTag, "SP01 G2 DO SEQUENTIAL TEST");
    ESP_LOGI(kTag, "MACHINE ACTUATORS MUST BE DISCONNECTED");
    ESP_LOGI(kTag, "sequence: 10 s ALL OFF, then DO1..DO8 one-hot");
    ESP_LOGI(kTag, "pulse: 500 ms ON, 2 s ALL OFF; sequence repeats");
    ESP_LOGI(kTag, "================================================");

    init_outputs_safe();
    ESP_LOGI(kTag, "ALL OFF  bits=0x00  safe-start window 10 s");
    vTaskDelay(kBootSafeMs);

    unsigned cycle = 0;
    for (;;) {
        ++cycle;
        ESP_LOGI(kTag, "===== SWEEP %u START =====", cycle);

        for (int i = 0; i < 8; ++i) {
            const std::uint8_t bit = static_cast<std::uint8_t>(1U << i);
            write_outputs(bit);
            ESP_LOGI(kTag, "DO%d %-20s ON   bits=0x%02x", i + 1, kDoNames[i], bit);
            vTaskDelay(kOnMs);

            write_outputs(0x00);
            ESP_LOGI(kTag, "DO%d %-20s OFF  bits=0x00", i + 1, kDoNames[i]);
            vTaskDelay(kOffGapMs);
        }

        ESP_LOGI(kTag, "===== SWEEP %u COMPLETE; ALL OFF =====", cycle);
    }
}
