#pragma once

#include <cstdint>

#include "esp_err.h"
#include "sdkconfig.h"

namespace sp01 {

#ifdef CONFIG_SP01_VIRTUAL_IO

// Starts the virtual-I/O HTTP server. Requires a working network interface.
esp_err_t vio_start();

// Current virtual channel states, bit0 = channel 1.
uint8_t vio_di();
uint8_t vio_do();

// Address shown in the UI status bar.
void vio_set_ip(const char* ip);

// Supplied by the application so the UI can show link state.
bool vio_link_up();

#endif  // CONFIG_SP01_VIRTUAL_IO

}  // namespace sp01
