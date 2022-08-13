#pragma once
#include <stdint.h>

#include "ssu.h"
#include "lcd.h"
#include "accel.h"
#include "rtc.h"
#include "portb.h"

#define ROM_end   0xBFFF
#define IO1_start 0xF020
#define IO1_end   0xF100
#define RAM_start 0xF780
#define RAM_end   0xFF80
#define IO2_start 0xFF80
#define IO2_end   0xFFFF

typedef enum {
    MODE_ACTIVE_HIGH,
    MODE_ACTIVE_MED,
    MODE_SUBACTIVE,
    MODE_SLEEP_HIGH,
    MODE_SLEEP_MED,
    MODE_SUBSLEEP,
    MODE_STANDBY,
    MODE_WATCH,
} exec_mode_t;



typedef struct pw_context {
    uint8_t rom[1 << 16];
    uint8_t eeprom_data[1 << 16];
    uint8_t ram[RAM_end - RAM_start];
    uint16_t regs[16];
    uint8_t ccr;
    uint16_t ip;
    uint16_t instr_prefetch;

    uint8_t pdr1;
    uint8_t pdr9;

    // interrupts
    uint8_t iegr;
    uint8_t ienr1;
    uint8_t ienr2;
    uint8_t irr1;
    uint8_t irr2;

    // system
    uint8_t pfcr;
    uint8_t syscr1;
    uint8_t syscr2;
    uint8_t osccr;
    uint8_t ckstpr1;
    uint8_t ckstpr2;

    // Timer B1
    uint8_t tmb1;
    uint8_t tcb1;
    uint8_t tlb1;

    // Timer W
    uint8_t tmrw;
    uint8_t tcrw;
    uint8_t tierw;
    uint8_t tsrw;
    uint8_t tior0;
    uint8_t tior1;
    uint16_t tcnt;
    uint16_t gra;
    uint16_t grb;
    uint16_t grc;
    uint16_t grd;

    uint8_t tmrw_rem;

    uint8_t rdr;
    uint8_t tdr;
    uint8_t smr;
    uint8_t scr;
    uint8_t ssr;
    uint8_t brr;
    uint8_t ircr;
    uint8_t semr;

    ssu_t ssu;
    eeprom_t eeprom;
    lcd_t lcd;
    accel_t accel;
    rtc_t rtc;
    portb_t portb;

    uint16_t prev_ip;
    uint8_t keys_pressed;
    exec_mode_t mode;
    int int_enabled;
    int internal_states;
    int word_access;
    int byte_access;

    int states;

    int i,j,k,l,m,n;
} pw_context_t;