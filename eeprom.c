#include <stdio.h>
#include "eeprom.h"

enum EEPROM_CMD {
    WRSR = 1, WRITE, READ, WRDI, RDSR, WREN
};

char *eeprom_cmds[] = {
    "WRSR",
    "WRITE",
    "READ",
    "WRDI",
    "RDSR",
    "WREN"
};

void eeprom_init(eeprom_t *eeprom, uint8_t *mem) {
    eeprom->mem       = mem;
    eeprom->buf_off   = -1;
    eeprom->next_read = 0;
}

uint8_t eeprom_spi_read(eeprom_t *eeprom) {
    return eeprom->next_read;
}

void eeprom_spi_write(eeprom_t *eeprom, uint8_t byte) {
    // TODO: clean this up
    if (eeprom->buf_off == -1 && (byte < 1 || byte > 6)) {
        printf("Ignoring bogus instruction %x\n", byte);
    } else {
        eeprom->buf_off++;
        if (eeprom->buf_off < 10) {
            eeprom->buf[eeprom->buf_off] = byte;
        }
        switch(eeprom->buf[0]) {
            case READ:
            case WRITE:
                if (eeprom->buf_off > 2) {
                    uint16_t start = (eeprom->buf[1] << 8) | eeprom->buf[2];
                    uint16_t addr = start+eeprom->buf_off-3;
                    if (eeprom->buf[0] == READ) {
                        eeprom->next_read = eeprom->mem[addr];;
                    } else {
                        eeprom->mem[addr] = byte;
                        eeprom->next_read = 0;
                    }
                }
                break;
            case RDSR:
            case WREN:
                eeprom->next_read = 0;
                break;
        }
    }
}

void eeprom_stop(eeprom_t *eeprom) {
    eeprom->buf_off = -1;
}