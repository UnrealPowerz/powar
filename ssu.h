#pragma once
#include <stdint.h>

#define SSU_SSCRH 0xF0E0
#define SSU_SSCRL 0xF0E1
#define SSU_SSMR  0xF0E2
#define SSU_SSER  0xF0E3
#define SSU_SSSR  0xF0E4
#define SSU_SSRDR 0xF0E9
#define SSU_SSTDR 0xF0EB

// SSCRH
// SS control register H
#define SSCRH_MSS_BIT  7
#define SSCRH_BIDE_BIT 6
#define SSCRH_SOOS_BIT 5
#define SSCRH_SOL_BIT  4
#define SSCRH_SOLP_BIT 3
#define SSCRH_SCKS_BIT 2
#define SSCRH_CSS1_BIT 1
#define SSCRH_CSS0_BIT 0

#define SSCRH_MASK 0xFF

// SSCRL
// SS control register L
#define SSCRL_SSUMS_BIT 6
#define SSCRL_SRES_BIT  5
#define SSCRL_SCKOS_BIT 4
#define SSCRL_CSOS_BIT  3

#define SSCRL_MASK 0x78

// SSMR
// SS mode register
#define SSMR_MLS_BIT  7
#define SSMR_CPOS_BIT 6
#define SSMR_CPHS_BIT 5
#define SSMR_CKS2_BIT 2
#define SSMR_CKS1_BIT 1
#define SSMR_CKS0_BIT 0

#define SSMR_MASK 0xE7

// SSER
// SS enable register
#define SSER_TE_BIT    7
#define SSER_RE_BIT    6
#define SSER_RSSTP_BIT 5
#define SSER_TEIE_BIT  3
#define SSER_TIE_BIT   2
#define SSER_RIE_BIT   1
#define SSER_CEIE_BIT  0

#define SSER_MASK 0xEF

// SSSR
// SS status register
#define SSSR_ORER_BIT 6
#define SSSR_TEND_BIT 3
#define SSSR_TDRE_BIT 2
#define SSSR_RDRF_BIT 1
#define SSSR_CE_BIT   0

#define SSSR_MASK 0x4F

// SSRDR
// SS receive data register

// SSTDR
// SS transmit data register

// SSTRSR
// SS shift register

typedef uint8_t (*ssu_read_callback_t)(void*);
typedef void (*ssu_write_callback_t)(void*, uint8_t);

typedef struct ssu_struct {
    uint8_t sscrh;
    uint8_t sscrl;
    uint8_t ssmr;
    uint8_t sser;
    uint8_t sssr;
    uint8_t ssrdr;
    uint8_t sstdr;
    uint8_t sstrsr;

    ssu_read_callback_t read_cb;
    ssu_write_callback_t write_cb;
    void *cb_data_ptr;
} ssu_t;

void ssu_init(ssu_t *ssu);

void ssu_set_sscrh(ssu_t *ssu, uint8_t byte);
uint8_t ssu_get_sscrh(ssu_t *ssu);

void ssu_set_sscrl(ssu_t *ssu, uint8_t byte);
uint8_t ssu_get_sscrl(ssu_t *ssu); 

void ssu_set_ssmr(ssu_t *ssu, uint8_t byte);
uint8_t ssu_get_ssmr(ssu_t *ssu);

void ssu_set_sser(ssu_t *ssu, uint8_t byte);
uint8_t ssu_get_sser(ssu_t *ssu);

void ssu_set_sssr(ssu_t *ssu, uint8_t byte);
uint8_t ssu_get_sssr(ssu_t *ssu);

uint8_t ssu_get_ssrdr(ssu_t *ssu);

void ssu_set_sstdr(ssu_t *ssu, uint8_t byte);
uint8_t ssu_get_sstdr(ssu_t *ssu);

void ssu_callbacks(ssu_t *ssu, ssu_read_callback_t read, ssu_write_callback_t write, void *data_ptr);
