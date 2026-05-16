#pragma once

#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#define GT911_MAX_WIDTH  480
#define GT911_MAX_HEIGHT 272
#define GT911_MAX_TOUCH  5

/** I2C-адрес GT911 (7 бит): 0x5D или 0x14.
 *  На JC4827W543C используется 0x5D (INT=high во время сброса). */
#define GT911_I2C_ADDR   0x5D

typedef enum {
    GT911_ORIENT_PORTRAIT = 0,
    GT911_ORIENT_LANDSCAPE,
    GT911_ORIENT_INVERTED_PORTRAIT,
    GT911_ORIENT_INVERTED_LANDSCAPE,
} gt911_orientation_t;

typedef struct {
    uint8_t  touch_count;
    uint16_t x[GT911_MAX_TOUCH];
    uint16_t y[GT911_MAX_TOUCH];
    uint16_t strength[GT911_MAX_TOUCH];
} gt911_touch_data_t;

/** Конфигурация GT911.
 *  JC4827W543C: SDA=8, SCL=4, RST=38, INTR=-1 (не подключён) */
typedef struct {
    int sda;   /*!< GPIO пин I2C SDA */
    int scl;   /*!< GPIO пин I2C SCL */
    int rst;   /*!< GPIO пин сброса */
    int intr;  /*!< GPIO пин прерывания (-1 если не используется) */
} gt911_config_t;

/**
 * @brief Инициализация GT911
 */
esp_err_t gt911_init(const gt911_config_t *config);

/**
 * @brief Считать состояние касания
 *
 * @param data  Выходная структура с координатами касания
 * @return ESP_OK если данные считаны успешно
 */
esp_err_t gt911_read_touch(gt911_touch_data_t *data);

/**
 * @brief Установить ориентацию (влияет на пересчёт координат)
 */
void gt911_set_orientation(gt911_orientation_t orientation);
