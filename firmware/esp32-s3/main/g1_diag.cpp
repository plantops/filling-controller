// SP01 G1 self-test firmware.
//
// Purpose: prove the board over the native USB Serial/JTAG console alone, with
// no UART adapter, no scope and no network. Runs a fixed sequence of stages,
// prints an explicit PASS/FAIL line for each, then heartbeats forever and
// reprints the summary every 10 s.
//
// Design rules for this build:
//   1. app_main() never returns. A returning app_main prints nothing further
//      and is indistinguishable from a dead board on this console.
//   2. No stage failure is fatal. Every stage is attempted and reported.
//   3. No Wi-Fi. It cannot be tested remotely over USB and is out of G1 scope.
//   4. No actuator motion. TCA9554 outputs are driven to the safe latch only.
//
// Machine wiring must remain disconnected.

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_chip_info.h"
#include "esp_eth.h"
#include "esp_eth_mac_spi.h"
#include "esp_eth_netif_glue.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr char kTag[] = "g1";

// Board pinout, mirrored from components/board_io and the W5500 wiring.
constexpr gpio_num_t kI2cSda = GPIO_NUM_42;
constexpr gpio_num_t kI2cScl = GPIO_NUM_41;
constexpr uint8_t kTcaAddr = 0x20;
constexpr uint8_t kTcaRegInput = 0x00;
constexpr uint8_t kTcaRegOutput = 0x01;
constexpr uint8_t kTcaRegConfig = 0x03;

constexpr gpio_num_t kDiPins[8] = {
    GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6,  GPIO_NUM_7,
    GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_10, GPIO_NUM_11,
};

constexpr spi_host_device_t kEthSpi = SPI2_HOST;
constexpr gpio_num_t kEthSck = GPIO_NUM_15;
constexpr gpio_num_t kEthMosi = GPIO_NUM_13;
constexpr gpio_num_t kEthMiso = GPIO_NUM_14;
constexpr gpio_num_t kEthCs = GPIO_NUM_16;
constexpr gpio_num_t kEthRst = GPIO_NUM_39;
constexpr gpio_num_t kEthIrq = GPIO_NUM_12;
constexpr int kEthPhyAddr = 1;
constexpr int kEthSpiHz = 8 * 1000 * 1000;  // conservative for bring-up

// W5500 common register block.
constexpr uint16_t kW5500RegVersion = 0x0039;
constexpr uint8_t kW5500VersionExpected = 0x04;
constexpr uint16_t kW5500RegPhyCfg = 0x002E;

enum class Verdict { kPass, kFail, kSkip };

struct Stage {
    const char* name;
    Verdict verdict;
    char detail[96];
};

constexpr int kStageCount = 7;
Stage g_stages[kStageCount] = {
    {"chip", Verdict::kSkip, ""},   {"flash", Verdict::kSkip, ""},
    {"i2c_tca9554", Verdict::kSkip, ""}, {"di_read", Verdict::kSkip, ""},
    {"spi_w5500", Verdict::kSkip, ""},   {"eth_link", Verdict::kSkip, ""},
    {"eth_dhcp", Verdict::kSkip, ""},
};

const char* verdict_text(Verdict v) {
    switch (v) {
        case Verdict::kPass: return "PASS";
        case Verdict::kFail: return "FAIL";
        default: return "SKIP";
    }
}

void record(int index, Verdict v, const char* fmt, ...) {
    if (index < 0 || index >= kStageCount) return;
    g_stages[index].verdict = v;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_stages[index].detail, sizeof(g_stages[index].detail), fmt, ap);
    va_end(ap);
    ESP_LOGI(kTag, "STAGE %-12s %s  %s", g_stages[index].name, verdict_text(v),
             g_stages[index].detail);
}

const char* reset_reason_text() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: return "POWERON";
        case ESP_RST_EXT: return "EXT";
        case ESP_RST_SW: return "SW";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INT_WDT";
        case ESP_RST_TASK_WDT: return "TASK_WDT";
        case ESP_RST_WDT: return "WDT";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_USB: return "USB";
        case ESP_RST_JTAG: return "JTAG";
        default: return "UNKNOWN";
    }
}

// ---------------------------------------------------------------- stage 0/1

void stage_chip() {
    esp_chip_info_t info{};
    esp_chip_info(&info);
    uint8_t mac[6]{};
    const esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    record(0, Verdict::kPass, "cores=%d rev=%d.%d mac=%02x:%02x:%02x:%02x:%02x:%02x rst=%s",
           info.cores, info.revision / 100, info.revision % 100, mac[0], mac[1],
           mac[2], mac[3], mac[4], mac[5],
           err == ESP_OK ? reset_reason_text() : "MAC_ERR");
}

void stage_flash() {
    uint32_t size = 0;
    const esp_err_t err = esp_flash_get_size(nullptr, &size);
    if (err != ESP_OK) {
        record(1, Verdict::kFail, "esp_flash_get_size=%s", esp_err_to_name(err));
        return;
    }
    record(1, size >= (16u << 20) ? Verdict::kPass : Verdict::kFail,
           "size=%lu KiB (expect 16384 for N16R8)",
           static_cast<unsigned long>(size / 1024));
}

// ------------------------------------------------------------------ stage 2/3

i2c_master_bus_handle_t g_i2c_bus = nullptr;
i2c_master_dev_handle_t g_tca = nullptr;

void stage_i2c() {
    i2c_master_bus_config_t bus_cfg{};
    bus_cfg.i2c_port = I2C_NUM_0;
    bus_cfg.sda_io_num = kI2cSda;
    bus_cfg.scl_io_num = kI2cScl;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&bus_cfg, &g_i2c_bus);
    if (err != ESP_OK) {
        record(2, Verdict::kFail, "i2c_new_master_bus=%s", esp_err_to_name(err));
        return;
    }

    err = i2c_master_probe(g_i2c_bus, kTcaAddr, 100);
    if (err != ESP_OK) {
        record(2, Verdict::kFail, "probe 0x20=%s (expander absent or bus stuck)",
               esp_err_to_name(err));
        return;
    }

    i2c_device_config_t dev_cfg{};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = kTcaAddr;
    dev_cfg.scl_speed_hz = 100000;
    err = i2c_master_bus_add_device(g_i2c_bus, &dev_cfg, &g_tca);
    if (err != ESP_OK) {
        record(2, Verdict::kFail, "add_device=%s", esp_err_to_name(err));
        return;
    }

    // Safe latch BEFORE switching pins to outputs, so init cannot pulse a load.
    uint8_t safe[2] = {kTcaRegOutput, 0x00};
    err = i2c_master_transmit(g_tca, safe, sizeof(safe), 100);
    if (err != ESP_OK) {
        record(2, Verdict::kFail, "write OUTPUT=%s", esp_err_to_name(err));
        return;
    }
    uint8_t cfg[2] = {kTcaRegConfig, 0x00};
    err = i2c_master_transmit(g_tca, cfg, sizeof(cfg), 100);
    if (err != ESP_OK) {
        record(2, Verdict::kFail, "write CONFIG=%s", esp_err_to_name(err));
        return;
    }

    // Read back the input register as a liveness check.
    uint8_t reg = kTcaRegInput;
    uint8_t val = 0;
    err = i2c_master_transmit_receive(g_tca, &reg, 1, &val, 1, 100);
    if (err != ESP_OK) {
        record(2, Verdict::kFail, "readback=%s", esp_err_to_name(err));
        return;
    }
    record(2, Verdict::kPass, "TCA9554@0x20 ok, outputs safe, INPUT=0x%02x", val);
}

void stage_di() {
    uint8_t bits = 0;
    for (int i = 0; i < 8; ++i) {
        gpio_config_t cfg{};
        cfg.pin_bit_mask = 1ULL << static_cast<unsigned>(kDiPins[i]);
        cfg.mode = GPIO_MODE_INPUT;
        cfg.pull_up_en = GPIO_PULLUP_ENABLE;
        cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        cfg.intr_type = GPIO_INTR_DISABLE;
        const esp_err_t err = gpio_config(&cfg);
        if (err != ESP_OK) {
            record(3, Verdict::kFail, "gpio_config DI%d=%s", i + 1,
                   esp_err_to_name(err));
            return;
        }
        if (gpio_get_level(kDiPins[i])) bits |= static_cast<uint8_t>(1u << i);
    }
    record(3, Verdict::kPass, "DI1..8 raw=0x%02x (pulled up, unwired expects 0xFF)",
           bits);
}

// -------------------------------------------------------------------- stage 4/5

spi_device_handle_t g_w5500 = nullptr;

esp_err_t w5500_read_reg(uint16_t addr, uint8_t* out) {
    // W5500 frame: 16-bit address, control byte, then data.
    // Control byte for a common-register read: BSB=00000, RWB=0, OM=00 -> 0x00.
    uint8_t tx[4] = {static_cast<uint8_t>(addr >> 8),
                     static_cast<uint8_t>(addr & 0xFF), 0x00, 0x00};
    uint8_t rx[4] = {0, 0, 0, 0};
    spi_transaction_t t{};
    t.length = 8 * sizeof(tx);
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    const esp_err_t err = spi_device_polling_transmit(g_w5500, &t);
    if (err == ESP_OK) *out = rx[3];
    return err;
}

void stage_spi_w5500() {
    gpio_config_t rst{};
    rst.pin_bit_mask = 1ULL << static_cast<unsigned>(kEthRst);
    rst.mode = GPIO_MODE_OUTPUT;
    esp_err_t err = gpio_config(&rst);
    if (err != ESP_OK) {
        record(4, Verdict::kFail, "RST gpio=%s", esp_err_to_name(err));
        return;
    }
    gpio_set_level(kEthRst, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(kEthRst, 1);
    vTaskDelay(pdMS_TO_TICKS(100));  // W5500 needs >50 ms after reset release

    spi_bus_config_t bus{};
    bus.miso_io_num = kEthMiso;
    bus.mosi_io_num = kEthMosi;
    bus.sclk_io_num = kEthSck;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = 64;
    err = spi_bus_initialize(kEthSpi, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        record(4, Verdict::kFail, "spi_bus_initialize=%s", esp_err_to_name(err));
        return;
    }

    spi_device_interface_config_t dev{};
    dev.mode = 0;
    dev.clock_speed_hz = kEthSpiHz;
    dev.spics_io_num = kEthCs;
    dev.queue_size = 4;
    err = spi_bus_add_device(kEthSpi, &dev, &g_w5500);
    if (err != ESP_OK) {
        record(4, Verdict::kFail, "spi_bus_add_device=%s", esp_err_to_name(err));
        return;
    }

    uint8_t version = 0;
    err = w5500_read_reg(kW5500RegVersion, &version);
    if (err != ESP_OK) {
        record(4, Verdict::kFail, "VERSIONR read=%s", esp_err_to_name(err));
        return;
    }
    if (version != kW5500VersionExpected) {
        record(4, Verdict::kFail,
               "VERSIONR=0x%02x expected 0x04 (check CS/MOSI/MISO/SCK/RST)",
               version);
        return;
    }
    record(4, Verdict::kPass, "VERSIONR=0x04, SPI link to W5500 confirmed");
}

// Ethernet autonegotiation takes 1-3 s. A single read taken shortly after
// reset release is meaningless, so the link stage is evaluated continuously by
// the heartbeat and latches PASS on the first LNK=1 observed.
bool g_link_ever_up = false;
uint8_t g_phy_last = 0;

void poll_eth_link() {
    if (g_stages[4].verdict != Verdict::kPass) {
        record(5, Verdict::kSkip, "SPI stage did not pass");
        return;
    }
    uint8_t phy = 0;
    const esp_err_t err = w5500_read_reg(kW5500RegPhyCfg, &phy);
    if (err != ESP_OK) {
        record(5, Verdict::kFail, "PHYCFGR read=%s", esp_err_to_name(err));
        return;
    }
    g_phy_last = phy;

    // PHYCFGR bit0 = LNK, bit1 = SPD (1=100M), bit2 = DPX (1=full).
    // SPD and DPX are only meaningful while LNK is set.
    const bool link = (phy & 0x01) != 0;
    if (link) {
        g_link_ever_up = true;
        record(5, Verdict::kPass, "PHYCFGR=0x%02x link=UP speed=%s duplex=%s",
               phy, (phy & 0x02) ? "100M" : "10M",
               (phy & 0x04) ? "FULL" : "HALF");
    } else if (!g_link_ever_up) {
        record(5, Verdict::kFail,
               "PHYCFGR=0x%02x link=DOWN (autoneg pending, cable, or magnetics)",
               phy);
    } else {
        record(5, Verdict::kFail, "PHYCFGR=0x%02x link=DOWN after having been UP",
               phy);
    }
}


// ------------------------------------------------------------------- stage 6
// Full Ethernet stack: MAC driver, netif, DHCP client. This is what produces an
// IP address visible on the router. The raw-SPI stages above only prove the
// W5500 responds; they deliberately touch nothing above the physical layer.

esp_eth_handle_t g_eth = nullptr;
esp_netif_t* g_eth_netif = nullptr;
volatile bool g_got_ip = false;
volatile bool g_driver_link = false;
char g_ip_text[16] = "0.0.0.0";
uint8_t g_eth_mac[6] = {0, 0, 0, 0, 0, 0};

void eth_event_handler(void*, esp_event_base_t, int32_t id, void*) {
    switch (id) {
        case ETHERNET_EVENT_CONNECTED:
            ESP_LOGI(kTag, "ETH EVENT: link connected");
            g_driver_link = true;
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGW(kTag, "ETH EVENT: link disconnected");
            g_driver_link = false;
            g_got_ip = false;
            std::snprintf(g_ip_text, sizeof(g_ip_text), "0.0.0.0");
            break;
        case ETHERNET_EVENT_START:
            ESP_LOGI(kTag, "ETH EVENT: driver started");
            break;
        case ETHERNET_EVENT_STOP:
            ESP_LOGW(kTag, "ETH EVENT: driver stopped");
            break;
        default:
            break;
    }
}

void ip_event_handler(void*, esp_event_base_t, int32_t, void* data) {
    const auto* ev = static_cast<ip_event_got_ip_t*>(data);
    std::snprintf(g_ip_text, sizeof(g_ip_text), IPSTR, IP2STR(&ev->ip_info.ip));
    g_got_ip = true;
    ESP_LOGI(kTag, "ETH DHCP: ip=" IPSTR " mask=" IPSTR " gw=" IPSTR,
             IP2STR(&ev->ip_info.ip), IP2STR(&ev->ip_info.netmask),
             IP2STR(&ev->ip_info.gw));
}

void stage_eth_stack() {
    if (g_stages[4].verdict != Verdict::kPass) {
        record(6, Verdict::kSkip, "W5500 SPI stage did not pass");
        return;
    }

    // Release the raw-SPI probe handle; the Ethernet driver adds its own
    // device on the same bus and the same CS line.
    if (g_w5500 != nullptr) {
        spi_bus_remove_device(g_w5500);
        g_w5500 = nullptr;
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        record(6, Verdict::kFail, "esp_netif_init=%s", esp_err_to_name(err));
        return;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        record(6, Verdict::kFail, "event_loop=%s", esp_err_to_name(err));
        return;
    }

    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        record(6, Verdict::kFail, "gpio_install_isr_service=%s",
               esp_err_to_name(err));
        return;
    }

    spi_device_interface_config_t dev{};
    dev.mode = 0;
    dev.clock_speed_hz = kEthSpiHz;
    dev.spics_io_num = kEthCs;
    dev.queue_size = 20;

    eth_w5500_config_t w5500 = ETH_W5500_DEFAULT_CONFIG(kEthSpi, &dev);
    w5500.int_gpio_num = kEthIrq;

    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    mac_cfg.rx_task_stack_size = 4096;
    esp_eth_mac_t* mac = esp_eth_mac_new_w5500(&w5500, &mac_cfg);
    if (mac == nullptr) {
        record(6, Verdict::kFail, "esp_eth_mac_new_w5500 returned null");
        return;
    }

    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.phy_addr = kEthPhyAddr;
    phy_cfg.reset_gpio_num = -1;  // already reset by the SPI probe stage
    esp_eth_phy_t* phy = esp_eth_phy_new_w5500(&phy_cfg);
    if (phy == nullptr) {
        record(6, Verdict::kFail, "esp_eth_phy_new_w5500 returned null");
        return;
    }

    esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    err = esp_eth_driver_install(&eth_cfg, &g_eth);
    if (err != ESP_OK) {
        record(6, Verdict::kFail, "esp_eth_driver_install=%s",
               esp_err_to_name(err));
        return;
    }

    // The W5500 has no built-in MAC address; one must be supplied.
    err = esp_read_mac(g_eth_mac, ESP_MAC_ETH);
    if (err != ESP_OK) {
        record(6, Verdict::kFail, "esp_read_mac=%s", esp_err_to_name(err));
        return;
    }
    err = esp_eth_ioctl(g_eth, ETH_CMD_S_MAC_ADDR, g_eth_mac);
    if (err != ESP_OK) {
        record(6, Verdict::kFail, "set MAC=%s", esp_err_to_name(err));
        return;
    }

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    g_eth_netif = esp_netif_new(&netif_cfg);
    if (g_eth_netif == nullptr) {
        record(6, Verdict::kFail, "esp_netif_new returned null");
        return;
    }
    (void)esp_netif_set_hostname(g_eth_netif, "sp01-g1");

    err = esp_netif_attach(g_eth_netif, esp_eth_new_netif_glue(g_eth));
    if (err != ESP_OK) {
        record(6, Verdict::kFail, "esp_netif_attach=%s", esp_err_to_name(err));
        return;
    }

    (void)esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                     &eth_event_handler, nullptr);
    (void)esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                     &ip_event_handler, nullptr);

    err = esp_eth_start(g_eth);
    if (err != ESP_OK) {
        record(6, Verdict::kFail, "esp_eth_start=%s", esp_err_to_name(err));
        return;
    }

    record(6, Verdict::kFail,
           "driver up, MAC=%02x:%02x:%02x:%02x:%02x:%02x, awaiting DHCP",
           g_eth_mac[0], g_eth_mac[1], g_eth_mac[2], g_eth_mac[3], g_eth_mac[4],
           g_eth_mac[5]);
}

void poll_eth_dhcp() {
    if (g_eth == nullptr) return;

    // Once esp_eth owns the SPI device, the raw PHYCFGR read used by the
    // eth_link stage is no longer valid. The driver's own link event is the
    // authoritative source from here on, so stage 5 tracks it.
    if (g_driver_link) {
        record(5, Verdict::kPass, "link UP reported by esp_eth driver");
    } else if (g_stages[5].verdict == Verdict::kPass) {
        record(5, Verdict::kFail, "link DOWN reported by esp_eth driver");
    }

    if (g_got_ip) {
        record(6, Verdict::kPass, "ip=%s mac=%02x:%02x:%02x:%02x:%02x:%02x",
               g_ip_text, g_eth_mac[0], g_eth_mac[1], g_eth_mac[2], g_eth_mac[3],
               g_eth_mac[4], g_eth_mac[5]);
    }
}

// ---------------------------------------------------------------------- report

void print_summary() {
    int pass = 0, fail = 0, skip = 0;
    for (const auto& s : g_stages) {
        if (s.verdict == Verdict::kPass) ++pass;
        else if (s.verdict == Verdict::kFail) ++fail;
        else ++skip;
    }
    ESP_LOGI(kTag, "===== G1 SUMMARY  pass=%d fail=%d skip=%d =====", pass, fail,
             skip);
    for (const auto& s : g_stages) {
        ESP_LOGI(kTag, "  %-12s %-4s %s", s.name, verdict_text(s.verdict),
                 s.detail);
    }
    ESP_LOGI(kTag, "===== G1 VERDICT: %s =====",
             (fail == 0 && skip == 0) ? "ALL STAGES PASS" : "NOT PASS");
    if (g_stages[6].verdict != Verdict::kPass) {
        ESP_LOGI(kTag, "  note: DHCP needs a link plus a server; eth_dhcp latches");
        ESP_LOGI(kTag, "        PASS on the first lease. Link alone gives no IP.");
    }
}

void heartbeat_task(void*) {
    unsigned long beat = 0;
    for (;;) {
        ++beat;
        poll_eth_dhcp();
        ESP_LOGI(kTag, "HB %lu up=%llu ms heap=%lu link=%s ip=%s", beat,
                 static_cast<unsigned long long>(esp_timer_get_time() / 1000),
                 static_cast<unsigned long>(esp_get_free_heap_size()),
                 g_got_ip ? "UP" : "wait", g_ip_text);
        if (beat % 10 == 0) print_summary();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

}  // namespace

extern "C" void app_main(void) {
    ESP_LOGI(kTag, "================================================");
    ESP_LOGI(kTag, "SP01 G1 SELF-TEST  reset=%s", reset_reason_text());
    ESP_LOGI(kTag, "machine wiring must be disconnected");
    ESP_LOGI(kTag, "================================================");

    stage_chip();
    stage_flash();
    stage_i2c();
    stage_di();
    stage_spi_w5500();
    poll_eth_link();  // physical-layer sample before the stack starts
    stage_eth_stack();

    print_summary();

    xTaskCreate(heartbeat_task, "g1_hb", 4096, nullptr, 1, nullptr);

    // app_main must not return in a diagnostic build.
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}
