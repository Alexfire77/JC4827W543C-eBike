#pragma once

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"

/**
 * Конфигурация дисплея NV3041A (QSPI-интерфейс)
 *
 * Плата JC4827W543C:
 *   CS=45, SCK=47, MOSI=21, MISO=48, QUADWP=40, QUADHD=39, BL=1
 */
typedef struct {
    int cs;       /*!< Chip Select */
    int sclk;     /*!< Clock */
    int mosi;     /*!< MOSI (D0) */
    int miso;     /*!< MISO (D1) */
    int quadwp;   /*!< D2 */
    int quadhd;   /*!< D3 */
    int bl;       /*!< Backlight (-1 if not used) */
    int width;    /*!< Display width in pixels */
    int height;   /*!< Display height in pixels */
} nv3041a_config_t;

/**
 * @brief Инициализация NV3041A и SPI-шины
 *
 * @param config  Конфигурация дисплея
 * @param out_dev Указатель для сохранения дескриптора SPI-устройства
 * @return ESP_OK при успехе
 */
esp_err_t nv3041a_init(const nv3041a_config_t *config, spi_device_handle_t *out_dev);

/**
 * @brief Установить окно вывода (CASET/RASET/RAMWR)
 */
void nv3041a_set_window(spi_device_handle_t dev, int x1, int y1, int x2, int y2);

/**
 * @brief Передать пиксельные данные в режиме QSPI
 *
 * @param dev       Дескриптор SPI-устройства
 * @param data      Указатель на буфер пикселей (RGB565, big-endian после swap)
 * @param len_bytes Размер буфера в байтах
 */
void nv3041a_flush_pixels(spi_device_handle_t dev, const void *data, size_t len_bytes);
