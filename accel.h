#pragma once
#include <stdint.h>

typedef struct accel {
    uint8_t read_mode;
    uint8_t addr;
    int count;
    uint8_t next_read;
} accel_t;

void accel_init(accel_t *accel);

void accel_write(accel_t *accel, uint8_t byte);

void accel_stop(accel_t *accel);

uint8_t accel_read(accel_t *accel);