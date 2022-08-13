#include "portb.h"
#include <stdio.h>

#define PMRB_MASK 0xB
#define PMRB_ADTSTCHG_BIT 3
#define PMRB_IRQ1_BIT 1
#define PMRB_IRQ0_BIT 0

#define PDRB_MASK 0x3F

void portb_init(portb_t *pb) {
    pb->pmrb = 0;
}

void portb_update(portb_t *pb, uint8_t byte) {
    pb->pdrb = byte & PDRB_MASK;
}

//int rtc_poll_int(rtc_t *rtc);

void portb_set_pmrb(portb_t *pb, uint8_t byte) {
    pb->pmrb = byte & PMRB_MASK;
    printf("[PMRB] ADTSTCHG:%d IRQ1:%d IRQ0:%d\n", 
        !!(pb->pmrb & (1 << PMRB_ADTSTCHG_BIT)),
        !!(pb->pmrb & (1 << PMRB_IRQ1_BIT)),
        !!(pb->pmrb & (1 << PMRB_IRQ0_BIT))
    );
}

uint8_t portb_get_pmrb(portb_t *pb) {
    return pb->pmrb;
}

uint8_t portb_get_pdrb(portb_t *pb) {
    //printf("READ PDRB %x\n", pb->pdrb);
    return pb->pdrb;
}