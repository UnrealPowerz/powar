#pragma once
#define NUM_INTERRUPT_SOURCES 40

enum interrupts {
    INT_RESET       = 0,
    INT_MNI         = 7,
    INT_TRAPA0      = 8,
    INT_TRAPA1      = 9,
    INT_TRAPA2      = 10,
    INT_TRAPA3      = 11,
    INT_SLEEP       = 13,
    INT_IRQ0        = 16,
    INT_IRQ1        = 17,
    INT_IRQAEC      = 18,
    INT_COMP0       = 21,
    INT_COMP1       = 22,
    INT_QUARTER_SEC = 23,
    INT_HALF_SEC    = 24,
    INT_SEC         = 25,
    INT_MINUTE      = 26,
    INT_HOUR        = 27,
    INT_DAY         = 28,
    INT_WEEK        = 29,
    INT_FREE        = 30,
    INT_WDT         = 31,
    INT_ASYNC_EVT   = 32,
    INT_TIMER_B1    = 33,
    INT_SSU_IIC2    = 34,
    INT_TIMER_W     = 35,
    INT_SCI3        = 37,
    INT_AD          = 38
};

extern char *int_names[];
