#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_eth.h"
#include "esp_eth_mac_spi.h"
#include "esp_eth_netif_glue.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
constexpr char TAG[] = "g1diag";
constexpr spi_host_device_t ETH_SPI = SPI2_HOST;
constexpr gpio_num_t ETH_SCK = GPIO_NUM_15;
constexpr gpio_num_t ETH_MOSI = GPIO_NUM_13;
constexpr gpio_num_t ETH_MISO = GPIO_NUM_14;
constexpr gpio_num_t ETH_CS = GPIO_NUM_16;
constexpr gpio_num_t ETH_IRQ = GPIO_NUM_12;
constexpr int ETH_RST = 39;
constexpr int ETH_PHY_ADDR = 1;
constexpr int ETH_SPI_HZ = 20 * 1000 * 1000;
constexpr gpio_num_t I2C_SDA = GPIO_NUM_42;
constexpr gpio_num_t I2C_SCL = GPIO_NUM_41;

esp_eth_handle_t g_eth = nullptr;

void heartbeat(void*) {
    for (;;) {
        ESP_LOGI(TAG, "HEARTBEAT");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void eth_event(void*, esp_event_base_t, int32_t id, void* data) {
    if (id == ETHERNET_EVENT_START) ESP_LOGI(TAG, "ETH START");
    if (id == ETHERNET_EVENT_CONNECTED) {
        uint8_t mac[6]{};
        auto h = *static_cast<esp_eth_handle_t*>(data);
        if (esp_eth_ioctl(h, ETH_CMD_G_MAC_ADDR, mac) == ESP_OK) {
            ESP_LOGI(TAG, "ETH LINK UP MAC=%02x:%02x:%02x:%02x:%02x:%02x",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        } else {
            ESP_LOGI(TAG, "ETH LINK UP");
        }
    }
    if (id == ETHERNET_EVENT_DISCONNECTED) ESP_LOGW(TAG, "ETH LINK DOWN");
}

void got_ip(void*, esp_event_base_t, int32_t, void* data) {
    const auto* ev = static_cast<ip_event_got_ip_t*>(data);
    ESP_LOGI(TAG, "DHCP IP=" IPSTR, IP2STR(&ev->ip_info.ip));
}

esp_err_t probe_tca9554() {
    i2c_master_bus_config_t bus_cfg{};
    bus_cfg.i2c_port = I2C_NUM_0;
    bus_cfg.sda_io_num = I2C_SDA;
    bus_cfg.scl_io_num = I2C_SCL;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;

    i2c_master_bus_handle_t bus = nullptr;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &bus), TAG, "I2C init");
    ESP_LOGI(TAG, "I2C READY");

    i2c_device_config_t dev_cfg{};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = 0x20;
    dev_cfg.scl_speed_hz = 100000;

    i2c_master_dev_handle_t dev = nullptr;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &dev_cfg, &dev), TAG, "TCA9554 add");

    uint8_t safe_latch[2] = {0x01, 0x00};
    ESP_RETURN_ON_ERROR(i2c_master_transmit(dev, safe_latch, sizeof(safe_latch), 20), TAG, "TCA9554 safe latch");
    uint8_t cfg[2] = {0x03, 0x00};
    ESP_RETURN_ON_ERROR(i2c_master_transmit(dev, cfg, sizeof(cfg), 20), TAG, "TCA9554 outputs");
    ESP_LOGI(TAG, "TCA9554 SAFE WRITE OK");
    return ESP_OK;
}

esp_err_t start_eth() {
    ESP_LOGI(TAG, "W5500 RESET/SPI BEGIN");

    gpio_config_t rst_cfg{};
    rst_cfg.pin_bit_mask = 1ULL << ETH_RST;
    rst_cfg.mode = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&rst_cfg), TAG, "RST gpio");
    gpio_set_level(static_cast<gpio_num_t>(ETH_RST), 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(static_cast<gpio_num_t>(ETH_RST), 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "W5500 RESET DONE");

    const esp_err_t isr = gpio_install_isr_service(0);
    if (isr != ESP_OK && isr != ESP_ERR_INVALID_STATE) return isr;
    ESP_LOGI(TAG, "GPIO ISR READY");

    spi_bus_config_t bus{};
    bus.miso_io_num = ETH_MISO;
    bus.mosi_io_num = ETH_MOSI;
    bus.sclk_io_num = ETH_SCK;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    ESP_RETURN_ON_ERROR(spi_bus_initialize(ETH_SPI, &bus, SPI_DMA_CH_AUTO), TAG, "SPI init");
    ESP_LOGI(TAG, "SPI READY");

    spi_device_interface_config_t dev{};
    dev.mode = 0;
    dev.clock_speed_hz = ETH_SPI_HZ;
    dev.queue_size = 16;
    dev.spics_io_num = ETH_CS;

    eth_w5500_config_t w5500 = ETH_W5500_DEFAULT_CONFIG(ETH_SPI, &dev);
    w5500.int_gpio_num = ETH_IRQ;

    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    mac_cfg.rx_task_stack_size = 4096;
    auto* mac = esp_eth_mac_new_w5500(&w5500, &mac_cfg);
    if (!mac) return ESP_FAIL;
    ESP_LOGI(TAG, "W5500 MAC DRIVER CREATED");

    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.phy_addr = ETH_PHY_ADDR;
    phy_cfg.reset_gpio_num = -1;
    auto* phy = esp_eth_phy_new_w5500(&phy_cfg);
    if (!phy) return ESP_FAIL;
    ESP_LOGI(TAG, "W5500 PHY CREATED");

    esp_eth_config_t cfg = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_RETURN_ON_ERROR(esp_eth_driver_install(&cfg, &g_eth), TAG, "ETH install");
    ESP_LOGI(TAG, "ETH DRIVER INSTALLED");

    uint8_t mac_addr[6]{};
    ESP_RETURN_ON_ERROR(esp_read_mac(mac_addr, ESP_MAC_ETH), TAG, "MAC read");
    ESP_RETURN_ON_ERROR(esp_eth_ioctl(g_eth, ETH_CMD_S_MAC_ADDR, mac_addr), TAG, "MAC set");
    ESP_LOGI(TAG, "ETH MAC SET %02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    auto* netif = esp_netif_new(&netif_cfg);
    if (!netif) return ESP_ERR_NO_MEM;
    (void)esp_netif_set_hostname(netif, "sp01-diag");
    ESP_RETURN_ON_ERROR(esp_netif_attach(netif, esp_eth_new_netif_glue(g_eth)), TAG, "netif attach");
    ESP_LOGI(TAG, "NETIF ATTACHED");

    ESP_RETURN_ON_ERROR(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event, nullptr), TAG, "ETH event");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip, nullptr), TAG, "IP event");
    ESP_RETURN_ON_ERROR(esp_eth_start(g_eth), TAG, "ETH start");
    ESP_LOGI(TAG, "ETH START CALLED; WAITING LINK/DHCP");
    return ESP_OK;
}
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "BOOT G1 MINIMAL DIAG");
    xTaskCreate(heartbeat, "g1_heartbeat", 2048, nullptr, 1, nullptr);

    const esp_err_t io = probe_tca9554();
    ESP_LOGI(TAG, "I2C/TCA RESULT=%s", esp_err_to_name(io));

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "NETIF INIT FAIL=%s", esp_err_to_name(err));
        return;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "EVENT LOOP FAIL=%s", esp_err_to_name(err));
        return;
    }

    const esp_err_t eth = start_eth();
    ESP_LOGI(TAG, "ETH RESULT=%s", esp_err_to_name(eth));
}
