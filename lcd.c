#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "lcd.h"

typedef struct lcd_cmd {
#ifdef LCD_DEBUG
    char *name;
#endif
    uint8_t length;
    void (*handler)(lcd_t *lcd, uint8_t *buf);
} lcd_command_t;

#ifdef LCD_DEBUG
#define LCD_CMD(name, length, handler) { name, length, handler }
#else
#define LCD_CMD(name, length, handler) { length, handler }
#endif

/* COMMAND HANDLERS */

static void set_lo_col_addr(lcd_t *lcd, uint8_t *buf) {
    lcd->column_address &= 0xE0;
    lcd->column_address |= (buf[0] & 0xF) << 1;
}

static void set_hi_col_addr(lcd_t *lcd, uint8_t *buf) {
    lcd->column_address &= 0x1E;
    lcd->column_address |= (buf[0] & 7) << 5;
}

static void set_page_addr(lcd_t *lcd, uint8_t *buf) {
    // this is different from the SSD1854 spec
    lcd->page_address = buf[0] & 0xF;
}

static void set_disp_start_line(lcd_t *lcd, uint8_t *buf) {
    uint8_t new_start = buf[1];
    if (new_start != lcd->display_start_line) {
        *(lcd->should_redraw) = 1;
    }
    lcd->display_start_line = new_start;
}

static void set_contrast(lcd_t *lcd, uint8_t *buf) {
    lcd->contrast = buf[1] & 0x3F;
}

static void set_mux_ratio(lcd_t *lcd, uint8_t *buf) {
    lcd->mux_ratio = buf[1];
}

static void set_segment_remap(lcd_t *lcd, uint8_t *buf) {
    lcd->segment_remap = buf[0] & 1;
}

static void lcd_reset(lcd_t *lcd, uint8_t *buf) {
    lcd->page_address            = 0;
    lcd->column_address          = 0;
    lcd->display_start_line      = 0;
    lcd->contrast                = 0x20;
}


/* END COMMAND HANDLERS */

static lcd_command_t LCD_COMMANDS[] = {
/* 00 */ LCD_CMD( "Set Lower Column Address",               1, set_lo_col_addr ),
/* 01 */ LCD_CMD( "Set Upper Column Address",               1, set_hi_col_addr ),
/* 02 */ LCD_CMD( "Set Master/Slave Mode",                  1, NULL ),
/* 03 */ LCD_CMD( "Reserved 1A~1F",                         0, NULL ),
/* 04 */ LCD_CMD( "Set Internal Regulator Resistor Ratio",  1, NULL ),
/* 05 */ LCD_CMD( "Set Power Control Register",             1, NULL ),
/* 06 */ LCD_CMD( "Reserved 30~3F",                         0, NULL ),
/* 07 */ LCD_CMD( "Set Display Start Line",                 2, set_disp_start_line ),
/* 08 */ LCD_CMD( "Set Display Offset",                     2, NULL ),
/* 09 */ LCD_CMD( "Set Multiplex Ratio",                    2, set_mux_ratio ),
/* 0A */ LCD_CMD( "Set N-line Inversion",                   2, NULL ),
/* 0B */ LCD_CMD( "Set LCD Bias",                           1, NULL ),
/* 0C */ LCD_CMD( "Reserved 58~5F",                         0, NULL ),
/* 0D */ LCD_CMD( "Set Upper Window Corner ax",             2, NULL ),
/* 0E */ LCD_CMD( "Set Upper Window Corner ay",             2, NULL ),
/* 0F */ LCD_CMD( "Set Lower Window Corner bx",             2, NULL ),
/* 10 */ LCD_CMD( "Set Lower Window Corner by",             2, NULL ),
/* 11 */ LCD_CMD( "Set DC-DC Converter Factor",             1, NULL ),
/* 12 */ LCD_CMD( "Reserved 68~80",                         0, NULL ),
/* 13 */ LCD_CMD( "Set Contrast Control Register",          2, set_contrast ),
/* 14 */ LCD_CMD( "Reserved 82~87",                         0, NULL ),
/* 15 */ LCD_CMD( "Set White Mode, Frame 2nd & 1st",        2, NULL ),
/* 16 */ LCD_CMD( "Set White Mode, Frame 4th & 3rd",        2, NULL ),
/* 17 */ LCD_CMD( "Set Light Gray Mode, Frame 2nd & 1st",   2, NULL ),
/* 18 */ LCD_CMD( "Set Light Gray Mode, Frame 4th & 3rd",   2, NULL ),
/* 19 */ LCD_CMD( "Set Dark Gray Mode, Frame 2nd & 1st",    2, NULL ),
/* 1A */ LCD_CMD( "Set Dark Gray Mode, Frame 4th & 3rd",    2, NULL ),
/* 1B */ LCD_CMD( "Set Black Mode, Frame 2nd & 1st",        2, NULL ),
/* 1C */ LCD_CMD( "Set Black Mode, Frame 4th & 3rd",        2, NULL ),
/* 1D */ LCD_CMD( "Set PWM and FRC",                        1, NULL ),
/* 1E */ LCD_CMD( "Reserved 98~9F",                         0, NULL ),
/* 1F */ LCD_CMD( "Set Segment Remap",                      1, set_segment_remap ),
/* 20 */ LCD_CMD( "Reserved A2~A3",                         0, NULL ),
/* 21 */ LCD_CMD( "Set Entire Display On/Off",              1, NULL ),
/* 22 */ LCD_CMD( "Set Normal/Reverse Display",             1, NULL ),
/* 23 */ LCD_CMD( "Reserved A8",                            0, NULL ),
/* 24 */ LCD_CMD( "Set Power Save Mode",                    1, NULL ),
/* 25 */ LCD_CMD( "Reserved A4",                            0, NULL ),
/* 26 */ LCD_CMD( "Start Internal Oscillator",              1, NULL ),
/* 27 */ LCD_CMD( "Unknown AC~AD",                          0, NULL ),
/* 28 */ LCD_CMD( "Set Display On/Off",                     1, NULL ),
/* 29 */ LCD_CMD( "Set Page Address",                       1, set_page_addr ),
/* 2A */ LCD_CMD( "Set COM Output Scan Direction",          1, NULL ),
/* 2B */ LCD_CMD( "Reserved D0~E0",                         0, NULL ),
/* 2C */ LCD_CMD( "Exit Power-save Mode",                   1, NULL ),
/* 2D */ LCD_CMD( "Software Reset",                         1, lcd_reset ),
/* 2E */ LCD_CMD( "Reserved E3",                            0, NULL ),
/* 2F */ LCD_CMD( "Exit N-line Inversion",                  1, NULL ),
/* 30 */ LCD_CMD( "Reserved E5",                            0, NULL ),
/* 31 */ LCD_CMD( "Enable Scroll Buffer RAM",               1, NULL ),
/* 32 */ LCD_CMD( "Set Display Data Length",                2, NULL ),
/* 33 */ LCD_CMD( "Set TC value",                           2, NULL ),
/* 34 */ LCD_CMD( "Reserved EA~EF",                         0, NULL ),
/* 35 */ LCD_CMD( "Extended Features",                      1, NULL ),
};

static uint8_t LCD_CMD_MAP[] = {
/*         0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F  */
/* 0 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/* 1 */ 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
/* 2 */ 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
/* 3 */ 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
/* 4 */ 0x07, 0x07, 0x07, 0x07, 0x08, 0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x0A, 0x0A, 0x0A, 0x0A,
/* 5 */ 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
/* 6 */ 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x11, 0x11, 0x11, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
/* 7 */ 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
/* 8 */ 0x12, 0x13, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
/* 9 */ 0x1D, 0x1D, 0x1D, 0x1D, 0x1D, 0x1D, 0x1D, 0x1D, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E,
/* A */ 0x1F, 0x1F, 0x20, 0x20, 0x21, 0x21, 0x22, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x27, 0x28, 0x28,
/* B */ 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29,
/* C */ 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A,
/* D */ 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B,
/* E */ 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x31, 0x32, 0x33, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34,
/* F */ 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35,
};

void lcd_init(lcd_t *lcd, int *should_redraw) {
    memset(lcd, 0, sizeof(lcd_t));

    lcd->mux_ratio             = 0xA0;
    lcd->contrast              = 0x20;
    lcd->lower_window_corner_x = 127;
    lcd->lower_window_corner_y = 159;

    lcd->buf_off = 0;
    lcd->should_redraw = should_redraw;
}

void lcd_cmd(lcd_t *lcd, uint8_t byte) {
    //printf("LCD WRITE %x\n", byte);
    lcd->buf[lcd->buf_off++] = byte;
    lcd_command_t *cmd = &LCD_COMMANDS[LCD_CMD_MAP[lcd->buf[0]]];
    if (lcd->buf_off >= cmd->length) {
#ifdef LCD_DEBUG
        if (1 || cmd->handler == NULL) {
            printf("LCD CMD: ");
            for (int i = 0; i < lcd->buf_off; i++) {
                printf("%02x ", lcd->buf[i]);
            }
            for (int i = lcd->buf_off; i < 2; i++) {
                printf("   ");
            }
            printf("(%s)\n", cmd->name);
        }
#endif
        if (cmd->handler != NULL) {
            cmd->handler(lcd, lcd->buf);
        }
        lcd->buf_off = 0;
    }
}

void lcd_data(lcd_t *lcd, uint8_t byte) {
    lcd->ram[lcd->page_address][lcd->column_address++] = byte;
    lcd->column_address &= 0xFF;
}