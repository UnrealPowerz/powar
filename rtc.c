#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "rtc.h"
#include "interrupts.h"

#define debug(...)
//printf(__VA_ARGS__)

static uint32_t bcd(uint32_t val) {
    uint32_t high = val / 10;
    uint32_t low  = val % 10;
    return (high << 4) | low;
}

void rtc_init(rtc_t *rtc) {
    memset(rtc, 0, sizeof(rtc_t));
}

void rtc_update(rtc_t *rtc) {
    if (!(rtc->rtccr1 & (1 << RTCCR1_RUN_BIT))) {
        return;
    }
    time_t t = time(NULL);
    struct tm *old = &(rtc->time);
    struct tm *new = localtime(&t);
    
    if (old->tm_sec != new->tm_sec) {
        rtc->pending_ints |= (1 << RTCFLG_1SEIFG_BIT);
        if (old->tm_min != new->tm_min) {
            rtc->pending_ints |= (1 << RTCFLG_MNIFG_BIT);
            if (old->tm_hour != new->tm_hour) {
                rtc->pending_ints |= (1 << RTCFLG_HRIFG_BIT);
            }
        }
    }
    rtc->pending_ints &= rtc->rtccr2;

    *old = *new;
}

int rtc_poll_int(rtc_t *rtc) {
    uint8_t pending = rtc->pending_ints;
    if (pending == 0)
        return -1;
    
    for (int i = 0; i < 8; i++) {
        if (pending & 1) {
            rtc->pending_ints ^= (1 << i);
            return INT_QUARTER_SEC + i;
        }
        pending >>= 1;
    }
    return -1;
}

void rtc_set_secdr(rtc_t *rtc, uint8_t byte) {
    debug("SET SECDR %02x\n", byte);
}

uint8_t rtc_get_secdr(rtc_t *rtc) {
    uint8_t res = bcd(rtc->time.tm_sec);
    debug("GET SECDR %02x\n", res);
    return res;
}

void rtc_set_mindr(rtc_t *rtc, uint8_t byte) {
    debug("SET MINDR %02x\n", byte);
}

uint8_t rtc_get_mindr(rtc_t *rtc) {
    uint8_t res = bcd(rtc->time.tm_min);
    debug("GET MINDR %02x\n", res);
    return res;
}

void rtc_set_hrdr(rtc_t *rtc, uint8_t byte) {
    debug("SET HRDR  %02x\n", byte);
}

uint8_t rtc_get_hrdr(rtc_t *rtc) {
    uint8_t res = bcd(rtc->time.tm_hour);
    debug("GET HRDR %02x\n", res);
    return res;
}

void rtc_set_wkdr(rtc_t *rtc, uint8_t byte) {
    debug("SET WKDR %02x\n", byte);
}

uint8_t rtc_get_wkdr(rtc_t *rtc) {
    uint8_t res = 0;
    debug("GET WKDR %02x\n", res);
    return res;
}

void rtc_set_cr1(rtc_t *rtc, uint8_t byte) {
    debug("SET CR1 %02x RUN:%d 12/24:%d PM:%d RST:%d INT:%d\n",
        byte,
        !!(byte & (1 << RTCCR1_RUN_BIT)),
        !!(byte & (1 << RTCCR1_12_24_BIT)),
        !!(byte & (1 << RTCCR1_PM_BIT)),
        !!(byte & (1 << RTCCR1_RST_BIT)),
        !!(byte & (1 << RTCCR1_INT_BIT))
    );
    if (!(rtc->rtccr1 & (1 << RTCCR1_RUN_BIT))
        && (byte & (1 << RTCCR1_RUN_BIT))) {
        time_t t = time(NULL);
        rtc->time = *localtime(&t);
    }
    rtc->rtccr1 = byte & RTCCR1_MASK;
}

uint8_t rtc_get_cr1(rtc_t *rtc) {
    uint8_t res = rtc->rtccr1;
    debug("GET CR1 %02x\n", res);
    return res;
}

void rtc_set_cr2(rtc_t *rtc, uint8_t byte) {
    debug("SET CR2 %02x FOIE:%d WKIE:%d DYIE:%d HRIE:%d MNIE:%d 1SEIE:%d 05SEIE:%d 025SEIE:%d\n",
        byte,
        !!(byte & (1 << RTCCR2_FOIE_BIT)),
        !!(byte & (1 << RTCCR2_WKIE_BIT)),
        !!(byte & (1 << RTCCR2_DYIE_BIT)),
        !!(byte & (1 << RTCCR2_HRIE_BIT)),
        !!(byte & (1 << RTCCR2_MNIE_BIT)),
        !!(byte & (1 << RTCCR2_1SEIE_BIT)),
        !!(byte & (1 << RTCCR2_05SEIE_BIT)),
        !!(byte & (1 << RTCCR2_025SEIE_BIT))
    );
    rtc->rtccr2 = byte & RTCCR2_MASK;
}

uint8_t rtc_get_cr2(rtc_t *rtc) {
    uint8_t res = rtc->rtccr2;
    debug("GET CR2 %02x\n", res);
    return res;
}

void rtc_set_csr(rtc_t *rtc, uint8_t byte) {
    debug("SET CSR %02x\n", byte);
}

uint8_t rtc_get_csr(rtc_t *rtc) {
    uint8_t res = 0;
    debug("GET CSR %02x\n", res);
    return res;
}

void rtc_set_flg(rtc_t *rtc, uint8_t byte) {
    debug("SET FLG %02x\n", byte);
}

uint8_t rtc_get_flg(rtc_t *rtc) {
    uint8_t res = 0;
    debug("GET FLG %02x\n", res);
    return res;
}