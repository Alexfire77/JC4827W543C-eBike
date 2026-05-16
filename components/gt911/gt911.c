#include "gt911.h"

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static const char *TAG = "gt911";

/* Адреса регистров GT911 */
#define GT_CTRL_REG        0x8040
#define GT_CFGS_REG        0x8047
#define GT_CHECK_REG       0x80FF
#define GT_PID_REG         0x8140
#define GT_GSTID_REG       0x814E
#define GT_READ_XY_REG     0x814E

static i2c_master_bus_handle_t s_i2c_bus   = NULL;
static i2c_master_dev_handle_t s_gt911_dev = NULL;
static gt911_orientation_t     s_orientation = GT911_ORIENT_PORTRAIT;
static int s_rst_pin = -1;

/* -------------------------------------------------------------------------
 * Низкоуровневые I2C-операции
 * ------------------------------------------------------------------------- */

static esp_err_t gt911_write_reg(uint16_t reg, const uint8_t *buf, size_t len)
{
    /* Формат: [REG_HIGH][REG_LOW][DATA...] */
    uint8_t *cmd = (uint8_t *)alloca(2 + len);
    cmd[0] = (uint8_t)(reg >> 8);
    cmd[1] = (uint8_t)(reg & 0xFF);
    memcpy(&cmd[2], buf, len);
    return i2c_master_transmit(s_gt911_dev, cmd, 2 + len, pdMS_TO_TICKS(100));
}

static esp_err_t gt911_read_reg(uint16_t reg, uint8_t *buf, size_t len)
{
    uint8_t reg_addr[2] = {
        (uint8_t)(reg >> 8),
        (uint8_t)(reg & 0xFF),
    };
    return i2c_master_transmit_receive(s_gt911_dev,
                                       reg_addr, sizeof(reg_addr),
                                       buf, len,
                                       pdMS_TO_TICKS(100));
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

esp_err_t gt911_init(const gt911_config_t *config)
{
    esp_err_t ret;
    s_rst_pin = config->rst;

    /* RST пин */
    gpio_config_t rst_conf = {
        .pin_bit_mask = BIT64(config->rst),
        .mode         = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&rst_conf));

    /* INT пин (опционально) */
    if (config->intr >= 0) {
        gpio_config_t int_conf = {
            .pin_bit_mask = BIT64(config->intr),
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&int_conf));
    }

    /* Последовательность аппаратного сброса */
    gpio_set_level(config->rst, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(config->rst, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(config->rst, 1);
    vTaskDelay(pdMS_TO_TICKS(200));

    /* Инициализация I2C Master bus */
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port            = I2C_NUM_0,
        .sda_io_num          = config->sda,
        .scl_io_num          = config->scl,
        .clk_source          = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt   = 7,
        .flags.enable_internal_pullup = false,
    };
    ret = i2c_new_master_bus(&bus_cfg, &s_i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Добавляем GT911 как I2C-устройство */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = GT911_I2C_ADDR,
        .scl_speed_hz    = 400000,
    };
    ret = i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &s_gt911_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_master_bus_add_device failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Мягкая конфигурация: сбросить регистр статуса */
    uint8_t cfg[2] = {0x00, 0x01};
    gt911_write_reg(GT_CHECK_REG, cfg, sizeof(cfg));

    ESP_LOGI(TAG, "GT911 initialized (SDA=%d SCL=%d RST=%d)", config->sda, config->scl, config->rst);
    return ESP_OK;
}

esp_err_t gt911_read_touch(gt911_touch_data_t *data)
{
    memset(data, 0, sizeof(*data));

    uint8_t status;
    esp_err_t ret = gt911_read_reg(GT_READ_XY_REG, &status, 1);
    if (ret != ESP_OK) {
        return ret;
    }

    if ((status & 0x80) == 0) {
        /* Нет новых данных */
        uint8_t clear = 0;
        gt911_write_reg(GT_READ_XY_REG, &clear, 1);
        return ESP_OK;
    }

    data->touch_count = status & 0x0F;
    if (data->touch_count > GT911_MAX_TOUCH) {
        data->touch_count = 0;
        uint8_t clear = 0;
        gt911_write_reg(GT_READ_XY_REG, &clear, 1);
        return ESP_OK;
    }

    if (data->touch_count > 0) {
        uint8_t buf[GT911_MAX_TOUCH * 8];
        ret = gt911_read_reg(GT_READ_XY_REG + 1, buf, data->touch_count * 8);
        if (ret != ESP_OK) {
            return ret;
        }

        for (uint8_t i = 0; i < data->touch_count; i++) {
            uint16_t raw_x = ((uint16_t)buf[2 + i * 8] << 8) | buf[1 + i * 8];
            uint16_t raw_y = ((uint16_t)buf[4 + i * 8] << 8) | buf[3 + i * 8];
            uint16_t tmp;

            switch (s_orientation) {
                case GT911_ORIENT_PORTRAIT:
                    data->x[i] = raw_x;
                    data->y[i] = raw_y;
                    break;
                case GT911_ORIENT_LANDSCAPE:
                    tmp         = raw_x;
                    data->x[i] = GT911_MAX_HEIGHT - raw_y;
                    data->y[i] = tmp;
                    break;
                case GT911_ORIENT_INVERTED_PORTRAIT:
                    data->x[i] = GT911_MAX_WIDTH  - raw_x;
                    data->y[i] = GT911_MAX_HEIGHT - raw_y;
                    break;
                case GT911_ORIENT_INVERTED_LANDSCAPE:
                    tmp         = raw_x;
                    data->x[i] = raw_y;
                    data->y[i] = GT911_MAX_WIDTH - tmp;
                    break;
            }
            data->strength[i] = ((uint16_t)buf[7 + i * 8] << 8) | buf[6 + i * 8];
        }
    }

    /* Сбросить регистр статуса */
    uint8_t clear = 0;
    gt911_write_reg(GT_READ_XY_REG, &clear, 1);

    return ESP_OK;
}

void gt911_set_orientation(gt911_orientation_t orientation)
{
    s_orientation = orientation;
}
