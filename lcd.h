#pragma once
#include <stdint.h>

#define LCD_PAGE_SIZE 0xFF
#define LCD_NUM_COLS  0x7F
#define LCD_NUM_PAGES 21
#define LCD_RAM_SIZE (LCD_PAGE_SIZE * LCD_NUM_PAGES)

typedef struct lcd {
    uint8_t page_address;
    uint8_t column_address;
    uint8_t display_on;
    uint8_t display_start_line;
    uint8_t display_offset;
    uint8_t mux_ratio;
    uint8_t entire_display_on;
    uint8_t contrast;
    uint8_t segment_remap;
    uint8_t upper_window_corner_x;
    uint8_t upper_window_corner_y;
    uint8_t lower_window_corner_x;
    uint8_t lower_window_corner_y;

    uint8_t buf[2];
    uint8_t buf_off;
    uint8_t ram[LCD_NUM_PAGES][LCD_RAM_SIZE];
    int *should_redraw;
} lcd_t;

void lcd_init(lcd_t *lcd, int *redraw);

void lcd_cmd(lcd_t *lcd, uint8_t byte);

void lcd_data(lcd_t *lcd, uint8_t byte);

void lcd_stop(lcd_t *lcd);