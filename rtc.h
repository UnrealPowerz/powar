#pragma once
#include <stdint.h>
#include <time.h>

#define RTC_FLG_ADDR    0xF067
#define RTC_SECDR_ADDR 0xF068
#define RTC_MINDR_ADDR 0xF069
#define RTC_HRDR_ADDR  0xF06A
#define RTC_CR1_ADDR    0xF06C
#define RTC_CR2_ADDR    0xF06D

// RSECDR
// Second data register/free running counter data register
#define RSECDR_MASK     0x7F
#define RSECDR_BSY_BIT     7
#define RSECDR_TEN_MASK 0x70
#define RSECDR_TEN_SHIFT   4
#define RSECDR_ONE_MASK  0xF
#define RSECDR_ONE_SHIFT   0

// RMINDR
// Minute data register
#define RMINDR_MASK     0x7F
#define RMINDR_BSY_BIT     7
#define RMINDR_TEN_MASK 0x70
#define RMINDR_TEN_SHIFT   4
#define RMINDR_ONE_MASK  0xF
#define RMINDR_ONE_SHIFT   0

// RHRDR
// Hour data register
#define RHRDR_MASK      0x3F
#define RHRDR_BSY_BIT      7
#define RHRDR_TEN_MASK 0x30
#define RHRDR_TEN_SHIFT   4
#define RHRDR_ONE_MASK  0xF
#define RHRDR_ONE_SHIFT   0

// RWKDR
// Day-of-week data register
#define RWKDR_MASK       0x7
#define RWKDR_DAY_MASK     7
#define RWKDR_DAY_SHIFT    0

// RTCCR1
// RTC control register 1
#define RTCCR1_MASK     0xF8
#define RTCCR1_RUN_BIT     7
#define RTCCR1_12_24_BIT   6
#define RTCCR1_PM_BIT      5
#define RTCCR1_RST_BIT     4
#define RTCCR1_INT_BIT     3

// RTCCR2
// RTC control register 2
#define RTCCR2_MASK     0xFF
#define RTCCR2_FOIE_BIT    7
#define RTCCR2_WKIE_BIT    6
#define RTCCR2_DYIE_BIT    5
#define RTCCR2_HRIE_BIT    4
#define RTCCR2_MNIE_BIT    3
#define RTCCR2_1SEIE_BIT   2
#define RTCCR2_05SEIE_BIT  1
#define RTCCR2_025SEIE_BIT 0

// RTCCSR
// Clock source select register
#define RTCCSR_MASK         0x7F
#define RTCCSR_CLK_OUT_MASK 0x70
#define RTCCSR_CLK_OUT_SHIFT   4
#define RTCCSR_CLK_SRC_MASK  0xF
#define RTCCSR_CLK_SRC_SHIFT   0


// RTCFLG
// RTC interrupt flag register
#define RTCFLG_MASK      0xFF
#define RTCFLG_FOIFG_BIT    7
#define RTCFLG_WKIFG_BIT    6
#define RTCFLG_DYIFG_BIT    5
#define RTCFLG_HRIFG_BIT    4
#define RTCFLG_MNIFG_BIT    3
#define RTCFLG_1SEIFG_BIT   2
#define RTCFLG_05SEIFG_BIT  1
#define RTCFLG_025SEIFG_BIT 0

typedef struct rtc {
    uint8_t rtccr1;
    uint8_t rtccr2;
    uint8_t rtccsr;
    uint8_t rtcflg;

    struct tm time;
    uint8_t pending_ints;
} rtc_t;

void rtc_init(rtc_t *rtc);

void rtc_update(rtc_t *rtc);

int rtc_poll_int(rtc_t *rtc);

void rtc_set_secdr(rtc_t *rtc, uint8_t byte);
uint8_t rtc_get_secdr(rtc_t *rtc);

void rtc_set_mindr(rtc_t *rtc, uint8_t byte);
uint8_t rtc_get_mindr(rtc_t *rtc);

void rtc_set_hrdr(rtc_t *rtc, uint8_t byte);
uint8_t rtc_get_hrdr(rtc_t *rtc);

void rtc_set_wkdr(rtc_t *rtc, uint8_t byte);
uint8_t rtc_get_wkdr(rtc_t *rtc);

void rtc_set_cr1(rtc_t *rtc, uint8_t byte);
uint8_t rtc_get_cr1(rtc_t *rtc);

void rtc_set_cr2(rtc_t *rtc, uint8_t byte);
uint8_t rtc_get_cr2(rtc_t *rtc);

void rtc_set_csr(rtc_t *rtc, uint8_t byte);
uint8_t rtc_get_csr(rtc_t *rtc);

void rtc_set_flg(rtc_t *rtc, uint8_t byte);
uint8_t rtc_get_flg(rtc_t *rtc);