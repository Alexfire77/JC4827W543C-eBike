#!/usr/bin/env python3
"""
Автоматическое исправление deprecated LVGL API в сгенерированных файлах SquareLine
"""

import os
import re
from pathlib import Path

UI_DIR = Path("components/ui")

def fix_lvgl_api(file_path):
    """Исправить deprecated LVGL функции в файле"""
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    original_content = content
    
    # Замена 1: lv_anim_set_reverse_duration → lv_anim_set_playback_duration
    content = re.sub(
        r'lv_anim_set_reverse_duration\(',
        'lv_anim_set_playback_duration(',
        content
    )
    
    # Замена 2: Удаление lv_anim_set_reverse_delay (вся строка с отступом)
    content = re.sub(
        r'\s+lv_anim_set_reverse_delay\([^)]+\);\n',
        '\n',
        content
    )
    
    # Замена 3: lv_slider_set_start_value → lv_slider_set_left_value
    content = re.sub(
        r'lv_slider_set_start_value\(',
        'lv_slider_set_left_value(',
        content
    )
    
    # Замена 4: lv_chart_set_axis_range → lv_chart_set_range
    content = re.sub(
        r'lv_chart_set_axis_range\(',
        'lv_chart_set_range(',
        content
    )
    
    # Замена 5: lv_chart_set_series_ext_y_array → lv_chart_set_ext_y_array
    content = re.sub(
        r'lv_chart_set_series_ext_y_array\(',
        'lv_chart_set_ext_y_array(',
        content
    )
    
    if content != original_content:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"✓ Исправлен: {file_path}")
        return True
    return False

def main():
    if not UI_DIR.exists():
        print(f"❌ Папка {UI_DIR} не найдена")
        return
    
    c_files = list(UI_DIR.glob("*.c"))
    print(f"Найдено {len(c_files)} .c файлов в {UI_DIR}")
    
    fixed_count = 0
    for c_file in sorted(c_files):
        if fix_lvgl_api(c_file):
            fixed_count += 1
    
    print(f"\n✅ Исправлено файлов: {fixed_count}/{len(c_files)}")

if __name__ == "__main__":
    main()
