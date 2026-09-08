#include "esp_log.h"
#include "sp01/model.hpp"

namespace {
constexpr char kTag[] = "sp01";
}

extern "C" void app_main(void) {
    const auto outputs = sp01::safe_output_image();

    ESP_LOGI(kTag, "SP01 firmware v0.1 M0 scaffold");
    ESP_LOGI(kTag, "controller model loaded; physical I/O not enabled");
    ESP_LOGI(kTag, "desired safe output image: %s",
             sp01::all_outputs_off(outputs) ? "ALL OFF" : "INVALID");
}
