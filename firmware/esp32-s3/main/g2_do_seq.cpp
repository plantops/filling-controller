// SP01 G2 physical DO -> DI loopback diagnostic.
//
// Purpose: prove DO1..DO8 one at a time without an external lamp by using the
// board's own DI channels/LEDs as a light physical load and readback path.
// Machine actuators must remain disconnected.
//
// Bench wiring for this test only:
//   INPUT DICOM   : floating
//   OUTPUT DOCOM  : floating (no inductive load in this loopback)
//   INPUT DGND --- OUTPUT GND
//   DO1 --------- DI1
//   DO2 --------- DI2
//   ...
//   DO8 --------- DI8
//
// With all outputs OFF the DI raw byte must be 0xFF. When DOx turns ON, the
// NPN/open-collector output sinks the corresponding dry-contact DI input, so
// exactly that DI bit must go low and its DI LED should light.
//
// Sequence after boot:
//   - all outputs OFF for 10 s (safe-start/reset observation window)
//   - one sweep DO1..DO8, 500 ms per channel
//   - each ON/OFF is physically checked through the matching DI
//   - after the sweep all outputs stay OFF; press RESET to repeat

#include <cstdint>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr char kTag[] = "g2loop";
constexpr gpio_num_t kI2cSda = GPIO_NUM_42;
constexpr gpio_num_t kI2cScl = GPIO_NUM_41;
constexpr std::uint8_t kTcaAddr = 0x20;
constexpr std::uint8_t kTcaRegOutput = 0x01;
constexpr std::uint8_t kTcaRegConfig = 0x03;

constexpr gpio_num_t kDiPins[8] = {
    GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6,  GPIO_NUM_7,
    GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_10, GPIO_NUM_11,
};

constexpr TickType_t kBootSafeMs = pdMS_TO_TICKS(10000);
constexpr TickType_t kSettleMs = pdMS_TO_TICKS(100);
constexpr TickType_t kOnRemainMs = pdMS_TO_TICKS(400);
constexpr TickType_t kOffGapMs = pdMS_TO_TICKS(1000);

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

}  // namespace

extern "C" void app_main(void) {
    ESP_LOGI(kTag, "================================================");
    ESP_LOGI(kTag, "SP01 G2 DO->DI LOOPBACK TEST");
    ESP_LOGI(kTag, "MACHINE ACTUATORS MUST BE DISCONNECTED");
    ESP_LOGI(kTag, "USB power only is sufficient for this low-current loopback");
    ESP_LOGI(kTag, "INPUT DICOM floating; OUTPUT DOCOM floating");
    ESP_LOGI(kTag, "wire INPUT DGND <-> OUTPUT GND");
    ESP_LOGI(kTag, "wire DO1->DI1 ... DO8->DI8");
    ESP_LOGI(kTag, "10 s ALL OFF, then one 500 ms one-hot sweep");
    ESP_LOGI(kTag, "================================================");

    init_outputs_safe();
    init_inputs();

    const std::uint8_t boot_raw = read_di_raw();
    ESP_LOGI(kTag, "SAFE START: DO=0x00 DIraw=0x%02x; wait 10 s", boot_raw);
    vTaskDelay(kBootSafeMs);

    std::uint8_t pass_mask = 0;
    std::uint8_t fail_mask = 0;

    const std::uint8_t before = read_di_raw();
    if (before != 0xFF) {
        ESP_LOGE(kTag, "BASELINE FAIL: expected DIraw=0xff with all DO OFF, got 0x%02x", before);
    } else {
        ESP_LOGI(kTag, "BASELINE PASS: all DO OFF -> DIraw=0xff");
    }

    for (int i = 0; i < 8; ++i) {
        const std::uint8_t bit = static_cast<std::uint8_t>(1U << i);
        const std::uint8_t expected_on = static_cast<std::uint8_t>(0xFFU & ~bit);

        write_outputs(bit);
        vTaskDelay(kSettleMs);
        const std::uint8_t on_raw = read_di_raw();
        const bool on_ok = on_raw == expected_on;
        ESP_LOGI(kTag,
                 "DO%d %-20s ON  bits=0x%02x DIraw=0x%02x expected=0x%02x %s",
                 i + 1, kDoNames[i], bit, on_raw, expected_on,
                 on_ok ? "PASS" : "FAIL");
        vTaskDelay(kOnRemainMs);

        write_outputs(0x00);
        vTaskDelay(kSettleMs);
        const std::uint8_t off_raw = read_di_raw();
        const bool off_ok = off_raw == 0xFF;
        ESP_LOGI(kTag,
                 "DO%d %-20s OFF bits=0x00 DIraw=0x%02x expected=0xff %s",
                 i + 1, kDoNames[i], off_raw, off_ok ? "PASS" : "FAIL");

        if (on_ok && off_ok) {
            pass_mask |= bit;
        } else {
            fail_mask |= bit;
        }
        vTaskDelay(kOffGapMs);
    }

    write_outputs(0x00);
    const std::uint8_t final_raw = read_di_raw();
    ESP_LOGI(kTag, "================================================");
    ESP_LOGI(kTag, "G2 DO LOOPBACK SUMMARY pass_mask=0x%02x fail_mask=0x%02x final_DIraw=0x%02x",
             pass_mask, fail_mask, final_raw);
    ESP_LOGI(kTag, "%s", (pass_mask == 0xFF && fail_mask == 0 && final_raw == 0xFF)
                              ? "G2 DO LOOPBACK VERDICT: PASS"
                              : "G2 DO LOOPBACK VERDICT: NOT PASS");
    ESP_LOGI(kTag, "ALL OUTPUTS HELD OFF. Press RESET to repeat.");
    ESP_LOGI(kTag, "================================================");

    for (;;) {
        write_outputs(0x00);
        ESP_LOGI(kTag, "HOLD SAFE: DO=0x00 DIraw=0x%02x", read_di_raw());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
