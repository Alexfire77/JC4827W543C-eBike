# JC4827W543C eBike Display — ESP-IDF + LVGL

| Language | Язык |
|----------|------|
| [🇬🇧 English](#english) | [🇷🇺 Русский](#русский) |

---

## English

### Project Description

ESP-IDF firmware project for **JC4827W543C** display board with eBike controller interface:

- **MCU:** ESP32-S3
- **Display:** NV3041A 480×272px, IPS, QSPI
- **Touchscreen:** GT911, I2C
- **UI Framework:** LVGL v9
- **Application:** eBike speed/battery/status display

### Features

✅ LVGL 9 integration with custom display driver  
✅ Capacitive touchscreen support  
✅ PSRAM support for large frame buffers  
✅ DMA-optimized QSPI display updates  
✅ Power-efficient display management  

### Project Structure

```
JC4827W543C_ebike/
├── CMakeLists.txt              # Project config
├── sdkconfig.defaults          # ESP32-S3 defaults (PSRAM, RGB565)
├── main/
│   ├── CMakeLists.txt
│   └── main.c                  # LVGL + display initialization
├── components/
│   ├── nv3041a/                # Display driver (QSPI)
│   ├── gt911/                  # Touchscreen driver (I2C)
│   └── ui/                     # UI components (SquareLine Studio exports)
├── build/                      # Build artifacts
└── partitions_custom.csv       # Custom flash partitioning
```

### Quick Start

#### Prerequisites

- ESP-IDF v5.5+ installed
- ESP-IDF tools (gcc-xtensa, esptool)
- Device drivers for USB-UART

#### Build & Flash

```bash
# Set target
idf.py set-target esp32s3

# Build firmware
idf.py build

# Flash to device (replace COMx with your port)
idf.py -p COMx flash monitor

# Or use unified command
idf.py -p COMx flash monitor
```

Key settings:
- **Chip target:** ESP32-S3
- **Memory:** PSRAM enabled
- **Color depth:** RGB565

---

## Русский

### Описание проекта

Прошивка ESP-IDF для дисплейной платы **JC4827W543C** с интерфейсом контроллера электровелосипеда:

- **МКУ:** ESP32-S3
- **Дисплей:** NV3041A 480×272px, IPS, QSPI
- **Сенсорный экран:** GT911, I2C
- **UI-фреймворк:** LVGL v9
- **Применение:** Отображение скорости, батареи и статуса eBike

### Возможности

✅ Интеграция LVGL 9 с кастомным драйвером дисплея  
✅ Поддержка сенсорного экрана  
✅ Поддержка PSRAM для больших буферов фреймов  
✅ DMA-оптимизированное обновление дисплея QSPI  
✅ Энергоэффективное управление дисплеем  

### Структура проекта

```
JC4827W543C_ebike/
├── CMakeLists.txt              # Конфиг проекта
├── sdkconfig.defaults          # Стандартные настройки ESP32-S3 (PSRAM, RGB565)
├── main/
│   ├── CMakeLists.txt
│   └── main.c                  # Инициализация LVGL и дисплея
├── components/
│   ├── nv3041a/                # Драйвер дисплея (QSPI)
│   ├── gt911/                  # Драйвер сенсорного экрана (I2C)
│   └── ui/                     # UI-компоненты (экспорт SquareLine Studio)
├── build/                      # Артефакты сборки
└── partitions_custom.csv       # Кастомное разбиение флеша
```

### Быстрый старт

#### Требования

- ESP-IDF v5.5+
- ESP-IDF инструменты (gcc-xtensa, esptool)
- Драйверы USB-UART устройства

#### Сборка и прошивка

```bash
# Установить целевой чип
idf.py set-target esp32s3

# Собрать прошивку
idf.py build

# Прошить устройство (замените COMx на ваш порт)
idf.py -p COMx flash monitor

# Или используйте объединенную команду
idf.py -p COMx flash monitor
```

Ключевые настройки:
- **Целевой чип:** ESP32-S3
- **Память:** PSRAM включен
- **Глубина цвета:** RGB565

### Лицензия

MIT License

### Автор

AlexFire

---

### Полезные ссылки / Useful Links

- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
- [LVGL Documentation](https://docs.lvgl.io/)
- [JC4827W543C Board Info](https://www.jczn.tech/)
    SRCS
        "ui.c"
        "ui_Screen1.c"
        "ui_helpers.c"
        "ui_comp_hook.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl
)
```

5. `idf.py build`

## Настройки SquareLine Studio

| Параметр           | Значение          |
|--------------------|-------------------|
| Project type       | ESP-IDF           |
| Resolution         | 480 × 272         |
| Color depth        | 16 bit (RGB565)   |
| LVGL version       | 9.x               |

## Пины (JC4827W543C)

| Сигнал   | GPIO |
|----------|------|
| LCD CS   | 45   |
| LCD SCK  | 47   |
| LCD MOSI | 21   |
| LCD MISO | 48   |
| QUADWP   | 40   |
| QUADHD   | 39   |
| LCD BL   | 1    |
| Touch SDA| 8    |
| Touch SCL| 4    |
| Touch RST| 38   |

