#include <stdint.h>

typedef struct eeprom {
    uint8_t *mem;

    uint8_t buf[10];
    int buf_off;
    uint8_t next_read;
} eeprom_t;

void eeprom_init(eeprom_t *eeprom, uint8_t *mem);

uint8_t eeprom_spi_read(eeprom_t *eeprom);

void eeprom_spi_write(eeprom_t *eeprom, uint8_t byte);

void eeprom_stop(eeprom_t *eeprom);