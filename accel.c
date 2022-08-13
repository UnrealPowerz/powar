#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "accel.h"

#ifdef ACCEL_DEBUG
static char *ACCEL_REG_NAME[] = {
/* 00 */ "chip_id<2:0>",
/* 01 */ "al_version<3:0>/ml_version<3:0>",
/* 02 */ "acc_x<1:0>/new_data_x",
/* 03 */ "acc_x<9:2>",
/* 04 */ "acc_y<1:0>/new_data_y",
/* 05 */ "acc_y<9:2>",
/* 06 */ "acc_z<1:0>/new_data_z",
/* 07 */ "acc_z<9:2>",
/* 08 */ "temp",
};
#endif

#define CHIP_ID_ADDR         0x00
#define VERSION_ADDR         0x01
#define ACC_X_NEW_ADDR       0x02
#define ACC_X_ADDR           0x03
#define ACC_Y_NEW_ADDR       0x04
#define ACC_Y_ADDR           0x05
#define ACC_Z_NEW_ADDR       0x06
#define ACC_Z_ADDR           0x07
#define RANGE_BANDWIDTH_ADDR 0x14
#define SPI4_ADDR            0x15
#define CONTROL_REG1_ADDR    0x0A
#define CONTROL_REG2_ADDR    0x0B
#define RESERVED_1E_ADDR     0x1E


static uint8_t internal_read(accel_t *accel, uint8_t addr) {
    uint8_t res = 0;
    switch(addr) {
        case CHIP_ID_ADDR:
            res = 2;
            break;
        case VERSION_ADDR:
        case ACC_X_NEW_ADDR:
        case ACC_X_ADDR:
        case ACC_Y_NEW_ADDR:
        case ACC_Y_ADDR:
        case ACC_Z_NEW_ADDR:
        case ACC_Z_ADDR:
        case RANGE_BANDWIDTH_ADDR:
        case SPI4_ADDR:
        case CONTROL_REG1_ADDR:
        case CONTROL_REG2_ADDR:
        case RESERVED_1E_ADDR:
            break;
        default:
            printf("Unknown accelerometer address: %x\n", addr);
            assert(0);
    }
    return res;
}

void accel_init(accel_t *accel) {
    memset(accel, 0, sizeof(accel_t));
}

void accel_write(accel_t *accel, uint8_t byte) {
    accel->count++;
    //printf("ACCEL WRITE %x CNT:%d\n", byte, accel->count++);
    if (accel->count == 1) {
        accel->read_mode = byte >> 7;
        accel->addr = byte & 0x7F;
        //printf("MODE:%s ADDR:%x\n", accel->read_mode ? "READ" : "WRITE", accel->addr);
    } else if (accel->read_mode) {
        accel->next_read = internal_read(accel, accel->addr);
#ifdef ACCEL_DEBUG
        printf("ACCEL READ  %02x = %x (%s)\n", accel->addr, accel->next_read, accel->addr < 9 ? ACCEL_REG_NAME[accel->addr] : "UNK");
#endif
        accel->addr++;
    } else {
#ifdef ACCEL_DEBUG
        printf("ACCEL WRITE %02x = %x\n", accel->addr, byte);
#endif
    }
}

uint8_t accel_read(accel_t *accel) {
    return accel->next_read;
}

void accel_stop(accel_t *accel) {
    accel->count = 0;
}