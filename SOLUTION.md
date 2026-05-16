# Решение проблем сборки и отображения UI на ESP32-S3

## Обзор проекта
- **Плата**: JC4827W543C (ESP32-S3, 4MB flash физически)
- **Дисплей**: NV3041A (480×272, QSPI)
- **Сенсор**: GT911 (I2C)
- **UI фреймворк**: LVGL 9.3 + SquareLine Studio
- **ФО**: ESP-IDF 5.5.2

---

## Изменение 1: Пользовательская таблица разделов в `sdkconfig`

**Проблема**: Стандартные таблицы разделов недостаточны для приложения с LVGL и сложным UI SquareLine Studio (бинарь ~1.9 МБ).

**Решение**: Создана пользовательская таблица разделов `partitions_custom.csv`:

```csv
# Custom partition table for JC4827W543C with 4MB flash
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x3F0000,
```

В `sdkconfig`:
```
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_custom.csv"
```

Это обеспечивает достаточно места для приложения в 4MB flash.

---

## Изменение 2: Увеличение LVGL heap в `sdkconfig`

**Проблема**: LVGL heap в DRAM был слишком мал (64 КБ → 256 КБ) для внутренних структур при рисовании сложного UI, вызывая watchdog timeout в allocator.

**Причина**: LVGL требует память для:
- Внутренних буферов при рисовании слоёв (draw layers)
- Кэша шрифтов
- Временных объектов при обработке событий

**Решение**: Увеличены параметры LVGL памяти:

```
CONFIG_LV_MEM_SIZE_KILOBYTES=256
CONFIG_LV_MEM_POOL_EXPAND_SIZE_KILOBYTES=64
```

Это выделяет 256 КБ + 64 КБ pool expansion для внутренних структур LVGL.

---

## Изменение 3: Выделение буфера рисования из PSRAM в `main.c`

**Проблема**: Буфер рисования (~25 КБ для 1/5 экрана) в статическом DRAM массиве занимал ценное место, оставляя недостаточно памяти для промежуточных буферов LVGL при сложном рисовании.

**Решение**: Буфер выделяется динамически из PSRAM (8 МБ доступно) вместо static массива:

```c
#include "esp_heap_caps.h"

// Было:
static uint8_t __attribute__((aligned(4))) lvgl_draw_buf[LVGL_BUF_PIXELS * 2];

// Стало:
static uint8_t *lvgl_draw_buf = NULL;  // указатель

// В app_main():
lvgl_draw_buf = (uint8_t *)heap_caps_malloc(LVGL_BUF_PIXELS * 2, MALLOC_CAP_SPIRAM);
if (!lvgl_draw_buf) {
    ESP_LOGE(TAG, "Failed to allocate PSRAM for LVGL buffer");
    return;
}
```

---

## Изменение 4: Увеличение стека LVGL задачи в `main.c`

**Проблема**: Stack overflow при инициализации UI и рисовании сложных объектов.

**Причина**: LVGL задача имела только 4096 байт стека, недостаточно для рекурсивного обхода дерева объектов.

**Решение**: Стек увеличен с 4096 на 16384 байта:

```c
// Было:
xTaskCreate(lvgl_task, "lvgl", 4096, NULL, 1, NULL);

// Стало:
xTaskCreate(lvgl_task, "lvgl", 16384, NULL, 1, NULL);
```

---

## Изменение 5: Размер буфера рисования в `main.c`

**Решение**: Буфер установлен на 1/5 экрана для стабильности (даже с PSRAM хватает памяти для промежуточных буферов):

```c
#define LVGL_BUF_PIXELS  (SCREEN_W * SCREEN_H / 5)
```

---

## Изменение 6: Обновление UI компонента для LVGL 9.x

**Проблема**: SquareLine Studio сгенерировал код для LVGL 8.x, использующий deprecated функции, вызывающие ошибки компиляции.

**Решение**: Создан и применён скрипт `fix_lvgl_api.py` для автоматического обновления всех .c файлов в `components/ui/`:

| Старая (8.x) | Новая (9.x) | Файлы |
|--|--|--|
| `lv_anim_set_reverse_duration()` | `lv_anim_set_playback_duration()` | ui.c, ui_Home.c, ui_Settings.c |
| `lv_anim_set_reverse_delay()` | ❌ удалено | ui.c, ui_Home.c, ui_Settings.c |
| `lv_slider_set_start_value()` | `lv_slider_set_left_value()` | ui_Home.c |
| `lv_chart_set_axis_range()` | `lv_chart_set_range()` | ui_Settings.c |
| `lv_chart_set_series_ext_y_array()` | `lv_chart_set_ext_y_array()` | ui_Settings.c |

---

## Финальная конфигурация `main.c`

```c
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

#define SCREEN_W    480
#define SCREEN_H    272
#define LVGL_TICK_MS     2
#define LVGL_BUF_PIXELS  (SCREEN_W * SCREEN_H / 5)

static uint8_t *lvgl_draw_buf = NULL;
static spi_device_handle_t s_lcd_dev;
static lv_display_t       *s_disp;

void app_main(void) {
    // Инициализация NV3041A дисплея...
    // Инициализация GT911 сенсора...
    
    lv_init();
    
    // Выделяем буфер из PSRAM
    lvgl_draw_buf = (uint8_t *)heap_caps_malloc(LVGL_BUF_PIXELS * 2, MALLOC_CAP_SPIRAM);
    if (!lvgl_draw_buf) {
        ESP_LOGE(TAG, "Failed to allocate PSRAM for LVGL buffer");
        return;
    }
    
    s_disp = lv_display_create(SCREEN_W, SCREEN_H);
    lv_display_set_flush_cb(s_disp, lvgl_flush_cb);
    lv_display_set_buffers(s_disp,
                           lvgl_draw_buf, NULL,
                           LVGL_BUF_PIXELS * 2,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    // Инициализация input device (тачскрин)...
    // Инициализация tick timer...
    
    // Инициализация UI
    ui_Home_screen_init();
    ui_Settings_screen_init();
    lv_disp_load_scr(ui_Home);
    
    // LVGL task с увеличенным стеком
    xTaskCreate(lvgl_task, "lvgl", 16384, NULL, 1, NULL);
}
```

---

## Финальная конфигурация `sdkconfig`

```
# Таблица разделов (пользовательская для 4MB flash)
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_custom.csv"

# LVGL память
CONFIG_LV_MEM_SIZE_KILOBYTES=256
CONFIG_LV_MEM_POOL_EXPAND_SIZE_KILOBYTES=64

# Flash size (физический размер платы)
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="4MB"
```

---

## Результат

✅ Проект успешно компилируется (бинарь ~1.9 МБ)  
✅ Устройство загружается без ошибок  
✅ LVGL инициализируется без watchdog timeout или зависаний  
✅ UI из SquareLine Studio полностью функционален  
✅ Сенсорный ввод работает (GT911)  
✅ Нет stack overflow  
✅ Картинка стабильно отображается и обновляется  

**Производительность**: UI достаточно плавный с буфером 1/5 экрана.
