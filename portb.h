#pragma once
#include <stdint.h>

typedef struct portb {
    uint8_t pmrb;
    uint8_t pdrb;
} portb_t;

void portb_init(portb_t *pb);

void portb_update(portb_t *pb, uint8_t byte);

//int rtc_poll_int(rtc_t *rtc);

void portb_set_pmrb(portb_t *pb, uint8_t byte);
uint8_t portb_get_pmrb(portb_t *pb);

uint8_t portb_get_pdrb(portb_t *pb);