#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "lvgl.h"
#include "ui/ui.h"
#include "nv3041a.h"
#include "gt911.h"

static const char *TAG = "main";

/* =========================================================================
 * Конфигурация пинов — плата JC4827W543C (ESP32-S3)
 * ========================================================================= */

/* Дисплей NV3041A (QSPI) */
#define LCD_CS      GPIO_NUM_45
#define LCD_SCK     GPIO_NUM_47
#define LCD_MOSI    GPIO_NUM_21
#define LCD_MISO    GPIO_NUM_48
#define LCD_QUADWP  GPIO_NUM_40
#define LCD_QUADHD  GPIO_NUM_39
#define LCD_BL      GPIO_NUM_1

/* Тачскрин GT911 (I2C) */
#define TOUCH_SDA   GPIO_NUM_8
#define TOUCH_SCL   GPIO_NUM_4
#define TOUCH_RST   GPIO_NUM_38
#define TOUCH_INTR  -1  /* не подключён */

/* Разрешение дисплея */
#define SCREEN_W    480
#define SCREEN_H    272

/* =========================================================================
 * LVGL
 * ========================================================================= */
#define LVGL_TICK_MS     2

/* 1/5 экрана — стабильный размер для сложного UI */
#define LVGL_BUF_PIXELS  (SCREEN_W * SCREEN_H / 5)

/* Буфер в PSRAM для большего объёма */
static uint8_t *lvgl_draw_buf = NULL;

static spi_device_handle_t s_lcd_dev;
static lv_display_t       *s_disp;

/* =========================================================================
 * Callback сброса LVGL → дисплей
 * ========================================================================= */
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *pixel_map)
{
    int32_t x1 = area->x1;
    int32_t y1 = area->y1;
    int32_t x2 = area->x2;
    int32_t y2 = area->y2;

    size_t pixel_count = (size_t)((x2 - x1 + 1) * (y2 - y1 + 1));

    /* LVGL хранит RGB565 в little-endian, NV3041A ждёт big-endian — меняем байты */
    uint16_t *pixels = (uint16_t *)pixel_map;
    for (size_t i = 0; i < pixel_count; i++) {
        pixels[i] = (pixels[i] >> 8) | (pixels[i] << 8);
    }

    nv3041a_set_window(s_lcd_dev, x1, y1, x2, y2);
    nv3041a_flush_pixels(s_lcd_dev, pixel_map, pixel_count * 2);
    lv_display_flush_ready(disp);
}

/* =========================================================================
 * Callback чтения тачскрина
 * ========================================================================= */
static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    gt911_touch_data_t td;
    if (gt911_read_touch(&td) == ESP_OK && td.touch_count > 0) {
        data->state   = LV_INDEV_STATE_PRESSED;
        data->point.x = td.x[0];
        data->point.y = td.y[0];
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/* =========================================================================
 * LVGL tick timer
 * ========================================================================= */
static void lvgl_tick_timer_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LVGL_TICK_MS);
}

/* =========================================================================
 * LVGL task
 * ========================================================================= */
static void lvgl_task(void *arg)
{
    (void)arg;
    while (true) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

/* =========================================================================
 * app_main
 * ========================================================================= */
void app_main(void)
{
    /* --- Дисплей NV3041A --- */
    const nv3041a_config_t lcd_cfg = {
        .cs     = LCD_CS,
        .sclk   = LCD_SCK,
        .mosi   = LCD_MOSI,
        .miso   = LCD_MISO,
        .quadwp = LCD_QUADWP,
        .quadhd = LCD_QUADHD,
        .bl     = LCD_BL,
        .width  = SCREEN_W,
        .height = SCREEN_H,
    };
    ESP_ERROR_CHECK(nv3041a_init(&lcd_cfg, &s_lcd_dev));

    /* --- Тачскрин GT911 --- */
    const gt911_config_t touch_cfg = {
        .sda  = TOUCH_SDA,
        .scl  = TOUCH_SCL,
        .rst  = TOUCH_RST,
        .intr = TOUCH_INTR,
    };
    ESP_ERROR_CHECK(gt911_init(&touch_cfg));
    gt911_set_orientation(GT911_ORIENT_PORTRAIT);

    /* --- LVGL --- */
    lv_init();

    /* Выделяем буфер рисования из PSRAM */
    lvgl_draw_buf = (uint8_t *)heap_caps_malloc(LVGL_BUF_PIXELS * 2, MALLOC_CAP_SPIRAM);
    if (!lvgl_draw_buf) {
        ESP_LOGE(TAG, "Failed to allocate PSRAM for LVGL buffer");
        return;
    }
    ESP_LOGI(TAG, "LVGL buffer allocated from PSRAM: %d bytes", LVGL_BUF_PIXELS * 2);

    s_disp = lv_display_create(SCREEN_W, SCREEN_H);
    lv_display_set_flush_cb(s_disp, lvgl_flush_cb);
    lv_display_set_buffers(s_disp,
                           lvgl_draw_buf, NULL,
                           LVGL_BUF_PIXELS * 2,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_read_cb);

    const esp_timer_create_args_t tick_args = {
        .callback = lvgl_tick_timer_cb,
        .name     = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, LVGL_TICK_MS * 1000));

    /* --- Инициализация UI (заменить компонент ui/ на экспорт SquareLine Studio) --- */
    
    /* ДИАГНОСТИКА: Пошагово включаем UI */
    ESP_LOGI(TAG, "UI: Creating event ID...");
    LV_EVENT_GET_COMP_CHILD = lv_event_register_id();
    
    ESP_LOGI(TAG, "UI: Setting theme...");
    lv_disp_t * dispp = lv_display_get_default();
    lv_theme_t * theme = lv_theme_simple_init(dispp);
    lv_disp_set_theme(dispp, theme);
    
    ESP_LOGI(TAG, "UI: Initializing Home screen...");
    ui_Home_screen_init();
    ESP_LOGI(TAG, "UI: Home screen OK");
    
    ESP_LOGI(TAG, "UI: Initializing Settings screen...");
    ui_Settings_screen_init();
    ESP_LOGI(TAG, "UI: Settings screen OK");
    
    ESP_LOGI(TAG, "UI: Loading Home screen...");
    lv_disp_load_scr(ui_Home);
    ESP_LOGI(TAG, "UI: Loaded!");

    /* --- LVGL task --- */
    xTaskCreate(lvgl_task, "lvgl", 16384, NULL, 1, NULL);

    ESP_LOGI(TAG, "JC4827W543C started");
}
