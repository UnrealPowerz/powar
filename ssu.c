#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "ssu.h"

#define debug(...) 
//printf(__VA_ARGS__)

void ssu_init(ssu_t *ssu) {
    ssu->sscrh  = (1 << SSCRH_SOLP_BIT);
    ssu->sscrl  = 0;
    ssu->ssmr   = 0;
    ssu->sser   = 0;
    ssu->sssr   = (1 << SSSR_TDRE_BIT);
    ssu->ssrdr  = 0;
    ssu->sstdr  = 0;
    ssu->sstrsr = 0;

    ssu->read_cb  = NULL;
    ssu->write_cb = NULL;
    ssu->cb_data_ptr = NULL;
}

void ssu_set_sscrh(ssu_t *ssu, uint8_t byte) {
    debug("wSSCRH MSS:%d BIDE:%d SOOS:%d SOL:%d SOLP:%d SCKS:%d CSS1:%d CSS0:%d\n",
        !!(byte & (1 << SSCRH_MSS_BIT)),
        !!(byte & (1 << SSCRH_BIDE_BIT)),
        !!(byte & (1 << SSCRH_SOOS_BIT)),
        !!(byte & (1 << SSCRH_SOL_BIT)),
        !!(byte & (1 << SSCRH_SOLP_BIT)),
        !!(byte & (1 << SSCRH_SCKS_BIT)),
        !!(byte & (1 << SSCRH_CSS1_BIT)),
        !!(byte & (1 << SSCRH_CSS0_BIT))
    );
    ssu->sscrh = byte & SSCRH_MASK;
}

uint8_t ssu_get_sscrh(ssu_t *ssu) {
    uint8_t byte = ssu->sscrh; (void)byte;
    debug("rSSCRH MSS:%d BIDE:%d SOOS:%d SOL:%d SOLP:%d SCKS:%d CSS1:%d CSS0:%d\n",
        !!(byte & (1 << SSCRH_MSS_BIT)),
        !!(byte & (1 << SSCRH_BIDE_BIT)),
        !!(byte & (1 << SSCRH_SOOS_BIT)),
        !!(byte & (1 << SSCRH_SOL_BIT)),
        !!(byte & (1 << SSCRH_SOLP_BIT)),
        !!(byte & (1 << SSCRH_SCKS_BIT)),
        !!(byte & (1 << SSCRH_CSS1_BIT)),
        !!(byte & (1 << SSCRH_CSS0_BIT))
    );
    return ssu->sscrh;
}

void ssu_set_sscrl(ssu_t *ssu, uint8_t byte) {
    debug("wSSCRL SSUMS:%d SRES:%d SCKOS:%d CSOS:%d\n",
        !!(byte & (1 << SSCRL_SSUMS_BIT)),
        !!(byte & (1 << SSCRL_SRES_BIT)),
        !!(byte & (1 << SSCRL_SCKOS_BIT)),
        !!(byte & (1 << SSCRL_CSOS_BIT))
    );
    ssu->sscrl = byte & SSCRL_MASK;
}

uint8_t ssu_get_sscrl(ssu_t *ssu) {
    uint8_t byte = ssu->sscrl; (void)byte;
    debug("rSSCRL SSUMS:%d SRES:%d SCKOS:%d CSOS:%d\n",
        !!(byte & (1 << SSCRL_SSUMS_BIT)),
        !!(byte & (1 << SSCRL_SRES_BIT)),
        !!(byte & (1 << SSCRL_SCKOS_BIT)),
        !!(byte & (1 << SSCRL_CSOS_BIT))
    );
    return ssu->sscrl;
}

void ssu_set_ssmr(ssu_t *ssu, uint8_t byte) {
    debug("wSSMR MLS:%d CPOS:%d CPHS:%d CKS2:%d CKS1:%d CKS0:%d\n",
        !!(byte & (1 << SSMR_MLS_BIT)),
        !!(byte & (1 << SSMR_CPOS_BIT)),
        !!(byte & (1 << SSMR_CPHS_BIT)),
        !!(byte & (1 << SSMR_CKS2_BIT)),
        !!(byte & (1 << SSMR_CKS1_BIT)),
        !!(byte & (1 << SSMR_CKS0_BIT))
    );
    ssu->ssmr = byte & SSMR_MASK;
}

uint8_t ssu_get_ssmr(ssu_t *ssu) {
    uint8_t byte = ssu->ssmr; (void)byte;
    debug("rSSMR MLS:%d CPOS:%d CPHS:%d CKS2:%d CKS1:%d CKS0:%d\n",
        !!(byte & (1 << SSMR_MLS_BIT)),
        !!(byte & (1 << SSMR_CPOS_BIT)),
        !!(byte & (1 << SSMR_CPHS_BIT)),
        !!(byte & (1 << SSMR_CKS2_BIT)),
        !!(byte & (1 << SSMR_CKS1_BIT)),
        !!(byte & (1 << SSMR_CKS0_BIT))
    );
    return ssu->ssmr;
}

void ssu_set_sser(ssu_t *ssu, uint8_t byte) {
    debug("wSSER TE:%d RE:%d RSSTP:%d TEIE:%d TIE:%d RIE:%d CEIE:%d\n",
        !!(byte & (1 << SSER_TE_BIT)),
        !!(byte & (1 << SSER_RE_BIT)),
        !!(byte & (1 << SSER_RSSTP_BIT)),
        !!(byte & (1 << SSER_TEIE_BIT)),
        !!(byte & (1 << SSER_TIE_BIT)),
        !!(byte & (1 << SSER_RIE_BIT)),
        !!(byte & (1 << SSER_CEIE_BIT))
    );
    ssu->sser = byte & SSER_MASK;
}

uint8_t ssu_get_sser(ssu_t *ssu) {
    uint8_t byte = ssu->sser; (void)byte;
    debug("rSSER TE:%d RE:%d RSSTP:%d TEIE:%d TIE:%d RIE:%d CEIE:%d\n",
        !!(byte & (1 << SSER_TE_BIT)),
        !!(byte & (1 << SSER_RE_BIT)),
        !!(byte & (1 << SSER_RSSTP_BIT)),
        !!(byte & (1 << SSER_TEIE_BIT)),
        !!(byte & (1 << SSER_TIE_BIT)),
        !!(byte & (1 << SSER_RIE_BIT)),
        !!(byte & (1 << SSER_CEIE_BIT))
    );
    return ssu->sser;
}

void ssu_set_sssr(ssu_t *ssu, uint8_t byte) {
    debug("wSSSR ORER:%d TEND:%d TDRE:%d RDRF:%d CE:%d\n",
        !!(byte & (1 << SSSR_ORER_BIT)),
        !!(byte & (1 << SSSR_TEND_BIT)),
        !!(byte & (1 << SSSR_TDRE_BIT)),
        !!(byte & (1 << SSSR_RDRF_BIT)),
        !!(byte & (1 << SSSR_CE_BIT))
    );
    // only allowed to clear flags, not set them
    ssu->sssr &= byte & SSSR_MASK;
}

uint8_t ssu_get_sssr(ssu_t *ssu) {
    uint8_t byte = (1 << SSSR_TEND_BIT) | (1 << SSSR_TDRE_BIT) | (1 << SSSR_RDRF_BIT);
    (void)byte;
    debug("rSSSR ORER:%d TEND:%d TDRE:%d RDRF:%d CE:%d\n",
        !!(byte & (1 << SSSR_ORER_BIT)),
        !!(byte & (1 << SSSR_TEND_BIT)),
        !!(byte & (1 << SSSR_TDRE_BIT)),
        !!(byte & (1 << SSSR_RDRF_BIT)),
        !!(byte & (1 << SSSR_CE_BIT))
    );
    return byte;
    // FIXME
    // return ssu->sssr;
}

uint8_t ssu_get_ssrdr(ssu_t *ssu) {
    if (ssu->read_cb) {
        ssu->ssrdr = ssu->read_cb(ssu->cb_data_ptr);
    }
    debug("rSSRDR %x\n", ssu->ssrdr);
    return ssu->ssrdr;
}

void ssu_set_sstdr(ssu_t *ssu, uint8_t byte) {
    debug("wSSTDR %x\n", byte);
    if (ssu->write_cb) {
        ssu->write_cb(ssu->cb_data_ptr, byte);
    }
    ssu->sstdr = byte;
}

uint8_t ssu_get_sstdr(ssu_t *ssu) {
    debug("rSSTDR %x\n", ssu->sstdr);
    return ssu->sstdr;
}

void ssu_callbacks(ssu_t *ssu, ssu_read_callback_t read, ssu_write_callback_t write, void *data_ptr) {
    ssu->read_cb  = read;
    ssu->write_cb = write;
    ssu->cb_data_ptr = data_ptr;
}
