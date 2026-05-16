#include "nv3041a.h"

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static const char *TAG = "nv3041a";

/* Глобальное хранение CS-пина для ручного управления */
static int s_cs_pin = -1;

static inline void cs_low(void)  { gpio_set_level(s_cs_pin, 0); }
static inline void cs_high(void) { gpio_set_level(s_cs_pin, 1); }

/* -------------------------------------------------------------------------
 * Протокол QSPI NV3041A:
 *   Команды/параметры (single SPI):
 *     CMD=0x02,  ADDR=(reg<<8) в 24-битном поле (MSB first),  DATA=1..N байт
 *   Пиксельные данные (Quad SPI):
 *     CMD=0x32,  ADDR=0x003C00,  DATA=пиксели в режиме QIO
 *     Последующие чанки: флаги VARIABLE_CMD/ADDR/DUMMY (без повтора заголовка)
 * -------------------------------------------------------------------------
 * ------------------------------------------------------------------------- */

/* Команда + 1 байт данных (single SPI) */
static void write_reg(spi_device_handle_t dev, uint8_t reg, uint8_t val)
{
    spi_transaction_ext_t t = { .base = {
        .flags  = SPI_TRANS_USE_TXDATA |
                  SPI_TRANS_MULTILINE_CMD |
                  SPI_TRANS_MULTILINE_ADDR,
        .cmd    = 0x02,
        .addr   = (uint32_t)reg << 8,
        .length = 8,
        .tx_data = { val },
    }};
    cs_low();
    spi_device_polling_transmit(dev, (spi_transaction_t *)&t);
    cs_high();
}

/* Команда + 4 байта данных (single SPI): используется для CASET/RASET */
static void write_reg4(spi_device_handle_t dev, uint8_t reg,
                       uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3)
{
    spi_transaction_ext_t t = { .base = {
        .flags  = SPI_TRANS_USE_TXDATA |
                  SPI_TRANS_MULTILINE_CMD |
                  SPI_TRANS_MULTILINE_ADDR,
        .cmd    = 0x02,
        .addr   = (uint32_t)reg << 8,
        .length = 32,
        .tx_data = { d0, d1, d2, d3 },
    }};
    cs_low();
    spi_device_polling_transmit(dev, (spi_transaction_t *)&t);
    cs_high();
}

/* Команда без данных (single SPI) */
static void write_cmd(spi_device_handle_t dev, uint8_t cmd)
{
    spi_transaction_ext_t t = { .base = {
        .flags  = SPI_TRANS_MULTILINE_CMD |
                  SPI_TRANS_MULTILINE_ADDR,
        .cmd    = 0x02,
        .addr   = (uint32_t)cmd << 8,
        .length = 0,
    }};
    cs_low();
    spi_device_polling_transmit(dev, (spi_transaction_t *)&t);
    cs_high();
}

/* -------------------------------------------------------------------------
 * Последовательность инициализации NV3041A
 * ------------------------------------------------------------------------- */
static void nv3041a_init_seq(spi_device_handle_t dev)
{
    write_reg(dev, 0xff, 0xa5);
    write_reg(dev, 0x36, 0xc0);
    write_reg(dev, 0x3A, 0x01);  /* RGB565 */
    write_reg(dev, 0x41, 0x03);
    write_reg(dev, 0x44, 0x15);
    write_reg(dev, 0x45, 0x15);
    write_reg(dev, 0x7d, 0x03);
    write_reg(dev, 0xc1, 0xbb);
    write_reg(dev, 0xc2, 0x05);
    write_reg(dev, 0xc3, 0x10);
    write_reg(dev, 0xc6, 0x3e);
    write_reg(dev, 0xc7, 0x25);
    write_reg(dev, 0xc8, 0x11);
    write_reg(dev, 0x7a, 0x5f);
    write_reg(dev, 0x6f, 0x44);
    write_reg(dev, 0x78, 0x70);
    write_reg(dev, 0xc9, 0x00);
    write_reg(dev, 0x67, 0x21);
    /* gate */
    write_reg(dev, 0x51, 0x0a);
    write_reg(dev, 0x52, 0x76);
    write_reg(dev, 0x53, 0x0a);
    write_reg(dev, 0x54, 0x76);
    /* source */
    write_reg(dev, 0x46, 0x0a);
    write_reg(dev, 0x47, 0x2a);
    write_reg(dev, 0x48, 0x0a);
    write_reg(dev, 0x49, 0x1a);
    write_reg(dev, 0x56, 0x43);
    write_reg(dev, 0x57, 0x42);
    write_reg(dev, 0x58, 0x3c);
    write_reg(dev, 0x59, 0x64);
    write_reg(dev, 0x5a, 0x41);
    write_reg(dev, 0x5b, 0x3c);
    write_reg(dev, 0x5c, 0x02);
    write_reg(dev, 0x5d, 0x3c);
    write_reg(dev, 0x5e, 0x1f);
    write_reg(dev, 0x60, 0x80);
    write_reg(dev, 0x61, 0x3f);
    write_reg(dev, 0x62, 0x21);
    write_reg(dev, 0x63, 0x07);
    write_reg(dev, 0x64, 0xe0);
    write_reg(dev, 0x65, 0x02);
    /* mux */
    write_reg(dev, 0xca, 0x20);
    write_reg(dev, 0xcb, 0x52);
    write_reg(dev, 0xcc, 0x10);
    write_reg(dev, 0xcD, 0x42);
    write_reg(dev, 0xD0, 0x20);
    write_reg(dev, 0xD1, 0x52);
    write_reg(dev, 0xD2, 0x10);
    write_reg(dev, 0xD3, 0x42);
    write_reg(dev, 0xD4, 0x0a);
    write_reg(dev, 0xD5, 0x32);
    /* gamma */
    write_reg(dev, 0x80, 0x00); write_reg(dev, 0xA0, 0x00);
    write_reg(dev, 0x81, 0x07); write_reg(dev, 0xA1, 0x06);
    write_reg(dev, 0x82, 0x02); write_reg(dev, 0xA2, 0x01);
    write_reg(dev, 0x86, 0x11); write_reg(dev, 0xA6, 0x10);
    write_reg(dev, 0x87, 0x27); write_reg(dev, 0xA7, 0x27);
    write_reg(dev, 0x83, 0x37); write_reg(dev, 0xA3, 0x37);
    write_reg(dev, 0x84, 0x35); write_reg(dev, 0xA4, 0x35);
    write_reg(dev, 0x85, 0x3f); write_reg(dev, 0xA5, 0x3f);
    write_reg(dev, 0x88, 0x0b); write_reg(dev, 0xA8, 0x0b);
    write_reg(dev, 0x89, 0x14); write_reg(dev, 0xA9, 0x14);
    write_reg(dev, 0x8a, 0x1a); write_reg(dev, 0xAa, 0x1a);
    write_reg(dev, 0x8b, 0x0a); write_reg(dev, 0xAb, 0x0a);
    write_reg(dev, 0x8c, 0x14); write_reg(dev, 0xAc, 0x08);
    write_reg(dev, 0x8d, 0x17); write_reg(dev, 0xAd, 0x07);
    write_reg(dev, 0x8e, 0x16); write_reg(dev, 0xAe, 0x06);
    write_reg(dev, 0x8f, 0x1B); write_reg(dev, 0xAf, 0x07);
    write_reg(dev, 0x90, 0x04); write_reg(dev, 0xB0, 0x04);
    write_reg(dev, 0x91, 0x0A); write_reg(dev, 0xB1, 0x0A);
    write_reg(dev, 0x92, 0x16); write_reg(dev, 0xB2, 0x15);
    write_reg(dev, 0xff, 0x00);
    write_reg(dev, 0x21, 0x00);  /* Display Inversion On (IPS panel) */
    write_reg(dev, 0x11, 0x00);  /* Sleep out */
    vTaskDelay(pdMS_TO_TICKS(120));
    write_reg(dev, 0x29, 0x00);  /* Display on */
    vTaskDelay(pdMS_TO_TICKS(100));
}

/* =========================================================================
 * Public API
 * ========================================================================= */

esp_err_t nv3041a_init(const nv3041a_config_t *config, spi_device_handle_t *out_dev)
{
    esp_err_t ret;
    s_cs_pin = config->cs;

    /* CS pin */
    gpio_config_t cs_conf = {
        .pin_bit_mask = BIT64(config->cs),
        .mode         = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&cs_conf));
    gpio_set_level(config->cs, 1);

    /* Backlight pin (выключаем на время инициализации) */
    if (config->bl >= 0) {
        gpio_config_t bl_conf = {
            .pin_bit_mask = BIT64(config->bl),
            .mode         = GPIO_MODE_OUTPUT,
        };
        ESP_ERROR_CHECK(gpio_config(&bl_conf));
        gpio_set_level(config->bl, 0);
    }

    /* SPI-шина */
    spi_bus_config_t buscfg = {
        .mosi_io_num     = config->mosi,
        .miso_io_num     = config->miso,
        .sclk_io_num     = config->sclk,
        .quadwp_io_num   = config->quadwp,
        .quadhd_io_num   = config->quadhd,
        .max_transfer_sz = (size_t)(config->width * config->height * 2),
        .flags           = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS,
    };
    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* SPI-устройство: command=8 бит, address=24 бита, CS ручной */
    spi_device_interface_config_t devcfg = {
        .command_bits     = 8,
        .address_bits     = 24,
        .dummy_bits       = 0,
        .mode             = 0,
        .clock_speed_hz   = 24 * 1000 * 1000,
        .spics_io_num     = -1,  /* CS управляется вручную */
        .flags            = SPI_DEVICE_HALFDUPLEX,
        .queue_size       = 1,
    };
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, out_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Захватываем шину для эксклюзивного доступа */
    spi_device_acquire_bus(*out_dev, portMAX_DELAY);

    /* Инициализация контроллера дисплея */
    nv3041a_init_seq(*out_dev);

    /* Включаем подсветку */
    if (config->bl >= 0) {
        gpio_set_level(config->bl, 1);
    }

    ESP_LOGI(TAG, "NV3041A initialized (%dx%d)", config->width, config->height);
    return ESP_OK;
}

void nv3041a_set_window(spi_device_handle_t dev, int x1, int y1, int x2, int y2)
{
    /* CASET (0x2A): X start/end */
    write_reg4(dev, 0x2A,
               (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF),
               (uint8_t)(x2 >> 8), (uint8_t)(x2 & 0xFF));
    /* RASET (0x2B): Y start/end */
    write_reg4(dev, 0x2B,
               (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF),
               (uint8_t)(y2 >> 8), (uint8_t)(y2 & 0xFF));
    /* RAMWR (0x2C): начало записи в RAM */
    write_cmd(dev, 0x2C);
}

void nv3041a_flush_pixels(spi_device_handle_t dev, const void *data, size_t len_bytes)
{
    /*
     * Пиксельные данные передаются в режиме Quad SPI:
     *   Первый чанк:      CMD=0x32, ADDR=0x003C00, флаг QIO
     *   Следующие чанки:  без CMD/ADDR (VARIABLE_CMD|VARIABLE_ADDR|VARIABLE_DUMMY)
     */
    const uint8_t *ptr = (const uint8_t *)data;
    size_t remaining   = len_bytes;
    bool first         = true;

    cs_low();
    while (remaining > 0) {
        size_t chunk = (remaining > 4096) ? 4096 : remaining;

        spi_transaction_ext_t t = {0};
        if (first) {
            t.base.flags = SPI_TRANS_MODE_QIO;
            t.base.cmd   = 0x32;
            t.base.addr  = 0x003C00;
            first        = false;
        } else {
            t.base.flags    = SPI_TRANS_MODE_QIO |
                              SPI_TRANS_VARIABLE_CMD |
                              SPI_TRANS_VARIABLE_ADDR |
                              SPI_TRANS_VARIABLE_DUMMY;
            t.command_bits  = 0;
            t.address_bits  = 0;
            t.dummy_bits    = 0;
        }
        t.base.tx_buffer = ptr;
        t.base.length    = chunk * 8;

        spi_device_polling_transmit(dev, (spi_transaction_t *)&t);

        ptr       += chunk;
        remaining -= chunk;
    }
    cs_high();
}
