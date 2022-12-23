#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <stdarg.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "ssu.h"
#include "eeprom.h"
#include "lcd.h"
#include "accel.h"
#include "rtc.h"
#include "interrupts.h"
#include "main.h"

// TODO: internal states for:
// MOV.W Rs,@–ERd
// POP.W Rn

#define STATES(si, sj, sk, sl, sm, sn) ctx->i = si; ctx->j = sj; ctx->k = sk; ctx->l = sl; ctx->m = sm; ctx->n = sn;

#ifdef PKW_DEBUG
__attribute__ ((unused)) static void debug_internal(const char *format, ...) {
    if (1) {
        va_list args;
        va_start(args, format);
        vfprintf(stdout, format, args);

        va_end(args);
        fflush(stdout);
    }
}
#endif

//#define PKW_DEBUG
#ifdef PKW_DEBUG
#define debug(...) debug_internal(__VA_ARGS__)
#else
#define debug(...)
#endif

#define UNIMPL(...)
//debug_internal(__VA_ARGS__)

#ifdef _MSC_VER
#define likely(x)       (x)
#define unlikely(x)     (x)
#else
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)
#endif

#define SCREEN_WIDTH 96
#define SCREEN_HEIGHT 64

#define GFX_SCALE 5
#define WINDOW_WIDTH  (SCREEN_WIDTH*GFX_SCALE)
#define WINDOW_HEIGHT (SCREEN_HEIGHT*GFX_SCALE)
#define WINDOW_TITLE "Powar - Pokéwalker emulator"

static SDL_Window* gWindow = NULL;
static SDL_Renderer* renderer;
static SDL_Texture* sdlTexture;

enum REGISTER_TYPE {
    REGTYPE_DBW8_ACCS2,
    REGTYPE_DBW8_ACCS3,
    REGTYPE_DBW16_ACCS2
};

typedef struct mm_reg {
    char *name;
    char *desc;
    uint16_t addr;
    enum REGISTER_TYPE type;
    union {
        uint8_t  (*read8)(uintptr_t);
        uint16_t (*read16)(uintptr_t);
    };
    union {
        void (*write8)(uintptr_t, uint8_t);
        void (*write16)(uintptr_t, uint16_t);
    };
    int ctx_offset;
} mm_reg_t;

#define MM_REG8( _name, _addr, _type, _read, _write, _desc, _ctx_off) { .name = _name, .desc = _desc, .addr = _addr, .type = _type, .read8 = (uint8_t (*)(uintptr_t)) _read, .write8 = (void (*)(uintptr_t, uint8_t)) _write, .ctx_offset = _ctx_off}
#define MM_REG16(_name, _addr, _type, _read, _write, _desc, _ctx_off) { .name = _name, .desc = _desc, .addr = _addr, .type = _type, .read16 = (uint16_t (*)(uintptr_t)) _read, .write16 = (void (*)(uintptr_t, uint16_t)) _write, .ctx_offset = _ctx_off}

//MM_REG("UNK_REG",   0xF088, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Unknown IO Reg (TODO?)", 0),

static uint8_t io2_get_pdr1(pw_context_t *ctx);
static void io2_set_pdr1(pw_context_t *ctx, uint8_t val);
static uint8_t io2_get_pdr9(pw_context_t *ctx);
static void io2_set_pdr9(pw_context_t *ctx, uint8_t val);

static uint8_t sci_get_rdr(pw_context_t *ctx) {
    return 0xFC ^ 0xAA;// ctx->rdr;
}

static void sci_set_tdr(pw_context_t *ctx, uint8_t val) {
    ctx->tdr = val;
}

static uint8_t sci_get_tdr(pw_context_t *ctx) {
    return ctx->tdr;
}

#define SCI_SMR_COM_BIT 7

static void sci_set_smr(pw_context_t *ctx, uint8_t val) {
    ctx->smr = val;
    printf("[SCI SMR] COM:%d\n", 
        !!(ctx->smr & (1 << SCI_SMR_COM_BIT))
    );
}

static uint8_t sci_get_smr(pw_context_t *ctx) {
    return ctx->smr;
}

#define SCI_SCR_TIE_BIT 7
#define SCI_SCR_RIE_BIT 6
#define SCI_SCR_TE_BIT 5
#define SCI_SCR_RE_BIT 4
#define SCI_SCR_MPIE_BIT 3
#define SCI_SCR_TEIE_BIT 2
#define SCI_SCR_CKE_MASK 3

static void sci_set_scr(pw_context_t *ctx, uint8_t val) {
    ctx->scr = val;
    printf("[SCI SCR] TIE:%d RIE:%d TE:%d RE:%d MPIE:%d TEIE:%d CKE:%d\n", 
        !!(ctx->scr & (1 << SCI_SCR_TIE_BIT)),
        !!(ctx->scr & (1 << SCI_SCR_RIE_BIT)),
        !!(ctx->scr & (1 << SCI_SCR_TE_BIT)),
        !!(ctx->scr & (1 << SCI_SCR_RE_BIT)),
        !!(ctx->scr & (1 << SCI_SCR_MPIE_BIT)),
        !!(ctx->scr & (1 << SCI_SCR_TEIE_BIT)),
        (ctx->scr & SCI_SCR_CKE_MASK)
    );
}

static uint8_t sci_get_scr(pw_context_t *ctx) {
    return ctx->scr;
}

static void sci_set_ssr(pw_context_t *ctx, uint8_t val) {
    ctx->ssr &= val;
    //printf("[SCI SSR] %x\n", val);
}

static uint8_t sci_get_ssr(pw_context_t *ctx) {
    return 0xBF; //ctx->ssr;
}

#define SCI_IRCR_IRE_BIT 7
#define SCI_IRCR_IRCKS_MASK 0x70
#define SCI_IRCR_IRCKS_SHIFT 4
#define SCI_IRCR_MASK 0xF0

static uint8_t sci_get_ircr(pw_context_t *ctx) {
    return ctx->ircr;
}

static void sci_set_ircr(pw_context_t *ctx, uint8_t val) {
    ctx->ircr = val & SCI_IRCR_MASK;
    printf("[SCI IRCR] IRE:%d IRCKS:%d\n", 
        !!(ctx->ircr & (1 << SCI_IRCR_IRE_BIT)),
         ((ctx->ircr & SCI_IRCR_IRCKS_MASK) >> SCI_IRCR_IRCKS_SHIFT)
    );
}

static uint8_t sci_get_semr(pw_context_t *ctx) {
    return ctx->semr;
}

#define SCI_SEMR_MASK 8
#define SCI_SEMR_ABCS_BIT 3

static void sci_set_semr(pw_context_t *ctx, uint8_t val) {
    ctx->semr = val & SCI_SEMR_MASK;
    printf("[SCI SEMR] ABCS:%d\n", 
        !!(ctx->semr & (1 << SCI_SEMR_ABCS_BIT))
    );
}

static uint16_t adc_get_addr(pw_context_t *ctx) {
    return 0xFFC0;
}

static uint8_t sys_get_osccr(pw_context_t *ctx) {
    // OSCF is determined by E7_2 pin
    //printf("[SYS OSCCR] READ\n");
    return ctx->osccr;
}

static void sys_set_osccr(pw_context_t *ctx, uint8_t val) {
    ctx->osccr = val;
    // printf("[SYS OSCCR] SUBSTP:%d RFCUT:%d SUBSEL:%d\n", 
    //     !!(ctx->osccr & (1 << 7)),
    //     !!(ctx->osccr & (1 << 6)),
    //     !!(ctx->osccr & (1 << 5))
    // );
}

static uint8_t sys_get_pfcr(pw_context_t *ctx) {
    return ctx->pfcr;
}

static void sys_set_pfcr(pw_context_t *ctx, uint8_t val) {
    ctx->pfcr = val;
    // printf("[SYS PFCR] SSUS:%d IRQ1:%d IRQ0:%d\n", 
    //     !!(ctx->pfcr & (1 << 4)),
    //      ((ctx->pfcr >> 2) & 3),
    //       (ctx->pfcr & 3)
    // );
}

#define SYS_SYSCR1_SSBY_BIT 7
#define SYS_SYSCR1_STS_SHIFT 4
#define SYS_SYSCR1_STS_MASK 7
#define SYS_SYSCR1_LSON_BIT 3
#define SYS_SYSCR1_TMA3_BIT 2
#define SYS_SYSCR1_MA_MASK 3

static uint8_t sys_get_syscr1(pw_context_t *ctx) {
    return ctx->syscr1;
}

static void sys_set_syscr1(pw_context_t *ctx, uint8_t val) {
    ctx->syscr1 = val;
    // printf("[SYS SYSCR1] SSBY:%d STS:%d LSON:%d TMA3:%d MA:%d\n", 
    //     !!(ctx->syscr1 & (1 << SYS_SYSCR1_SSBY_BIT)),
    //      ((ctx->syscr1 >> SYS_SYSCR1_STS_SHIFT) & SYS_SYSCR1_STS_MASK),
    //     !!(ctx->syscr1 & (1 << SYS_SYSCR1_LSON_BIT)),
    //     !!(ctx->syscr1 & (1 << SYS_SYSCR1_TMA3_BIT)),
    //       (ctx->syscr1 & SYS_SYSCR1_MA_MASK)
    // );
}

#define SYS_SYSCR2_NESEL_BIT 4
#define SYS_SYSCR2_DTON_BIT 3
#define SYS_SYSCR2_MSON_BIT 2
#define SYS_SYSCR2_SA_MASK 3

static uint8_t sys_get_syscr2(pw_context_t *ctx) {
    return ctx->syscr2;
}

static void sys_set_syscr2(pw_context_t *ctx, uint8_t val) {
    ctx->syscr2 = val;
    // printf("[SYS SYSCR2] NESEL:%d DTON:%d MSON:%d SA:%d\n", 
    //     !!(ctx->syscr2 & (1 << SYS_SYSCR2_NESEL_BIT)),
    //     !!(ctx->syscr2 & (1 << SYS_SYSCR2_DTON_BIT)),
    //     !!(ctx->syscr2 & (1 << SYS_SYSCR2_MSON_BIT)),
    //       (ctx->syscr2 & SYS_SYSCR2_SA_MASK)
    // );
}

static uint8_t sys_get_ckstpr1(pw_context_t *ctx) {
    return ctx->ckstpr1;
}

static void sys_set_ckstpr1(pw_context_t *ctx, uint8_t val) {
    ctx->ckstpr1 = val;
    // printf("[SYS CKSTPR1] S3CKSTP:%d ADCKSTP:%d TB1CKSTP:%d FROMCKSTP:%d RTCCKSTP:%d\n", 
    //     !!(ctx->ckstpr1 & (1 << 6)),
    //     !!(ctx->ckstpr1 & (1 << 4)),
    //     !!(ctx->ckstpr1 & (1 << 2)),
    //     !!(ctx->ckstpr1 & (1 << 1)),
    //     !!(ctx->ckstpr1 &  1)
    // );
}

static uint8_t sys_get_ckstpr2(pw_context_t *ctx) {
    return ctx->ckstpr2;
}

static void sys_set_ckstpr2(pw_context_t *ctx, uint8_t val) {
    ctx->ckstpr2 = val;
    // printf("[SYS CKSTPR2] TWCKSTP:%d IICCKSTP:%d SSUCKSTP:%d AECCKSTP:%d WDCKSTP:%d COMPCKSTP:%d\n", 
    //     !!(ctx->ckstpr2 & (1 << 6)),
    //     !!(ctx->ckstpr2 & (1 << 5)),
    //     !!(ctx->ckstpr2 & (1 << 4)),
    //     !!(ctx->ckstpr2 & (1 << 3)),
    //     !!(ctx->ckstpr2 & (1 << 2)),
    //     !!(ctx->ckstpr2 & (1 << 1))
    // );
}

const char *exec_modes[] = {
    "Active (high-speed) mode",
    "Active (medium-speed) mode",
    "Subactive mode",
    "Sleep (high-speed) mode",
    "Sleep (medium-speed) mode",
    "Subsleep mode",
    "Standby mode",
    "Watch mode"
};

void sleep(pw_context_t *ctx) {
    return;
    //exec_mode_t next = -1;
    //int lson = ctx->syscr1 & (1 << SYS_SYSCR1_LSON_BIT);
    // int mson = ctx->syscr2 & (1 << SYS_SYSCR2_MSON_BIT);
    // int ssby = ctx->syscr1 & (1 << SYS_SYSCR1_SSBY_BIT);
    // int tma3 = ctx->syscr1 & (1 << SYS_SYSCR1_TMA3_BIT);
    // int dton = ctx->syscr2 & (1 << SYS_SYSCR2_DTON_BIT);

    // LSON (SYSCR1:3)
    //   Selects the system clock (φ) or subclock (φSUB) as the
    //   CPU operating clock when watch mode is cleared.
    //     0: The CPU operates on the system clock (φ)
    //     1: The CPU operates on the subclock (φSUB)
    // MSON (SYSCR2:2)
    //   After standby, watch, or sleep mode is cleared, this bit
    //   selects active (high-speed) or active (medium-speed) mode.
    //     0: Operation in active (high-speed) mode
    //     1: Operation in active (medium-speed) mode
    // SSBY (SYSCR1:7)
    //   Selects the mode to transit after the execution of the
    //   SLEEP instruction.
    //     0: A transition is made to sleep mode or subsleep mode.
    //     1: A transition is made to standby mode or watch mode.
    // TMA3 (SYSCR1:2)
    //   Selects the mode to which the transition is made after
    //   the SLEEP instruction is executed with bits SSBY and
    //   LSON in SYSCR1 and bits DTON and MSON in SYSCR2.
    // DTON (SYSCR2:3)
    //   Selects the mode to which the transition is made after
    //   the SLEEP instruction is executed with bits SSBY,
    //   TMA3, and LSON in SYSCR1 and bit MSON in SYSCR2.

    // ACTIVE HIGH =(a)==> SLEEP HIGH
    // ACTIVE MED  =(a)==> SLEEP HIGH
    // ACTIVE HIGH =(b)==> SLEEP MED
    // ACTIVE MED  =(b)==> SLEEP MED
    // SUBACTIVE   =(c)==> SUBSLEEP
    // ACTIVE HIGH =(d)==> STANDBY
    // ACTIVE MED  =(d)==> STANDBY
    // ACTIVE HIGH =(e)==> WATCH
    // ACTIVE MED  =(e)==> WATCH
    // SUBACTIVE   =(e)==> WATCH
    // ACTIVE MED  =(f)==> ACTIVE HIGH
    // ACTIVE HIGH =(g)==> ACTIVE MED
    // SUBACTIVE   =(h)==> ACTIVE MED
    // ACTIVE HIGH =(i1)=> SUBACTIVE
    // ACTIVE MED  =(i2)=> SUBACTIVE
    // SUBACTIVE   =(j)==> ACTIVE HIGH

    // LSON MSON SSBY TMA3 DTON
    // a : 0 0 0 x 0
    // b : 0 1 0 x 0
    // c : 1 x 0 1 0
    // d : 0 x 1 0 0
    // e : x x 1 1 0
    // f : 0 0 0 x 1
    // g : 0 1 0 x 1
    // h : 0 1 1 1 1
    // i1: 1 x 1 1 1
    // i2: 1 1 1 1 1
    // j : 0 0 1 1 1
    

    // switch (ctx->mode) {
    //     case MODE_ACTIVE_HIGH: // a,b,d,e,g,i1
    //         if (ssby) { // d,e,i1
    //             if (tma3) { // e, i1
    //                 if (dton) { // i1
    //                     next = MODE_SUBACTIVE;
    //                 } else { // e
    //                     next = MODE_WATCH;
    //                 }
    //             } else { // d
    //                 next = MODE_STANDBY;
    //             }
    //         } else { // a,b,g
    //             if (mson) { // b,g
    //                 if (dton) { // g
    //                     next = MODE_ACTIVE_MED;
    //                 } else { // b
    //                     next = MODE_SLEEP_MED;
    //                 }
    //             } else { // a
    //                 next = MODE_SLEEP_HIGH;
    //             }
    //         }
    //         break;
    //     case MODE_ACTIVE_MED: // a,b,d,e,f,i2
    //         if (ssby) { // d,e,i1
    //             if (tma3) { // e, i1
    //                 if (dton) { // i1
    //                     next = MODE_SUBACTIVE;
    //                 } else { // e
    //                     next = MODE_WATCH;
    //                 }
    //             } else { // d
    //                 next = MODE_STANDBY;
    //             }
    //         } else { // a,b,f
    //             if (mson) { // b
    //                 next = MODE_SLEEP_MED;
    //             } else { // a,f
    //                 if (dton) { // f
    //                     next = MODE_ACTIVE_HIGH;
    //                 } else { // a
    //                     next = MODE_SLEEP_HIGH;
    //                 }
    //             }
    //         }
    //         break;
    //     case MODE_SUBACTIVE: // c,e,h,j
    //         if (dton) { // h,j
    //             if (mson) { // h
    //                 next = MODE_ACTIVE_MED;
    //             } else { // j
    //                 next = MODE_ACTIVE_HIGH;
    //             }
    //         } else { // c,e
    //             if (ssby) { // e
    //                 next = MODE_WATCH;
    //             } else { // c
    //                 next = MODE_SUBSLEEP;
    //             }
    //         }
    //         break;
    //     default:
    // }
    // printf("SLEEP %x LSON:%d MSON:%d SSBY:%d TMA3:%d DTON:%d\n[%s] => [%s]\n", 
    //     ctx->ip,
    //     !!lson, !!mson, !!ssby, !!tma3, !!dton,
    //     exec_modes[ctx->mode], exec_modes[next]);
}

static uint8_t int_get_iegr(pw_context_t *ctx) {
    return ctx->iegr;
}

static void int_set_iegr(pw_context_t *ctx, uint8_t val) {
    ctx->iegr = val;
}

static uint8_t int_get_ienr1(pw_context_t *ctx) {
    return ctx->ienr1;
}

static void int_set_ienr1(pw_context_t *ctx, uint8_t val) {
    ctx->ienr1 = val;
}

static uint8_t int_get_ienr2(pw_context_t *ctx) {
    return ctx->ienr2;
}

static void int_set_ienr2(pw_context_t *ctx, uint8_t val) {
    ctx->ienr2 = val;
}

static uint8_t int_get_irr1(pw_context_t *ctx) {
    return ctx->irr1;
}

static void int_set_irr1(pw_context_t *ctx, uint8_t val) {
    ctx->irr1 = val;
}

static uint8_t int_get_irr2(pw_context_t *ctx) {
    return ctx->irr2;
}

static void int_set_irr2(pw_context_t *ctx, uint8_t val) {
    ctx->irr2 = val;
}

// Timer B1
static void tb1_set_tmb1(pw_context_t *ctx, uint8_t val) {
    ctx->tmb1 = val;
}

static uint8_t tb1_get_tmb1(pw_context_t *ctx) {
    return ctx->tmb1;
}

static uint8_t tb1_get_tcb1(pw_context_t *ctx) {
    return ctx->tmb1;
}

static void tb1_set_tlb1(pw_context_t *ctx, uint8_t val) {
    ctx->tlb1 = val;
}

// Timer W

#define TMRW_TMRW_CTS_BIT 7
#define TMRW_TMRW_BUFEB_BIT 5
#define TMRW_TMRW_BUFEA_BIT 4
#define TMRW_TMRW_PWMD_BIT 2
#define TMRW_TMRW_PWMC_BIT 1
#define TMRW_TMRW_PWMB_BIT 0
#define TMRW_TMRW_MASK 0xB7
static uint8_t tmrw_get_tmrw(pw_context_t *ctx) {
    return ctx->tmrw;
}

static void tmrw_set_tmrw(pw_context_t *ctx, uint8_t val) {
    ctx->tmrw = val & TMRW_TMRW_MASK;
    // printf("[TMRW TMRW] CTS:%d BUFEB:%d BUFEA:%d PWMD:%d PWMC:%d PWMB:%d\n", 
    //     !!(ctx->tmrw & (1 << TMRW_TMRW_CTS_BIT)),
    //     !!(ctx->tmrw & (1 << TMRW_TMRW_BUFEB_BIT)),
    //     !!(ctx->tmrw & (1 << TMRW_TMRW_BUFEA_BIT)),
    //     !!(ctx->tmrw & (1 << TMRW_TMRW_PWMD_BIT)),
    //     !!(ctx->tmrw & (1 << TMRW_TMRW_PWMC_BIT)),
    //       (ctx->tmrw & (1 << TMRW_TMRW_PWMB_BIT))
    // );
}

#define TMRW_TCRW_CCLR_BIT 7
#define TMRW_TCRW_CKS_MASK 0x70
#define TMRW_TCRW_CKS_SHIFT 4
#define TMRW_TCRW_TOD_BIT 3
#define TMRW_TCRW_TOC_BIT 2
#define TMRW_TCRW_TOB_BIT 1
#define TMRW_TCRW_TOA_BIT 0

static uint8_t tmrw_get_tcrw(pw_context_t *ctx) {
    return ctx->tcrw;
}

static void tmrw_set_tcrw(pw_context_t *ctx, uint8_t val) {
    ctx->tcrw = val;
    // printf("[TMRW TCRW] CCLR:%d CKS:%d TOD:%d TOC:%d TOB:%d TOA:%d\n", 
    //     !!(ctx->tcrw & (1 << TMRW_TCRW_CCLR_BIT)),
    //     (ctx->tcrw & TMRW_TCRW_CKS_MASK) >> TMRW_TCRW_CKS_SHIFT,
    //     !!(ctx->tcrw & (1 << TMRW_TCRW_TOD_BIT)),
    //     !!(ctx->tcrw & (1 << TMRW_TCRW_TOC_BIT)),
    //     !!(ctx->tcrw & (1 << TMRW_TCRW_TOB_BIT)),
    //       (ctx->tcrw & (1 << TMRW_TCRW_TOA_BIT))
    // );
}

#define TMRW_TIERW_OVIE_BIT 7
#define TMRW_TIERW_IMIED_BIT 3
#define TMRW_TIERW_IMIEC_BIT 2
#define TMRW_TIERW_IMIEB_BIT 1
#define TMRW_TIERW_IMIEA_BIT 0
#define TMRW_TIERW_MASK 0x8F

static uint8_t tmrw_get_tierw(pw_context_t *ctx) {
    return ctx->tierw;
}

static void tmrw_set_tierw(pw_context_t *ctx, uint8_t val) {
    ctx->tierw = val & TMRW_TIERW_MASK;
    // printf("[TMRW TIERW] OVIE:%d IMIED:%d IMIEC:%d IMIEB:%d IMIEA:%d\n", 
    //     !!(ctx->tierw & (1 << TMRW_TIERW_OVIE_BIT)),
    //     !!(ctx->tierw & (1 << TMRW_TIERW_IMIED_BIT)),
    //     !!(ctx->tierw & (1 << TMRW_TIERW_IMIEC_BIT)),
    //     !!(ctx->tierw & (1 << TMRW_TIERW_IMIEB_BIT)),
    //       (ctx->tierw & (1 << TMRW_TIERW_IMIEA_BIT))
    // );
}

#define TMRW_TSRW_OVF_BIT 7
#define TMRW_TSRW_IMFD_BIT 3
#define TMRW_TSRW_IMFC_BIT 2
#define TMRW_TSRW_IMFB_BIT 1
#define TMRW_TSRW_IMFA_BIT 0
#define TMRW_TSRW_MASK 0x8F

static uint8_t tmrw_get_tsrw(pw_context_t *ctx) {
    return ctx->tsrw;
}

static void tmrw_set_tsrw(pw_context_t *ctx, uint8_t val) {
    ctx->tsrw &= val | ~TMRW_TSRW_MASK;
    // printf("[TMRW TSRW] OVF:%d IMFD:%d IMFC:%d IMFB:%d IMFA:%d\n", 
    //     !!(ctx->tsrw & (1 << TMRW_TSRW_OVF_BIT)),
    //     !!(ctx->tsrw & (1 << TMRW_TSRW_IMFD_BIT)),
    //     !!(ctx->tsrw & (1 << TMRW_TSRW_IMFC_BIT)),
    //     !!(ctx->tsrw & (1 << TMRW_TSRW_IMFB_BIT)),
    //       (ctx->tsrw & (1 << TMRW_TSRW_IMFA_BIT))
    // );
}

#define TMRW_TIOR0_IOB_MASK 0x70
#define TMRW_TIOR0_IOB_SHIFT 4
#define TMRW_TIOR0_IOA_MASK 7
#define TMRW_TIOR0_IOA_SHIFT 0
#define TMRW_TIOR0_MASK 0x77

static uint8_t tmrw_get_tior0(pw_context_t *ctx) {
    return ctx->tior0;
}

static void tmrw_set_tior0(pw_context_t *ctx, uint8_t val) {
    ctx->tior0 = val & TMRW_TIOR0_MASK;
    // printf("[TMRW TIOR0] IOB:%d IOA:%d\n", 
    //     (ctx->tior0 & TMRW_TIOR0_IOB_MASK) >> TMRW_TIOR0_IOB_SHIFT,
    //     (ctx->tior0 & TMRW_TIOR0_IOA_MASK) >> TMRW_TIOR0_IOA_SHIFT
    // );
}

#define TMRW_TIOR1_IOD_MASK 0x70
#define TMRW_TIOR1_IOD_SHIFT 4
#define TMRW_TIOR1_IOC_MASK 7
#define TMRW_TIOR1_IOC_SHIFT 0
#define TMRW_TIOR1_MASK 0x77

static uint8_t tmrw_get_tior1(pw_context_t *ctx) {
    return ctx->tior1;
}

static void tmrw_set_tior1(pw_context_t *ctx, uint8_t val) {
    ctx->tior1 = val & TMRW_TIOR1_MASK;
    // printf("[TMRW TIOR1] IOD:%d IOC:%d\n", 
    //     (ctx->tior1 & TMRW_TIOR1_IOD_MASK) >> TMRW_TIOR1_IOD_SHIFT,
    //     (ctx->tior1 & TMRW_TIOR1_IOC_MASK) >> TMRW_TIOR1_IOC_SHIFT
    // );
}

static uint16_t tmrw_get_tcnt(pw_context_t *ctx) {
    return ctx->tcnt;
}

static void tmrw_set_tcnt(pw_context_t *ctx, uint16_t val) {
    ctx->tcnt = val;
    // printf("[TMRW TCNT] %x\n", val);
}

static uint16_t tmrw_get_gra(pw_context_t *ctx) {
    return ctx->gra;
}

static void tmrw_set_gra(pw_context_t *ctx, uint16_t val) {
    ctx->gra = val;
    // printf("[TMRW GRA] %x @ %x\n", val, ctx->ip);
}

static uint16_t tmrw_get_grb(pw_context_t *ctx) {
    return ctx->grb;
}

static void tmrw_set_grb(pw_context_t *ctx, uint16_t val) {
    ctx->grb = val;
    // printf("[TMRW GRB] %x\n", val);
}

static uint16_t tmrw_get_grc(pw_context_t *ctx) {
    return ctx->grc;
}

static void tmrw_set_grc(pw_context_t *ctx, uint16_t val) {
    ctx->grc = val;
    // printf("[TMRW GRC] %x\n", val);
}

static uint16_t tmrw_get_grd(pw_context_t *ctx) {
    return ctx->grd;
}

static void tmrw_set_grd(pw_context_t *ctx, uint16_t val) {
    ctx->grd = val;
    // printf("[TMRW GRD] %x\n", val);
}

mm_reg_t mm_registers[] = {
MM_REG8("FLMCR1",  0xF020, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Flash memory control register 1", 0),
MM_REG8("FLMCR2",  0xF021, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Flash memory control register 2", 0),
MM_REG8("FLPWCR",  0xF022, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Flash memory power control register", 0),
MM_REG8("EBR1",    0xF023, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Erase block register 1", 0),
MM_REG8("FENR",    0xF02B, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Flash memory enable register", 0),
MM_REG8("RTCFLG",  0xF067, REGTYPE_DBW8_ACCS2,  rtc_get_flg, rtc_set_flg, "RTC interrupt flag register", offsetof(pw_context_t, rtc)),
MM_REG8("RSECDR",  0xF068, REGTYPE_DBW8_ACCS2,  rtc_get_secdr, rtc_set_secdr, "Second data register/free running counter data register", offsetof(pw_context_t, rtc)),
MM_REG8("RMINDR",  0xF069, REGTYPE_DBW8_ACCS2,  rtc_get_mindr, rtc_set_mindr, "Minute data register", offsetof(pw_context_t, rtc)),
MM_REG8("RHRDR",   0xF06A, REGTYPE_DBW8_ACCS2,  rtc_get_hrdr, rtc_set_hrdr, "Hour data register", offsetof(pw_context_t, rtc)),
MM_REG8("RWKDR",   0xF06B, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Day-of-week data register", offsetof(pw_context_t, rtc)),
MM_REG8("RTCCR1",  0xF06C, REGTYPE_DBW8_ACCS2,  rtc_get_cr1, rtc_set_cr1, "RTC control register 1", offsetof(pw_context_t, rtc)),
MM_REG8("RTCCR2",  0xF06D, REGTYPE_DBW8_ACCS2,  rtc_get_cr2, rtc_set_cr2, "RTC control register 2", offsetof(pw_context_t, rtc)),
MM_REG8("RTCCSR",  0xF06F, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Clock source select register", offsetof(pw_context_t, rtc)),
MM_REG8("ICCR1",   0xF078, REGTYPE_DBW8_ACCS2,  NULL, NULL, "I2C bus control register 1", 0),
MM_REG8("ICCR2",   0xF079, REGTYPE_DBW8_ACCS2,  NULL, NULL, "I2C bus control register 2", 0),
MM_REG8("ICMR",    0xF07A, REGTYPE_DBW8_ACCS2,  NULL, NULL, "I2C bus mode register", 0),
MM_REG8("ICIER",   0xF07B, REGTYPE_DBW8_ACCS2,  NULL, NULL, "I2C bus interrupt enable register", 0),
MM_REG8("ICSR",    0xF07C, REGTYPE_DBW8_ACCS2,  NULL, NULL, "I2C bus status register", 0),
MM_REG8("SAR",     0xF07D, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Slave address register", 0),
MM_REG8("ICDRT",   0xF07E, REGTYPE_DBW8_ACCS2,  NULL, NULL, "I2C bus transmit data register", 0),
MM_REG8("ICDRR",   0xF07F, REGTYPE_DBW8_ACCS2,  NULL, NULL, "I2C bus receive data register", 0),
MM_REG8("PFCR",    0xF085, REGTYPE_DBW8_ACCS2,  sys_get_pfcr, sys_set_pfcr, "Port function control register", 0),
MM_REG8("PUCR8",   0xF086, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Port pull-up control register 8", 0),
MM_REG8("PUCR9",   0xF087, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Port pull-up control register 9", 0),
MM_REG8("UNK_REG", 0xF088, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Unknown IO Reg (TODO?)", 0),
MM_REG8("PODR9",   0xF08C, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Port open-drain control register 9", 0),
MM_REG8("TMB1",    0xF0D0, REGTYPE_DBW8_ACCS2,  tb1_get_tmb1, tb1_set_tmb1, "Timer mode register B1", 0),
MM_REG8("TCB1 (R)/TLB1 (W)", 0xF0D1, REGTYPE_DBW8_ACCS2,  tb1_get_tcb1, tb1_set_tlb1, "Timer counter B1/Timer load register B1", 0),
MM_REG8("CMCR0",   0xF0DC, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Compare control register 0", 0),
MM_REG8("CMCR1",   0xF0DD, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Compare control register 1", 0),
MM_REG8("CMDR",    0xF0DE, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Compare data register", 0),
MM_REG8("SSCRH",   0xF0E0, REGTYPE_DBW8_ACCS3,  ssu_get_sscrh, ssu_set_sscrh, "SS control register H", offsetof(pw_context_t, ssu)),
MM_REG8("SSCRL",   0xF0E1, REGTYPE_DBW8_ACCS3,  ssu_get_sscrl, ssu_set_sscrl, "SS control register L", offsetof(pw_context_t, ssu)),
MM_REG8("SSMR",    0xF0E2, REGTYPE_DBW8_ACCS3,  ssu_get_ssmr, ssu_set_ssmr, "SS mode register", offsetof(pw_context_t, ssu)),
MM_REG8("SSER",    0xF0E3, REGTYPE_DBW8_ACCS3,  ssu_get_sser, ssu_set_sser, "SS enable register", offsetof(pw_context_t, ssu)),
MM_REG8("SSSR",    0xF0E4, REGTYPE_DBW8_ACCS3,  ssu_get_sssr, ssu_set_sssr, "SS status register", offsetof(pw_context_t, ssu)),
MM_REG8("SSRDR",   0xF0E9, REGTYPE_DBW8_ACCS3,  ssu_get_ssrdr, NULL, "SS receive data register", offsetof(pw_context_t, ssu)),
MM_REG8("SSTDR",   0xF0EB, REGTYPE_DBW8_ACCS3,  ssu_get_sstdr, ssu_set_sstdr, "SS transmit data register", offsetof(pw_context_t, ssu)),
MM_REG8("TMRW",    0xF0F0, REGTYPE_DBW8_ACCS2,  tmrw_get_tmrw, tmrw_set_tmrw, "Timer mode register W", 0),
MM_REG8("TCRW",    0xF0F1, REGTYPE_DBW8_ACCS2,  tmrw_get_tcrw, tmrw_set_tcrw, "Timer control register W", 0),
MM_REG8("TIERW",   0xF0F2, REGTYPE_DBW8_ACCS2,  tmrw_get_tierw, tmrw_set_tierw, "Timer interrupt enable register W", 0),
MM_REG8("TSRW",    0xF0F3, REGTYPE_DBW8_ACCS2,  tmrw_get_tsrw, tmrw_set_tsrw, "Timer status register W", 0),
MM_REG8("TIOR0",   0xF0F4, REGTYPE_DBW8_ACCS2,  tmrw_get_tior0, tmrw_set_tior0, "Timer I/O control register 0", 0),
MM_REG8("TIOR1",   0xF0F5, REGTYPE_DBW8_ACCS2,  tmrw_get_tior1, tmrw_set_tior1, "Timer I/O control register 1", 0),
MM_REG16("TCNT",   0xF0F6, REGTYPE_DBW16_ACCS2, tmrw_get_tcnt, tmrw_set_tcnt, "Timer counter", 0),
MM_REG16("GRA",    0xF0F8, REGTYPE_DBW16_ACCS2, tmrw_get_gra, tmrw_set_gra, "General register A", 0),
MM_REG16("GRB",    0xF0FA, REGTYPE_DBW16_ACCS2, tmrw_get_grb, tmrw_set_grb, "General register B", 0),
MM_REG16("GRC",    0xF0FC, REGTYPE_DBW16_ACCS2, tmrw_get_grc, tmrw_set_grc, "General register C", 0),
MM_REG16("GRD",    0xF0FE, REGTYPE_DBW16_ACCS2, tmrw_get_grd, tmrw_set_grd, "General register D", 0),
MM_REG16("ECPWCR", 0xFF8C, REGTYPE_DBW16_ACCS2, NULL, NULL, "Event counter PWM compare register", 0),
MM_REG16("ECPWDR", 0xFF8E, REGTYPE_DBW16_ACCS2, NULL, NULL, "Event counter PWM data register", 0),
MM_REG8("SPCR",    0xFF91, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Serial port control register", 0),
MM_REG8("AEGSR",   0xFF92, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Input pin edge select register", 0),
MM_REG8("ECCR",    0xFF94, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Event counter control register", 0),
MM_REG8("ECCSR",   0xFF95, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Event counter control/status register", 0),
MM_REG8("ECH",     0xFF96, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Event counter H", 0),
MM_REG8("ECL",     0xFF97, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Event counter L", 0),
MM_REG8("SMR3",    0xFF98, REGTYPE_DBW8_ACCS3,  sci_get_smr, sci_set_smr, "Serial mode register 3", 0),
MM_REG8("BRR3",    0xFF99, REGTYPE_DBW8_ACCS3,  NULL, NULL, "Bit rate register 3", 0),
MM_REG8("SCR3",    0xFF9A, REGTYPE_DBW8_ACCS3,  sci_get_scr, sci_set_scr, "Serial control register 3", 0),
MM_REG8("TDR3",    0xFF9B, REGTYPE_DBW8_ACCS3,  sci_get_tdr, sci_set_tdr, "Transmit data register 3", 0),
MM_REG8("SSR3",    0xFF9C, REGTYPE_DBW8_ACCS3,  sci_get_ssr, sci_set_ssr, "Serial status register 3", 0),
MM_REG8("RDR3",    0xFF9D, REGTYPE_DBW8_ACCS3,  sci_get_rdr, NULL, "Receive data register 3", 0),
MM_REG8("SEMR",    0xFFA6, REGTYPE_DBW8_ACCS3,  sci_get_semr, sci_set_semr, "Serial extended mode register", 0),
MM_REG8("IrCR",    0xFFA7, REGTYPE_DBW8_ACCS2,  sci_get_ircr, sci_set_ircr, "IrDA control register", 0),
MM_REG8("TMWD",    0xFFB0, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Timer mode register WD", 0),
MM_REG8("TCSRWD1", 0xFFB1, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Timer control/status register WD1", 0),
MM_REG8("TCSRWD2", 0xFFB2, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Timer control/status register WD2", 0),
MM_REG8("TCWD",    0xFFB3, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Timer counter WD", 0),
MM_REG16("ADRR",   0xFFBC, REGTYPE_DBW16_ACCS2, adc_get_addr, NULL, "A/D result register", 0),
MM_REG8("AMR",     0xFFBE, REGTYPE_DBW8_ACCS2,  NULL, NULL, "A/D mode register", 0),
MM_REG8("ADSR",    0xFFBF, REGTYPE_DBW8_ACCS2,  NULL, NULL, "A/D start register", 0),
MM_REG8("PMR1",    0xFFC0, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Port mode register 1", 0),
MM_REG8("PMR3",    0xFFC2, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Port mode register 3", 0),
MM_REG8("PMRB",    0xFFCA, REGTYPE_DBW8_ACCS2,  portb_get_pmrb, portb_set_pmrb, "Port mode register B", offsetof(pw_context_t, portb)),
MM_REG8("PDR1",    0xFFD4, REGTYPE_DBW8_ACCS2,  io2_get_pdr1, io2_set_pdr1, "Port data register 1", 0),
MM_REG8("PDR3",    0xFFD6, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Port data register 3", 0),
MM_REG8("PDR8",    0xFFDB, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Port data register 8", 0),
MM_REG8("PDR9",    0xFFDC, REGTYPE_DBW8_ACCS2,  io2_get_pdr9, io2_set_pdr9, "Port data register 9", 0),
MM_REG8("PDRB",    0xFFDE, REGTYPE_DBW8_ACCS2,  portb_get_pdrb, NULL, "Port data register B", offsetof(pw_context_t, portb)),
MM_REG8("PUCR1",   0xFFE0, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Port pull-up control register 1", 0),
MM_REG8("PUCR3",   0xFFE1, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Port pull-up control register 3", 0),
MM_REG8("PCR1",    0xFFE4, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Port control register 1", 0),
MM_REG8("PCR3",    0xFFE6, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Port control register 3", 0),
MM_REG8("PCR8",    0xFFEB, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Port control register 8", 0),
MM_REG8("PCR9",    0xFFEC, REGTYPE_DBW8_ACCS2,  NULL, NULL, "Port control register 9", 0),
MM_REG8("SYSCR1",  0xFFF0, REGTYPE_DBW8_ACCS2,  sys_get_syscr1, sys_set_syscr1, "System control register 1", 0),
MM_REG8("SYSCR2",  0xFFF1, REGTYPE_DBW8_ACCS2,  sys_get_syscr2, sys_set_syscr2, "System control register 2", 0),
MM_REG8("IEGR",    0xFFF2, REGTYPE_DBW8_ACCS2,  int_get_iegr, int_set_iegr, "Interrupt edge select register", 0),
MM_REG8("IENR1",   0xFFF3, REGTYPE_DBW8_ACCS2,  int_get_ienr1, int_set_ienr1, "Interrupt enable register 1", 0),
MM_REG8("IENR2",   0xFFF4, REGTYPE_DBW8_ACCS2,  int_get_ienr2, int_set_ienr2, "Interrupt enable register 2", 0),
MM_REG8("OSCCR",   0xFFF5, REGTYPE_DBW8_ACCS2,  sys_get_osccr, sys_set_osccr, "Oscillator control register", 0),
MM_REG8("IRR1",    0xFFF6, REGTYPE_DBW8_ACCS2,  int_get_irr1, int_set_irr1, "Interrupt flag register 1", 0),
MM_REG8("IRR2",    0xFFF7, REGTYPE_DBW8_ACCS2,  int_get_irr2, int_set_irr2, "Interrupt flag register 2", 0),
MM_REG8("CKSTPR1", 0xFFFA, REGTYPE_DBW8_ACCS2,  sys_get_ckstpr1, sys_set_ckstpr1, "Clock stop register 1", 0),
MM_REG8("CKSTPR2", 0xFFFB, REGTYPE_DBW8_ACCS2,  sys_get_ckstpr2, sys_set_ckstpr2, "Clock stop register 2", 0),
};

mm_reg_t *find_mm_reg(uint16_t addr, int include_unaligned) {
    include_unaligned = 1;
    static uint16_t cached_addr = 0;
    static mm_reg_t *cached_reg = NULL;

    if (cached_addr == addr) {
        return cached_reg;
    }

    int a = 0;
    int b = sizeof(mm_registers) / sizeof(mm_registers[0]);
    while (a <= b) {
        int g = a + (b - a) / 2;
        mm_reg_t *reg = &mm_registers[g];
        if (reg->addr == addr) {
            cached_addr = addr;
            cached_reg = reg;
            return reg;
        } else if (addr < reg->addr) {
            b = g - 1;
        } else {
            a = g + 1;
        }
    }
    if (include_unaligned && likely(a > 0)) {
        mm_reg_t *prev_reg = &mm_registers[a-1];
        if (likely(prev_reg->type == REGTYPE_DBW16_ACCS2 && prev_reg->addr == addr-1)) {
            return prev_reg;
        }
    }
    return NULL;
}

enum grayscale {
    WHITE, LIGHT_GRAY, DARK_GRAY, BLACK
};

enum keys {
    KEY_ENTER = 2,
    KEY_LEFT  = 4,
    KEY_RIGHT = 8
};

#define RGBA(r,g,b,a) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))
int32_t colors[] = {
    RGBA(172, 174, 163, 255),
    RGBA(128, 130, 116, 255),
    RGBA(92, 92, 82, 255),
    RGBA(30, 25, 22, 255)
};

void fill_audio(void* userdata, uint8_t* stream, int len) {
    for (int i = 0; i < len; i++) {
        stream[i] = i % 50;
    }
}

static int sdl_init() {
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#ifdef __EMSCRIPTEN__
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
#endif
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }
    gWindow = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN );
    if (gWindow == NULL) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }
    // Create renderer
    renderer = SDL_CreateRenderer(gWindow, -1, 0);
    SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Create texture that stores frame buffer
    sdlTexture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH, SCREEN_HEIGHT);
    return 1;
}

static void sdl_quit() {
	SDL_DestroyWindow(gWindow);
	gWindow = NULL;
	SDL_Quit();
}

static void sdl_draw(lcd_t *lcd) {
    // int32_t palette[4];
    // {
    //     int32_t white = colors[0];
    //     int wr = (white >> 16) & 0xFF;
    //     int wg = (white >> 8) & 0xFF;
    //     int wb = white & 0xFF;
    //     palette[0] = white;
    //     for (int i = 1; i < 4; i++) {
    //         int32_t col = colors[i];
    //         int r = (col >> 16) & 0xFF;
    //         int g = (col >> 8) & 0xFF;
    //         int b = col & 0xFF;
    //         palette[i] = RGBA(
    //             wr + (int) ((r - wr) * (lcd.contrast / 32.0)), 
    //             wg + (int) ((g - wg) * (lcd.contrast / 32.0)), 
    //             wb + (int) ((b - wb) * (lcd.contrast / 32.0)), 
    //             255
    //         );
    //     }
    // }
    int pitch = SCREEN_WIDTH * sizeof(uint32_t);
    uint32_t *screen;
    SDL_LockTexture(sdlTexture, NULL, (void**) &screen, &pitch);
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            int yo  = y + lcd->display_start_line;
            int pg  = (yo / 8) % LCD_NUM_PAGES;
            int b   = yo % 8;
            int col = x;
            uint16_t byte = (lcd->ram[pg][col*2] << 8) | lcd->ram[pg][col*2+1];
            byte >>= b;
            uint8_t color = ((byte & 0x100) >> 7) | (byte & 1);
            screen[y*SCREEN_WIDTH+x] = colors[color];
        }
    }
    // for (int pg = 0; pg < LCD_NUM_PAGES; pg++) {
    //     for (int col = 0; col < SCREEN_WIDTH; col++) {
    //         uint16_t byte  = (lcd.ram[pg][col*2] << 8) | lcd.ram[pg][col*2+1];
    //         for (int b = 0; b < 8; b++) {
    //             uint8_t color = ((byte & 0x100) >> 7) | (byte & 1);
    //             int offset = (pg*8+b) * SCREEN_WIDTH+col;
    //             if (offset > 0 && offset < SCREEN_HEIGHT * SCREEN_WIDTH) {
    //                 screen[offset] = colors[color];
    //             }
    //             byte >>= 1;
    //         }
    //     }
    // }
    SDL_UnlockTexture(sdlTexture);
    SDL_RenderCopy(renderer, sdlTexture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

char branch_mnemonics[16][4] = {
    "bra", "brn", "bhi", "bls", "bcc", "bcs", "bne", "beq",
    "bvc", "bvs", "bpl", "bmi", "bge", "blt", "bgt", "ble"
};

static uint32_t sign16_32(uint16_t val) {
    if (val & (1 << 15)) {
        return val | 0xFFFF0000;
    } else {
        return val;
    }
}

static uint32_t sign8_32(uint8_t val) {
    if (val & (1 << 7)) {
        return val | 0xFFFFFF00;
    } else {
        return val;
    }
}

static uint16_t sign8_16(uint8_t val) {
    if (val & (1 << 7)) {
        return val | 0xFF00;
    } else {
        return val;
    }
}

#define ABS8(x)  (0xFFFF00 | x)
#define ABS16(x) x
//#define ABS16(x) sign16_32(x)
#define ABS24(x) x
#define DISPI32(disp, reg) (read_reg32(ctx, reg) + (int32_t)disp)
#define DISPI16(disp, reg) (read_reg32(ctx, reg) + (int32_t)sign16_32(disp))

enum ccr_bit { 
    CCR_C, CCR_V, CCR_Z, CCR_N, CCR_U1,
    CCR_H, CCR_U2, CCR_I
};

enum reg32 {
    ER0, ER1, ER2, ER3, ER4, ER5, ER6, ER7
};

#define ER_SP ER7

enum reg16 {
    R0, R1, R2, R3, R4, R5, R6, R7,
    E0, E1, E2, E3, E4, E5, E6, E7
};

enum reg8 {
    R0H, R1H, R2H, R3H, R4H, R5H, R6H, R7H,
    R0L, R1L, R2L, R3L, R4L, R5L, R6L, R7L
};

static uint32_t read_reg32(pw_context_t *ctx, uint32_t reg) {
    assert(reg < 8);
    return (ctx->regs[reg+8] << 16) | ctx->regs[reg];
}

static uint16_t read_reg16(pw_context_t *ctx, uint32_t reg) {
    assert(reg < 16);
    return ctx->regs[reg];
}

static uint8_t read_reg8(pw_context_t *ctx, uint32_t reg) {
    assert(reg < 16);
    return ctx->regs[reg & 0b111] >> ((reg & 8) ? 0 : 8);
}

static void write_reg32(pw_context_t *ctx, uint32_t reg, uint32_t value) {
    assert(reg < 8);
    ctx->regs[reg+8] = value >> 16;
    ctx->regs[reg]   = value;
}

static void write_reg16(pw_context_t *ctx, uint32_t reg, uint16_t value) {
    assert(reg < 16);
    ctx->regs[reg] = value;
}

static void write_reg8(pw_context_t *ctx, uint32_t reg, uint8_t value) {
    assert(reg < 16);
    int shift = (reg & 8) ? 0 : 8;
    ctx->regs[reg & 0b111] &= 0xFF  << (8 - shift);
    ctx->regs[reg & 0b111] |= value << shift;
}

static uint8_t get_ccr(pw_context_t *ctx) {
    return ctx->ccr;
}

static void set_ccr(pw_context_t *ctx, uint8_t nccr) {
    ctx->ccr = nccr & 0xFF;
}

static int get_ccr_bit(pw_context_t *ctx, enum ccr_bit bit) {
    return (ctx->ccr >> bit) & 1;
}

static void set_ccr_bit(pw_context_t *ctx, enum ccr_bit bit, int x) {
    ctx->ccr ^= (-x ^ ctx->ccr) & (1UL << bit);
}

static uint8_t peek8(pw_context_t *ctx, uint16_t addr) {
    addr &= 0xFFFF;
    if (addr <= ROM_end) {
        return ctx->rom[addr];
    } else if (addr >= RAM_start && addr < RAM_end) {
        return ctx->ram[addr - RAM_start];
    }
    return 0;
}

static uint16_t peek16(pw_context_t *ctx, uint32_t addr) {
    addr &= 0xFFFF;
    if (addr <= ROM_end - 1) {
        return (ctx->rom[addr] << 8) | ctx->rom[addr+1];
    } else if (addr >= RAM_start && addr < RAM_end - 1) {
        return (ctx->ram[addr - RAM_start] << 8) | ctx->ram[addr - RAM_start + 1];
    }
    return 0;
}

static uint32_t peek24(pw_context_t *ctx, uint32_t addr) {
    return (peek16(ctx, addr) << 8) | peek8(ctx, addr+2);
}

static uint32_t peek32(pw_context_t *ctx, uint32_t addr) {
    return (peek16(ctx, addr) << 16) | peek16(ctx, addr+2);
}

#define INTERNAL_STATES(i) ctx->internal_states += i; ctx->states += i;
#define ON_CHIP_MEM_ACCESS ctx->states += 2;
#define ON_CHIP_MOD8_2_ACCESS ctx->states += 2;
#define ON_CHIP_MOD8_3_ACCESS ctx->states += 3;
#define ON_CHIP_MOD16_2_ACCESS ctx->states += 2;

static uint8_t read8(pw_context_t *ctx, uint16_t addr) {
    ctx->byte_access++;
    addr &= 0xFFFF;
    if (addr <= ROM_end) {
        ON_CHIP_MEM_ACCESS;
        return ctx->rom[addr];
    } else if (addr >= RAM_start && addr < RAM_end) {
        ON_CHIP_MEM_ACCESS;
        // if (addr == 0xF7C4) {
        //     // hack to get into main event loop
        //     return 1;
        // }
        if (addr == 0xF7B5) { // common_bit_flags
            return (ctx->ram[addr - RAM_start] | 1UL) & ~(1 << 4);
        }
        return ctx->ram[addr - RAM_start];
    } else if ((addr >= IO1_start && addr < IO1_end) || (addr >= IO2_start && addr < IO2_end)) {
        mm_reg_t *reg = find_mm_reg(addr, 0);
        if (reg && reg->type != REGTYPE_DBW16_ACCS2) {
            if (reg->read8) {
                if (reg->type == REGTYPE_DBW8_ACCS2) {
                    ON_CHIP_MOD8_2_ACCESS;
                } else {
                    ON_CHIP_MOD8_3_ACCESS;
                }
                return reg->read8(((uintptr_t)ctx) + reg->ctx_offset);
            }
            UNIMPL("read8: unimplemented register %s [%x]\n", reg->name, ctx->ip);
            return 0;
        }
    }
    debug("read8: Unknown addr %x\n", addr);
    exit(0);
}

static uint16_t read16(pw_context_t *ctx, uint32_t addr) {
    ctx->word_access++;
    addr &= 0xFFFF;
    if (addr <= ROM_end - 1) {
        ON_CHIP_MEM_ACCESS;
        return (ctx->rom[addr] << 8) | ctx->rom[addr+1];
    } else if (addr >= RAM_start && addr < RAM_end - 1) {
        ON_CHIP_MEM_ACCESS;
        if (addr == 0xF78E) { // watts
            return 132;
        }
        return (ctx->ram[addr - RAM_start] << 8) | ctx->ram[addr - RAM_start + 1];
    } else if ((addr >= IO1_start && addr < IO1_end - 1) || (addr >= IO2_start && addr < IO2_end - 1)) {
        mm_reg_t *reg = find_mm_reg(addr, 0);
        if (reg && reg->type == REGTYPE_DBW16_ACCS2) {
            if (reg->read16) {
                ON_CHIP_MOD16_2_ACCESS;
                return reg->read16(((uintptr_t)ctx) + reg->ctx_offset);
            }
            UNIMPL("read16: unimplemented register %s [%x]\n", reg->name, ctx->ip);
            return 0;
        }
    }
    debug("read16: Unknown addr %x\n", addr);
    exit(0);
}

static uint32_t read24(pw_context_t *ctx, uint32_t addr) {
    return (read8(ctx, addr) << 16) | (read8(ctx, addr+1) << 8) | read8(ctx, addr+2);
}

static uint32_t read32(pw_context_t *ctx, uint32_t addr) {
    return (read16(ctx, addr) << 16) | read16(ctx, addr+2);
}

static void write8(pw_context_t *ctx, uint16_t addr, uint8_t val) {
    ctx->byte_access++;
    addr = addr & 0xFFFF;
    if (addr >= RAM_start && addr < RAM_end) {
        ON_CHIP_MEM_ACCESS;
        ctx->ram[addr - RAM_start] = val;
        return;
    } else if ((addr >= IO1_start && addr < IO1_end) || (addr >= IO2_start && addr < IO2_end)) {
        mm_reg_t *reg = find_mm_reg(addr, 0);
        if (reg && reg->type != REGTYPE_DBW16_ACCS2) {
            if (reg->write8) {
                if (reg->type == REGTYPE_DBW8_ACCS2) {
                    ON_CHIP_MOD8_2_ACCESS;
                } else {
                    ON_CHIP_MOD8_3_ACCESS;
                }
                reg->write8(((uintptr_t)ctx) + reg->ctx_offset, val);
            } else {
                UNIMPL("write8: unimplemented register %s [%x]\n", reg->name, ctx->ip);
            }
            return;
        }
    }
    debug("write8: Unknown addr %x\n", addr);
    exit(0);
}

static void write16(pw_context_t *ctx, uint16_t addr, uint16_t val) {
    ctx->word_access++;
    addr = addr & 0xFFFF;
    if (addr >= RAM_start && addr < RAM_end - 1) {
        ON_CHIP_MEM_ACCESS;
        ctx->ram[addr - RAM_start] = val >> 8;
        ctx->ram[addr - RAM_start + 1] = val;
        return;
    } else if ((addr >= IO1_start && addr < IO1_end - 1) || (addr >= IO2_start && addr < IO2_end - 1)) {
        mm_reg_t *reg = find_mm_reg(addr, 0);
        if (reg && reg->type == REGTYPE_DBW16_ACCS2) {
            if (reg->write16) {
                ON_CHIP_MOD16_2_ACCESS;
                reg->write16(((uintptr_t)ctx) + reg->ctx_offset, val);
            } else {
                UNIMPL("write16: unimplemented register %s [%x]\n", reg->name, ctx->ip);
            }
            return;
        }
    }
    debug("write16: Unknown addr %x\n", addr);
    exit(0);
}

static void write32(pw_context_t *ctx, uint16_t addr, uint32_t val) {
    write16(ctx, addr, val >> 16);
    write16(ctx, addr+2, val & 0xFFFF);
}

char ccr_names[8] = "CVZNUHUI";
static void print_state(pw_context_t *ctx) {
    debug("=== ");
    for (int i = 0; i < 8; i++) {
        debug("er%d:%.8x ", i, read_reg32(ctx, i));
    }
    debug("[");
    for (int i = 0; i < 8; i++) {
        debug(get_ccr_bit(ctx, i) ? "%c" : " ", ccr_names[i]);
    }
    debug("] STK:%.8x {B:%d W:%d I:%d}\n", peek32(ctx, read_reg32(ctx, ER_SP)-4), ctx->byte_access, ctx->word_access, ctx->internal_states);
}

// INSNS

static uint8_t rotlb(pw_context_t *ctx, uint8_t val) {
    uint8_t res = (val << 1) & (val >> 7);
    set_ccr_bit(ctx, CCR_N, res >> 7);
    set_ccr_bit(ctx, CCR_Z, res == 0);
    set_ccr_bit(ctx, CCR_V, 0);
    set_ccr_bit(ctx, CCR_C, val >> 7);
    return res;
}

static uint16_t rotlw(pw_context_t *ctx, uint16_t val) {
    uint8_t res = (val << 1) & (val >> 15);
    set_ccr_bit(ctx, CCR_N, res >> 15);
    set_ccr_bit(ctx, CCR_Z, res == 0);
    set_ccr_bit(ctx, CCR_V, 0);
    set_ccr_bit(ctx, CCR_C, val >> 15);
    return res;
}

static uint16_t negw(pw_context_t *pw, uint16_t val) {
    // FIXME
    return ~val + 1;
}

static uint8_t negb(pw_context_t *ctx, uint8_t val) {
    // FIXME
    return ~val + 1;
}

// FIXME overflow for arithmetic shifts
// static uint8_t shalb(uint8_t val) {
//     uint8_t res = val << 1;
//     set_ccr_bit(ctx, CCR_N, val >> 7);
//     set_ccr_bit(ctx, CCR_Z, res == 0);
//     set_ccr_bit(ctx, CCR_V, 0);
//     set_ccr_bit(ctx, CCR_C, res >> 7);
//     return res;
// }

// static uint16_t shalw(uint16_t val) {
//     uint16_t res = val << 1;
//     set_ccr_bit(ctx, CCR_N, val >> 15);
//     set_ccr_bit(ctx, CCR_Z, res == 0);
//     set_ccr_bit(ctx, CCR_V, 0);
//     set_ccr_bit(ctx, CCR_C, res >> 15);
//     return res;
// }

// static uint32_t shall(uint32_t val) {
//     uint32_t res = val << 1;
//     set_ccr_bit(ctx, CCR_N, val >> 31);
//     set_ccr_bit(ctx, CCR_Z, res == 0);
//     set_ccr_bit(ctx, CCR_V, 0);
//     set_ccr_bit(ctx, CCR_C, res >> 31);
//     return res;
// }

// static uint8_t sharb(uint8_t val) {
//     uint8_t res = (int8_t)val >> 1;
//     set_ccr_bit(ctx, CCR_N, 0);
//     set_ccr_bit(ctx, CCR_Z, res == 0);
//     set_ccr_bit(ctx, CCR_V, 0);
//     set_ccr_bit(ctx, CCR_C, val & 1U);
//     return res;
// }

static uint16_t sharw(pw_context_t *ctx, uint16_t val) {
    uint16_t res = (int16_t)val >> 1;
    set_ccr_bit(ctx, CCR_N, 0);
    set_ccr_bit(ctx, CCR_Z, res == 0);
    set_ccr_bit(ctx, CCR_V, 0);
    set_ccr_bit(ctx, CCR_C, val & 1U);
    return res;
}

static uint32_t sharl(pw_context_t *ctx, uint32_t val) {
    uint32_t res = (int32_t)val >> 1;
    set_ccr_bit(ctx, CCR_N, 0);
    set_ccr_bit(ctx, CCR_Z, res == 0);
    set_ccr_bit(ctx, CCR_V, 0);
    set_ccr_bit(ctx, CCR_C, val & 1U);
    return res;
}

static uint8_t shllb(pw_context_t *ctx, uint8_t val) {
    uint8_t res = val << 1;
    set_ccr_bit(ctx, CCR_N, val >> 7);
    set_ccr_bit(ctx, CCR_Z, res == 0);
    set_ccr_bit(ctx, CCR_V, 0);
    set_ccr_bit(ctx, CCR_C, res >> 7);
    return res;
}

static uint16_t shllw(pw_context_t *ctx, uint16_t val) {
    uint16_t res = val << 1;
    set_ccr_bit(ctx, CCR_N, val >> 15);
    set_ccr_bit(ctx, CCR_Z, res == 0);
    set_ccr_bit(ctx, CCR_V, 0);
    set_ccr_bit(ctx, CCR_C, res >> 15);
    return res;
}

static uint32_t shlll(pw_context_t *ctx, uint32_t val) {
    uint32_t res = val << 1;
    set_ccr_bit(ctx, CCR_N, val >> 31);
    set_ccr_bit(ctx, CCR_Z, res == 0);
    set_ccr_bit(ctx, CCR_V, 0);
    set_ccr_bit(ctx, CCR_C, res >> 31);
    return res;
}

static uint8_t shlrb(pw_context_t *ctx, uint8_t val) {
    uint8_t res = val >> 1;
    set_ccr_bit(ctx, CCR_N, 0);
    set_ccr_bit(ctx, CCR_Z, res == 0);
    set_ccr_bit(ctx, CCR_V, 0);
    set_ccr_bit(ctx, CCR_C, val & 1U);
    return res;
}

static uint16_t shlrw(pw_context_t *ctx, uint16_t val) {
    uint16_t res = val >> 1;
    set_ccr_bit(ctx, CCR_N, 0);
    set_ccr_bit(ctx, CCR_Z, res == 0);
    set_ccr_bit(ctx, CCR_V, 0);
    set_ccr_bit(ctx, CCR_C, val & 1U);
    return res;
}

static uint32_t shlrl(pw_context_t *ctx, uint32_t val) {
    uint32_t res = val >> 1;
    set_ccr_bit(ctx, CCR_N, 0);
    set_ccr_bit(ctx, CCR_Z, res == 0);
    set_ccr_bit(ctx, CCR_V, 0);
    set_ccr_bit(ctx, CCR_C, val & 1U);
    return res;
}

static uint8_t incb(pw_context_t *ctx, uint8_t val) {
    return val + 1;
}

static uint8_t bset(pw_context_t *ctx, uint8_t val, int shift) {
    return val | (1 << shift);
}

static uint8_t bclr(pw_context_t *ctx, uint8_t val, int shift) {
    return val & ~(1 << shift);
}

static uint8_t bst(pw_context_t *ctx, uint8_t val, int shift) {
    return (val & ~(1 << shift)) | (get_ccr_bit(ctx, CCR_C) << shift);
}

static uint8_t bnot(pw_context_t *ctx, uint8_t val, int shift) {
    return val ^ (1 << shift);
}

static uint8_t movb(pw_context_t *ctx, uint8_t ignore, uint8_t val) {
    set_ccr_bit(ctx, CCR_N, (int8_t)val < 0);
    set_ccr_bit(ctx, CCR_Z, val == 0);
    set_ccr_bit(ctx, CCR_V, 0);
    return val;
}

static uint16_t movw(pw_context_t *ctx, uint16_t ignore, uint16_t val) {
    set_ccr_bit(ctx, CCR_N, (int16_t)val < 0);
    set_ccr_bit(ctx, CCR_Z, val == 0);
    set_ccr_bit(ctx, CCR_V, 0);
    return val;
}

static uint32_t movl(pw_context_t *ctx, uint32_t ignore, uint32_t val) {
    set_ccr_bit(ctx, CCR_N, (int32_t)val < 0);
    set_ccr_bit(ctx, CCR_Z, val == 0);
    set_ccr_bit(ctx, CCR_V, 0);
    return val;
}

static uint8_t notb(pw_context_t *ctx, uint8_t val) {
    return movb(ctx, 0, ~val);
}

static uint8_t orb(pw_context_t *ctx, uint8_t a, uint8_t b) {
    return movb(ctx, 0, a | b);
}

static uint8_t andb(pw_context_t *ctx, uint8_t a, uint8_t b) {
    return movb(ctx, 0, a & b);
}

static uint8_t xorb(pw_context_t *ctx, uint8_t a, uint8_t b) {
    return movb(ctx, 0, a ^ b);
}

static uint16_t notw(pw_context_t *ctx, uint16_t val) {
    return movw(ctx, 0, ~val);
}

static uint16_t orw(pw_context_t *ctx, uint16_t a, uint16_t b) {
    return movw(ctx, 0, a | b);
}

static uint16_t andw(pw_context_t *ctx, uint16_t a, uint16_t b) {
    return movw(ctx, 0, a & b);
}

static uint32_t andl(pw_context_t *ctx, uint32_t a, uint32_t b) {
    return movl(ctx, 0, a & b);
}

static uint16_t xorw(pw_context_t *ctx, uint16_t a, uint16_t b) {
    return movw(ctx, 0, a ^ b);
}

// static uint16_t incw_flags(uint16_t val) {
//     // FIXME
//     return val;
// }

static uint16_t extsw(pw_context_t *pw, uint8_t val) {
    return sign8_16(val & 0xFF);
}

static uint32_t extsl(pw_context_t *pw, uint16_t val) {
    return sign16_32(val & 0xFFFF);
}

static uint8_t decb_flags(pw_context_t *ctx, uint8_t val) {
    set_ccr_bit(ctx, CCR_N, (int8_t)val < 0);
    set_ccr_bit(ctx, CCR_Z, val == 0);
    set_ccr_bit(ctx, CCR_V, val + 1 == 0x80);
    return val;
}

static uint8_t decb(pw_context_t *ctx, uint8_t val) {
    return decb_flags(ctx, val-1);
}

static uint16_t decw(pw_context_t *ctx, uint16_t val, uint16_t dec) {
    uint16_t res = val - dec;
    set_ccr_bit(ctx, CCR_N, (int16_t)res < 0);
    set_ccr_bit(ctx, CCR_Z, val == 0);
    set_ccr_bit(ctx, CCR_V, 0); // FIXME
    return res;
}

// FIXME
#define CARRY_AT(a, b, n)  (((a & ((1<<n)-1)) + (b & ((1<<n)-1))) > ((1<<n)-1))
#define BORROW_AT(a, b, n) (b > a)
#define OVERFLOW_SUB(a, b, r, sign_bit) (((a ^ b)&(a ^ r)) >> sign_bit)
#define OVERFLOW_ADD(a, b, r, sign_bit) (((~(a ^ b))&(a ^ r)) >> sign_bit)

static int branch_condition(pw_context_t *ctx, uint32_t cc) {
    switch(cc) {
        case 0: // BRA
            return 1;
        case 1: // BRN
            return 0;
        case 2: // BHI
            return !(get_ccr_bit(ctx, CCR_C) || get_ccr_bit(ctx, CCR_Z));
        case 3: // BLS
            return get_ccr_bit(ctx, CCR_C) || get_ccr_bit(ctx, CCR_Z);
        case 4: // BCC
            return !get_ccr_bit(ctx, CCR_C); 
        case 5: // BCS
            return get_ccr_bit(ctx, CCR_C);
        case 6: // BNE
            return !get_ccr_bit(ctx, CCR_Z); 
        case 7: // BEQ
            return get_ccr_bit(ctx, CCR_Z);
        case 8: // BVC
            return !get_ccr_bit(ctx, CCR_V); 
        case 9: // BVS
            return get_ccr_bit(ctx, CCR_V);
        case 0xA: // BPL
            return !get_ccr_bit(ctx, CCR_N); 
        case 0xB: // BMI
            return get_ccr_bit(ctx, CCR_N);
        case 0xC: // BGE
            return !(get_ccr_bit(ctx, CCR_N) ^ get_ccr_bit(ctx, CCR_V)); 
        case 0xD: // BLT
            return get_ccr_bit(ctx, CCR_N) ^ get_ccr_bit(ctx, CCR_V);        
        case 0xE: // BGT
            return !(get_ccr_bit(ctx, CCR_Z) || (get_ccr_bit(ctx, CCR_N) ^ get_ccr_bit(ctx, CCR_V))); 
        case 0xF: // BLE
            return get_ccr_bit(ctx, CCR_Z) || (get_ccr_bit(ctx, CCR_N) ^ get_ccr_bit(ctx, CCR_V));
        default:
#ifndef _MSC_VER
    __builtin_unreachable();
#endif // !_MSC_VER
            assert(0);
    }
}

static void print_cmp(pw_context_t *ctx) {
    for (int i = 0; i < 16; i++) {
        debug("%s:%d ", branch_mnemonics[i], branch_condition(ctx, i));
    }
    debug("\n");
}

static uint8_t addx(pw_context_t *ctx, uint8_t a, uint8_t b) {
    uint8_t val = a + b + get_ccr_bit(ctx, CCR_C);
    // TODO FLAGS
    return val;
}

static uint8_t subx(pw_context_t *ctx, uint8_t a, uint8_t b) {
    uint8_t val = a - b - get_ccr_bit(ctx, CCR_C);
    // TODO FLAGS
    return val;
}

static uint8_t subb(pw_context_t *ctx, uint8_t a, uint8_t b) {
    uint8_t val = a - b;
    //debug("CMP8 %x %d %u, %x %d %u\n", b, (int8_t)b, b, a, (int8_t)a, a);
    set_ccr_bit(ctx, CCR_H, BORROW_AT(a, b, 3));
    set_ccr_bit(ctx, CCR_N, val >> 7);
    set_ccr_bit(ctx, CCR_Z, val == 0);
    set_ccr_bit(ctx, CCR_V, OVERFLOW_SUB(a, b, val, 7));
    set_ccr_bit(ctx, CCR_C, BORROW_AT(a, b, 7));
    print_cmp(ctx);
    return val;
}

static uint16_t subw(pw_context_t *ctx, uint16_t a, uint16_t b) {
    uint16_t val = a - b;
    //debug("CMP16 %x %d %u, %x %d %u\n", b, (int16_t)b, b, a, (int16_t)a, a);
    set_ccr_bit(ctx, CCR_H, BORROW_AT(a, b, 11));
    set_ccr_bit(ctx, CCR_N, val >> 15);
    set_ccr_bit(ctx, CCR_Z, val == 0);
    set_ccr_bit(ctx, CCR_V, OVERFLOW_SUB(a, b, val, 15));
    set_ccr_bit(ctx, CCR_C, BORROW_AT(a, b, 15));
    print_cmp(ctx);
    return val;
}

static uint32_t subl(pw_context_t *ctx, uint32_t a, uint32_t b) {
    uint32_t val = a - b;
    //debug("CMP32 %x %d %u, %x %d %u\n", b, (int32_t)b, b, a, (int32_t)a, a);
    set_ccr_bit(ctx, CCR_H, BORROW_AT(a, b, 27));
    set_ccr_bit(ctx, CCR_N, val >> 31);
    set_ccr_bit(ctx, CCR_Z, val == 0);
    set_ccr_bit(ctx, CCR_V, OVERFLOW_SUB(a, b, val, 31));
    set_ccr_bit(ctx, CCR_C, BORROW_AT(a, b, 31));
    print_cmp(ctx);
    return val;
}

static uint32_t extu(pw_context_t *ctx, uint32_t val) {
    set_ccr_bit(ctx, CCR_N, 0);
    set_ccr_bit(ctx, CCR_Z, val == 0);
    set_ccr_bit(ctx, CCR_V, 0);
    return val;
}

static uint16_t extuw(pw_context_t *ctx, uint16_t val) {
    return extu(ctx, val & 0xFF);
}

static uint32_t extul(pw_context_t *ctx, uint16_t val) {
    return extu(ctx, val & 0xFFFF);
}

static uint8_t addb(pw_context_t *ctx, uint8_t a, uint8_t b) {
    // FIXME
    return a + b;
}

static uint16_t addw(pw_context_t *ctx, uint16_t a, uint16_t b) {
    // FIXME
    return a + b;
}

static uint32_t addl(pw_context_t *ctx, uint32_t a, uint32_t b) {
    // FIXME
    return a + b;
}

static uint16_t divxub(pw_context_t *ctx, uint16_t a, uint8_t b) {
    debug("DIVuB %u / %u \n", a, b);
    set_ccr_bit(ctx, CCR_N, a >> 7);
    set_ccr_bit(ctx, CCR_Z, b == 0);
    if (b == 0) {
        return 0;
    } else {
        uint8_t quotient = a / b;
        uint8_t remainder = a % b;
        return (remainder << 8) | quotient;
    }
}

static uint32_t divxuw(pw_context_t *ctx, uint32_t a, uint16_t b) {
    debug("DIVuW %u / %u \n", a, b);
    set_ccr_bit(ctx, CCR_N, a >> 7);
    set_ccr_bit(ctx, CCR_Z, b == 0);
    if (b == 0) {
        return 0;
    } else {
        uint16_t quotient = a / b;
        uint16_t remainder = a % b;
        return (remainder << 16) | quotient;
    }
}

// FIXME
static uint16_t divxsb(pw_context_t *ctx, uint16_t a, uint8_t b) {
    int16_t sa = a;
    int8_t sb = b;
    debug("DIVB %u / %u \n", sa, sb);
    set_ccr_bit(ctx, CCR_N, a >> 7);
    set_ccr_bit(ctx, CCR_Z, b == 0);
    if (b == 0) {
        return 0;
    } else {
        uint8_t quotient = sa / sb;
        uint8_t remainder = sa % sb;
        return (remainder << 8) | quotient;
    }
}

// FIXME
static uint32_t divxsw(pw_context_t *ctx, uint32_t a, uint16_t b) {
    int32_t sa = a;
    int16_t sb = b;
    debug("DIVW %u / %u \n", sa, sb);
    set_ccr_bit(ctx, CCR_N, a >> 7);
    set_ccr_bit(ctx, CCR_Z, b == 0);
    if (b == 0) {
        return 0;
    } else {
        uint16_t quotient = sa / sb;
        uint16_t remainder = sa % sb;
        return (remainder << 16) | quotient;
    }
}

static uint16_t mulxub(uint16_t a, uint8_t b) {
    return sign8_16(a) * b;
}

static uint32_t mulxuw(uint32_t a, uint16_t b) {
    return sign16_32(a) * b;
}

static uint16_t mulxsb(uint16_t a, uint8_t b) {
    return (int16_t)sign8_16(a) * (int8_t)b;
}

static uint32_t mulxsw(uint32_t a, uint16_t b) {
    return (int32_t)sign16_32(a) * (int16_t)b;
}

static void pushl(pw_context_t *ctx, uint32_t val) {
    uint32_t sp = read_reg32(ctx, ER_SP) - 4;
    write32(ctx, sp, val);
    write_reg32(ctx, ER_SP, sp);
}

static uint32_t popl(pw_context_t *ctx) {
    uint32_t sp = read_reg32(ctx, ER_SP);
    write_reg32(ctx, ER_SP, sp + 4);
    return read32(ctx, sp);
}

static void pushw(pw_context_t *ctx, uint16_t val) {
    uint32_t sp = read_reg32(ctx, ER_SP) - 2;
    write16(ctx, sp, val);
    write_reg32(ctx, ER_SP, sp);
}

static uint16_t popw(pw_context_t *ctx) {
    uint32_t sp = read_reg32(ctx, ER_SP);
    write_reg32(ctx, ER_SP, sp + 2);
    return read16(ctx, sp);
}

#define PUSHIP pushw
#define POPIP  popw

static void branch(pw_context_t *ctx, uint32_t br_addr, uint32_t ret) {
    br_addr &= 0xFFFF;
    debug("BRANCH TO %x\n", br_addr);
    //switch(br_addr) {
        // case 0x0822: // irTxByte
        //     {
        //         uint8_t byt = read_reg8(ctx, R0L);
        //         debug("IR TX: %x\n", byt);
        //         ctx->ip = ret;
        //         return;
        //     }
        // case 0x369C: // is_F7C4_nonzero
        //     write_reg8(ctx, R0L, 0);
        //     ctx->ip = ret;
        //     ctx->word_access++;
        //     return;
        // case 0xB924:
        //     debug("bitfield %x val:%u bits:%u off:%u\n",
        //         read_reg16(ctx, R0), read_reg8(ctx, R1L), read_reg8(ctx, R2L), read_reg8(ctx, R2H));
        //     break;
        // case 0x3832: // likelysetVolume
        //     debug("LCD volume %x %u\n", ctx->ip, read_reg8(ctx, R0L));
        //     ctx->ip = ret;
        //     ctx->word_access++;
        //     return;
        //     break;
        //case 0x76AA: // accelReadSample
        //case 0xB390: // delaySomewhatAndThenSetTheRtc
        //case 0x273C: // accelInit
        // case 0x25AC: // check_some_rtc_set_bit_and_maybe_wait
            // ctx->ip = ret;
            // return;
        // case 0x7998: // normalModeEventLoop
        //     debug("MAIN EVENT LOOP !\n");
        //     break;
        // case 0x7882: // sleepModeEventLoop
        //     debug("SLEEP EVENT LOOP !\n");
        //     break;
        // case 0x289A: // checkBatteryForBelowGivenLevel
        //     debug("BAT CHECK !\n");
        //     write_reg8(ctx, R0L, 0);
        //     ctx->ip = ret;
        //     ctx->word_access++;
        //     return;
        //case 0x7FFC: // someLcdRelatedThing
            //SDL_Delay(100);
            //break;
    //}

    PUSHIP(ctx, ret);
    ctx->ip = br_addr;
}

typedef void (*instr_handler)(pw_context_t *ctx, uint16_t, uint16_t);

#define MAJ      ( instr >> 8)
#define MAJ_H    ( instr >> 12)
#define MAJ_L    ((instr >> 8) & 0xF)
#define MIN      ( instr       & 0xFF)
#define MIN_H    ((instr >> 4) & 0xF)
#define MIN_L    ( instr       & 0xF)
#define MIN_HMSB ((instr >> 7) & 1)
#define MIN_HCHK ((instr >> 4) & 7)
#define MIN_LMSB ((instr >> 3) & 1)
#define MIN_LCHK ( instr       & 7)
#define BIG_INSTRUCTION uint16_t instr3_4 = read16(ctx, addr+2);
#define THR_FUR   instr3_4
#define THR      (instr3_4 >> 8)
#define THR_H    (instr3_4 >> 12)
#define THR_L    ((instr3_4 >> 8) & 0xF)
#define FUR      ( instr3_4       & 0xFF)
#define FUR_H    ((instr3_4 >> 4) & 0xF)
#define FUR_L    (instr3_4 & 0xF)
#define FUR_HMSB ((instr3_4 >> 7) & 1)
#define FUR_HCHK ((instr3_4 >> 4) & 7)
#define FUR_LMSB ((instr3_4 >> 3) & 1)
#define FUR_LCHK ( instr3_4       & 7)

#define NOOP(mmn) debug(mmn"\n"); ctx->ip += 2; STATES(1, 0, 0, 0, 0, 0);

#define OP_R8_R8(mmn, f) debug(mmn " r%d, r%d\n", MIN_H, MIN_L); write_reg8(ctx, MIN_L, f(ctx, read_reg8(ctx, MIN_L), read_reg8(ctx, MIN_H))); ctx->ip += 2; STATES(1, 0, 0, 0, 0, 0);
#define OP_R8(mmn, f) debug(mmn " r%d\n", MIN_L); write_reg8(ctx, MIN_L, f(ctx, read_reg8(ctx, MIN_L))); ctx->ip += 2; STATES(1, 0, 0, 0, 0, 0);
#define OP_IMM8_R8(mmn, f) debug(mmn " #%x:8, r%d\n", MIN, MAJ_L); write_reg8(ctx, MAJ_L, f(ctx, read_reg8(ctx, MAJ_L), MIN)); ctx->ip += 2; STATES(1, 0, 0, 0, 0, 0);
#define OP_IMM3_ABS8(mmn, f) debug(mmn " #%x, @%x:8\n", FUR_HCHK, ABS8(MIN)); write8(ctx, ABS8(MIN), f(ctx, read8(ctx, ABS8(MIN)), FUR_HCHK)); ctx->ip += 4; STATES(2, 0, 0, 2, 0, 0);
#define OP_IMM3_REG8(mmn, f) debug(mmn " #%x, r%d\n", MIN_HCHK, MIN_L); write_reg8(ctx, MIN_L, f(ctx, read_reg8(ctx, MIN_L), MIN_HCHK)); ctx->ip += 2; STATES(1, 0, 0, 0, 0, 0);
#define OP_IMM3_REGI8(mmn, f) { debug(mmn " #%x:3, @er%d\n", FUR_HCHK, MIN_HCHK); uint32_t erd_addr = read_reg32(ctx, MIN_HCHK); write8(ctx, erd_addr, f(ctx, read8(ctx, erd_addr), FUR_HCHK)); ctx->ip += 4; STATES(2, 0, 0, 2, 0, 0); }

#define OP_R16_R16(mmn, f) debug(mmn " r%d, r%d\n", MIN_H, MIN_L); write_reg16(ctx, MIN_L, f(ctx, read_reg16(ctx, MIN_L), read_reg16(ctx, MIN_H))); ctx->ip += 2; STATES(1, 0, 0, 0, 0, 0);
#define OP_R16(mmn, f) debug(mmn " r%d\n", MIN_L); write_reg16(ctx, MIN_L, f(ctx, read_reg16(ctx, MIN_L))); ctx->ip += 2; STATES(1, 0, 0, 0, 0, 0);
#define OP_IMM16_R16(mmn, f) debug(mmn " #%x:16, r%d\n", peek16(ctx, addr+2), MIN_L); write_reg16(ctx, MIN_L, f(ctx, read_reg16(ctx, MIN_L), read16(ctx, addr+2))); ctx->ip += 4; STATES(2, 0, 0, 0, 0, 0);

#define OP_R32_R32(mmn, f) debug(mmn " er%d, er%d\n", MIN_HCHK, MIN_LCHK); write_reg32(ctx, MIN_LCHK, f(ctx, read_reg32(ctx, MIN_LCHK), read_reg32(ctx, MIN_HCHK))); ctx->ip += 2; STATES(1, 0, 0, 0, 0, 0);
#define OP_R32(mmn, f) debug(mmn  " er%d\n", MIN_LCHK); write_reg32(ctx, MIN_LCHK, f(ctx, read_reg32(ctx, MIN_LCHK))); ctx->ip += 2; STATES(1, 0, 0, 0, 0, 0);
#define OP_IMM32_R32(mmn, f) debug(mmn  " #%x:32, ER%d\n", peek32(ctx, addr+2), MIN_LCHK); write_reg32(ctx, MIN_LCHK, f(ctx, read_reg32(ctx, MIN_LCHK), read32(ctx, addr+2))); ctx->ip += 6; STATES(3, 0, 0, 0, 0, 0);

static void op_00(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if (MIN == 0) {
        // NOP
        NOOP("nop");
    }
}

static void op_01(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if(MIN == 0x00) {
        BIG_INSTRUCTION;
        switch (THR) {
            case 0x69:
                if (FUR_HMSB == 0 && FUR_LMSB == 0) {
                    // MOV.L @ERs,Erd
                    debug("mov.l @er%d, er%d\n", FUR_HCHK, FUR_LCHK);
                    write_reg32(ctx, FUR_LCHK, movl(ctx, 0, read32(ctx, read_reg32(ctx, FUR_HCHK))));
                    ctx->ip += 4;
                    STATES(2, 0, 0, 0, 2, 0);
                } else if (FUR_HMSB == 1 && FUR_LMSB == 0) {
                    // MOV.L ERs,@ERd
                    debug("mov.l er%d, @er%d\n", FUR_LCHK, FUR_HCHK);
                    write32(ctx, read_reg32(ctx, FUR_HCHK), movl(ctx, 0, read_reg32(ctx, FUR_LCHK)));
                    ctx->ip += 4;
                    STATES(2, 0, 0, 0, 2, 0);
                }
                break;
            case 0x6B:
                switch (FUR_H) {
                    case 0:
                        if (FUR_LMSB == 0) {
                            // MOV.L @aa:16,ERd
                            debug("mov.l @%x:16, er%d\n", ABS16(peek16(ctx, addr+4)), FUR_LCHK);
                            write_reg32(ctx, FUR_LCHK, movl(ctx, 0, read32(ctx, ABS16(read16(ctx, addr+4)))));
                            ctx->ip += 6;
                            STATES(3, 0, 0, 0, 2, 0);
                        }
                        break;
                    case 2:
                        if (FUR_LMSB == 0) {
                            // MOV.L @aa:24,ERd
                            debug("mov.l @%x:24, er%d\n", peek24(ctx, addr+5), FUR_LCHK);
                        }
                        break;
                    case 8:
                        if (FUR_LMSB == 0) {
                            // MOV.L ERs,@aa:16
                            debug("mov.l er%d, @%x:16\n", FUR_LCHK, ABS16(peek16(ctx, addr+4)));
                            write32(ctx, ABS16(read16(ctx, addr+4)), movl(ctx, 0, read_reg32(ctx, FUR_LCHK)));
                            ctx->ip += 6;
                            STATES(3, 0, 0, 0, 2, 0);
                        }
                        break;
                    case 0xA:
                        if (FUR_LMSB == 0) {
                            // MOV.L ERs,@aa:24
                            debug("mov.l er%d, @aa:24\n", FUR_LCHK, peek24(ctx, addr+5));
                        }
                        break;
                }
                break;
            case 0x6D:
                if (FUR_HMSB == 0 && FUR_LMSB == 0) {
                    if (FUR_H == 7) {
                        // POP.L ERn
                        debug("pop.l er%d\n", FUR_LCHK);
                        write_reg32(ctx, FUR_LCHK, popl(ctx));
                        INTERNAL_STATES(2);
                        ctx->ip += 4;
                        STATES(2, 0, 0, 0, 2, 2);
                    } else {
                        // MOV.L @ERs+,ERd
                        debug("mov.l @er%d+, er%d\n", FUR_HCHK, FUR_LCHK);
                        INTERNAL_STATES(2);
                    }
                } else if (FUR_HMSB == 1 && FUR_LMSB == 0) {\
                    if (FUR_H == 0xF) {
                        // PUSH.L ERn
                        debug("push.l er%d\n", FUR_LCHK);
                        pushl(ctx, read_reg32(ctx, FUR_LCHK));
                        INTERNAL_STATES(2);
                        ctx->ip += 4;
                        // Documentation is wrong about the cycles for this instruction
                        // Spec says 1 instruction fetch cycle but there are actually 2
                        STATES(2, 0, 0, 0, 2, 2);
                    } else {
                        // MOV.L ERs,@-ERd
                        debug("mov.l er%d, @-er%d\n", FUR_LCHK, FUR_HCHK);
                        INTERNAL_STATES(2);
                    }
                }
                break;
            case 0x6F:
                if (FUR_HMSB == 0 && FUR_LMSB == 0) {
                    // MOV.L @(d:16,ERs),ERd
                    debug("mov.l @(%x, er%d), er%d\n", peek16(ctx, addr+4), FUR_HCHK, FUR_LCHK);
                    write_reg32(ctx, FUR_LCHK, read32(ctx, movl(ctx, 0, DISPI32(read16(ctx, addr+4), FUR_HCHK))));
                    ctx->ip += 6;
                    STATES(3, 0, 0, 0, 2, 0);
                } else if (FUR_HMSB == 1 && FUR_LMSB == 0) {
                    // MOV.L ERs,(d:16,ERd)
                    debug("mov.l er%d, @(%x, er%d)\n", FUR_LCHK, peek16(ctx, addr+4), FUR_HCHK);
                    write32(ctx, DISPI16(read16(ctx, addr+4), FUR_HCHK), movl(ctx, 0, read_reg32(ctx, FUR_LCHK)));
                    ctx->ip += 6;
                    STATES(3, 0, 0, 0, 2, 0);
                }
                break;
            case 0x78:
                // FIXME
                break;
        }
    } else if (MIN == 0x40) {
        // LDC/STC
        // switch (THR) {
        //     case 0x69:

        //     case 0x6B:

        //     case 0x6D:

        //     case 0x6F:

        //     case 0x78:

        // }
    } else if (MIN == 0x80) {
        sleep(ctx);
        NOOP("sleep");
    } else if  (MIN == 0xC0) {
        BIG_INSTRUCTION;
        if (THR == 0x50) {
            // MULXS.B Rs,Rd
            debug("mulxs.b r%d, r%d\n", FUR_H, FUR_L);
            write_reg16(ctx, FUR_L, mulxsb(read_reg16(ctx, FUR_L), read_reg8(ctx, FUR_H)));
            INTERNAL_STATES(12);
            ctx->ip += 4;
            STATES(2, 0, 0, 0, 0, 12);
        } else if (THR == 0x52) {
            // MULXS.W Rs,ERd
            debug("mulxs.w r%d, er%d\n", FUR_H, FUR_LCHK);
            write_reg32(ctx, FUR_LCHK, mulxsw(read_reg32(ctx, FUR_LCHK), read_reg16(ctx, FUR_H)));
            INTERNAL_STATES(20);
            ctx->ip += 4;
            STATES(2, 0, 0, 0, 0, 20);
        }
    } else if (MIN == 0xD0) {
        BIG_INSTRUCTION;
        if (THR == 0x51) {
            // DIVXS.B Rs,Rd
            debug("divxs.b r%d, r%d\n", FUR_H, FUR_L);
            write_reg16(ctx, FUR_L, divxsb(ctx, read_reg16(ctx, FUR_L), read_reg8(ctx, FUR_H)));
            INTERNAL_STATES(12);
            ctx->ip += 4;
            STATES(2, 0, 0, 0, 0, 12);
        } else if (THR == 0x53) {
            // DIVXS.W Rs,ERd
            debug("divxs.w r%d, er%d\n", FUR_H, FUR_LCHK);
            write_reg32(ctx, FUR_LCHK, divxsw(ctx, read_reg32(ctx, FUR_LCHK), read_reg16(ctx, FUR_H)));
            INTERNAL_STATES(20);
            ctx->ip += 4;
            STATES(2, 0, 0, 0, 0, 20);
        }
    } else if(MIN == 0xF0) {
        BIG_INSTRUCTION;
        if (THR == 0x65) {
            // XOR.L ERs,ERd
            debug("xor.l er%d, er%d\n", FUR_HCHK, FUR_LCHK);
        } else if (THR == 0x66) {
            // AND.L ERs,ERd
            debug("and.l er%d, er%d\n", FUR_HCHK, FUR_LCHK);
        }
    }
}

static void op_02(pw_context_t *ctx, uint16_t addr, uint16_t instr) {

}

static void op_03(pw_context_t *ctx, uint16_t addr, uint16_t instr) {

}

static void op_04(pw_context_t *ctx, uint16_t addr, uint16_t instr) {

}

static void op_05(pw_context_t *ctx, uint16_t addr, uint16_t instr) {

}

static void op_06(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // ANDC #xx:8,CCR
    debug("andc #%x:8, CCR\n", MIN);
}

static void op_07(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // LDC #xx:8,CCR
    debug("ldc #%x:8, CCR\n", MIN);
    set_ccr(ctx, MIN);
    ctx->ip += 2;
    STATES(1, 0, 0, 0, 0, 0);
}

static void op_08(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // ADD.B Rs,Rd
    OP_R8_R8("add.b", addb);
}

static void op_09(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // ADD.W Rs,Rd
    OP_R16_R16("add.w", addw);
}

static void op_0A(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if(MIN_HMSB == 1 && MIN_LMSB == 0) {
        // ADD.L ERs,ERd
        OP_R32_R32("add.l", addl);
    } else if (MIN_H == 0) {
        // INC.B Rd
        OP_R8("inc.b", incb);
    }
}

static void op_0B(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // FIXME spec might be wrong about ADDS
    if(MIN_H == 0 && MIN_LMSB == 0) {
        // ADDS #1,ERd
        debug("adds #1, er%d\n", MIN_LCHK);
        write_reg32(ctx, MIN_LCHK, read_reg32(ctx, MIN_LCHK) + 1);
        ctx->ip += 2;
        STATES(1, 0, 0, 0, 0, 0);
    } else if (MIN_H == 8 && MIN_LMSB == 0) {
        // ADDS #2,ERd
        debug("adds #2, er%d\n", MIN_LCHK);
        write_reg32(ctx, MIN_LCHK, read_reg32(ctx, MIN_LCHK) + 2);
        ctx->ip += 2;
        STATES(1, 0, 0, 0, 0, 0);
    } else if (MIN_H == 9 && MIN_LMSB == 0) {
        // ADDS #4,ERd
        debug("adds #4, er%d\n", MIN_LCHK);
        write_reg32(ctx, MIN_LCHK, read_reg32(ctx, MIN_LCHK) + 4);
        ctx->ip += 2;
        STATES(1, 0, 0, 0, 0, 0);
    } else if (MIN_H == 5) {
        // INC.W #1,Rd
        debug("inc.w #1, r%d\n", MIN_L);
        write_reg16(ctx, MIN_L, read_reg16(ctx, MIN_L) + 1);
        ctx->ip += 2;
        STATES(1, 0, 0, 0, 0, 0);
    } else if (MIN_H == 0xD) {
        // INC.W #2,Rd
        debug("inc.w #2, r%d\n", MIN_L);
        write_reg16(ctx, MIN_L, read_reg16(ctx, MIN_L) + 2);
        ctx->ip += 2;
        STATES(1, 0, 0, 0, 0, 0);
    } else if (MIN_H == 7 && MIN_LMSB == 0) {
        debug("inc.l #1, er%d\n", MIN_LCHK);
        write_reg32(ctx, MIN_LCHK, read_reg32(ctx, MIN_LCHK) + 1);
        ctx->ip += 2;
        STATES(1, 0, 0, 0, 0, 0);
    }

}

static void op_0C(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // MOV.B Rs,Rd
    OP_R8_R8("mov.b", movb);
}

static void op_0E(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // ADDX Rs,Rd
    OP_R8_R8("addx", addx);
}

static void op_0D(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // MOV.W Rs,Rd
    OP_R16_R16("mov.w", movw);
}

static void op_0F(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if(MIN_HMSB == 1 && MIN_LMSB == 0) {
        // MOV.L ERs,ERd
        OP_R32_R32("mov.l", movl);
    }
}

static void op_10(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if (MIN_H == 0) {
        // SHLL.B Rd
        OP_R8("shll.b", shllb);
    } else if (MIN_H == 1) {
        // SHLL.W Rd
        OP_R16("shll.w", shllw);
    } else if (MIN_H == 3 && MIN_LMSB == 0) {
        // SHLL.L ERd
        OP_R32("shll.l", shlll);
    }
}

static void op_11(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if (MIN_H == 0) {
        // SHLR.B Rd
        OP_R8("shlr.b", shlrb);
    } else if (MIN_H == 1) {
        // SHLR.W Rd
        OP_R16("shlr.w", shlrw);
    } else if (MIN_H == 3 && MIN_LMSB == 0) {
        // SHLR.L ERd
        OP_R32("shlr.l", shlrl);
    } else if (MIN_H == 9) {
        // SHAR.W Rd
        OP_R16("shar.w", sharw);
    } else if (MIN_H == 0xB && MIN_LMSB == 0) {
        // SHAR.L ERd
        OP_R32("shar.l", sharl);
    }
}

static void op_12(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if(MIN_H == 1) {
        // ROTXL.W Rd
        debug("rotxl.w r%d\n", MIN_L);
    } else if (MIN_H == 3 && MIN_HMSB == 0) {
        /// ROTXL.L ERd
        debug("rotxl.l er%d\n", MIN_LCHK);
    } else if(MIN_H == 8) {
        // ROTL.B Rd
        OP_R8("rotl.b", rotlb);
    } else if(MIN_H == 9) {
        // ROTL.W Rd
        OP_R16("rotl.w", rotlw);
    }
}

static void op_13(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // ROTR, ROTRXR
}

static void op_14(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // OR.B Rs,Rd
    OP_R8_R8("or.b", orb);
}

static void op_15(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // XOR.B Rs,Rd
    OP_R8_R8("xor.b", xorb);
}

static void op_16(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // AND.B Rs,Rd
    OP_R8_R8("and.b", andb);
}

static void op_17(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if(MIN_H == 0) {
        // NOT.B Rd
        OP_R8("not.b", notb);
    } else if (MIN_H == 1) {
        // NOT.W Rd
        debug("not.w r%d\n", MIN_L);
    } else if (MIN_H == 5) {
        // EXTU.W Rd
        OP_R16("extu.w", extuw);
    } else if (MIN_H == 7 && MIN_LMSB == 0) {
        // EXTU.L ERd
        OP_R32("extu.l", extul);
    } else if (MIN_H == 8) {
        // NEG.B Rd
        OP_R8("neg.b", negb);
    } else if (MIN_H == 9) {
        // NEG.W Rd
        OP_R16("neg.w", negw);
    } else if (MIN_H == 0xD) {
        // EXTS.W Rd
        OP_R16("exts.w", extsw);
    } else if (MIN_H == 0xF && MIN_LMSB == 0) {
        // EXTS.L ERd
        OP_R32("exts.l", extsl);
    }
}

static void op_18(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // SUB.B Rs,Rd
    OP_R8_R8("sub.b", subb);
}

static void op_19(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // SUB.W Rs,Rd
    OP_R16_R16("sub.w", subw);
}

static void op_1A(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if(MIN_H == 0) {
        // DEC.B Rd
        OP_R8("dec.b", decb);
    } else if (MIN_HMSB == 1 && MIN_LMSB == 0) {
        // SUB.L ERs,ERd
        OP_R32_R32("sub.l", subl);
    }
}

static void op_1B(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if (MIN_H == 0 && MIN_LMSB == 0) {
        // SUBS #1,ERd
        debug("subs #1, er%d\n", MIN_LCHK);
        write_reg32(ctx, MIN_LCHK, read_reg32(ctx, MIN_LCHK) - 1);
        ctx->ip += 2;
        STATES(1, 0, 0, 0, 0, 0);
    } else if (MIN_H == 8 && MIN_LMSB == 0) {
        // SUBS #2,ERd
        debug("subs #2, er%d\n", MIN_LCHK);
        write_reg32(ctx, MIN_LCHK, read_reg32(ctx, MIN_LCHK) - 2);
        ctx->ip += 2;
        STATES(1, 0, 0, 0, 0, 0);
    } else if (MIN_H == 9 && MIN_LMSB == 0) {
        // SUBS #4,ERd
        debug("subs #4, er%d\n", MIN_LCHK);
        write_reg32(ctx, MIN_LCHK, read_reg32(ctx, MIN_LCHK) - 4);
        ctx->ip += 2;
        STATES(1, 0, 0, 0, 0, 0);
    } else if (MIN_H == 5) {
        // DEC.W #1,Rd
        debug("dec.w #1, r%d\n", MIN_L);
        write_reg16(ctx, MIN_L, decw(ctx, read_reg16(ctx, MIN_L), 1));
        ctx->ip += 2;
        STATES(1, 0, 0, 0, 0, 0);
    }
}

static void op_1C(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // CMP.B Rs,Rd
    debug("cmp.b r%d, r%d\n", MIN_H, MIN_L);
    subb(ctx, read_reg8(ctx, MIN_L), read_reg8(ctx, MIN_H));
    ctx->ip += 2;
    STATES(1, 0, 0, 0, 0, 0);
}

static void op_1D(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // CMP.W Rs,Rd
    debug("cmp.w r%d, r%d\n", MIN_H, MIN_L);
    subw(ctx, read_reg16(ctx, MIN_L), read_reg16(ctx, MIN_H));
    ctx->ip += 2;
    STATES(1, 0, 0, 0, 0, 0);
}

static void op_1E(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // SUBX Rs,Rd
    OP_R8_R8("subx", subx);
}

static void op_1F(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if(MIN_HMSB == 1 && MIN_LMSB == 0) {
        // CMP.L ERs,ERd
        debug("cmp.l er%d, er%d\n", MIN_HCHK, MIN_LCHK);
        subl(ctx, read_reg32(ctx, MIN_LCHK), read_reg32(ctx, MIN_HCHK));
        ctx->ip += 2;
        STATES(1, 0, 0, 0, 0, 0);
    }
}

static void op_2x(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // MOV.B @aa:8,Rd
    debug("mov.b @%x:8, r%d\n", ABS8(MIN), MAJ_L);
    write_reg8(ctx, MAJ_L, movb(ctx, 0, read8(ctx, ABS8(MIN))));
    ctx->ip += 2;
    STATES(1, 0, 0, 1, 0, 0);
}

static void op_3x(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // MOV.B Rs,@aa:8
    debug("mov.b r%d, @%x:8\n", MAJ_L, ABS8(MIN));
    write8(ctx, ABS8(MIN), movb(ctx, 0, read_reg8(ctx, MAJ_L)));
    ctx->ip += 2;
    STATES(1, 0, 0, 1, 0, 0);
}

static void op_4x(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // Bxx d:8
    uint32_t target = sign8_32(MIN) + addr + 2;
    if (target > 0x3838 && target < 0x388A) {
    printf("%s %x %d\n", branch_mnemonics[MAJ_L], target, branch_condition(ctx, MAJ_L));
    }
    if (branch_condition(ctx, MAJ_L)) {
        read16(ctx, ctx->ip + 2);
        ctx->ip = target;
    } else {
        read16(ctx, target);
        ctx->ip += 2;
    }
    STATES(2, 0, 0, 0, 0, 0);
}

static void op_50(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // MULXU.B Rs,Rd
    debug("mulxu.b r%d r%d\n", MIN_H, MIN_L);
    write_reg16(ctx, MIN_L, mulxub(read_reg16(ctx, MIN_L), read_reg8(ctx, MIN_H)));
    INTERNAL_STATES(12);
    ctx->ip += 2;
    STATES(1, 0, 0, 0, 0, 12);
}

static void op_51(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // DIVXU.B Rs,Rd
    debug("divxu.b r%d, r%d\n", MIN_H, MIN_L);
    write_reg16(ctx, MIN_L, divxub(ctx, read_reg16(ctx, MIN_L), read_reg8(ctx, MIN_H)));
    INTERNAL_STATES(12);
    ctx->ip += 2;
    STATES(1, 0, 0, 0, 0, 12);
}

static void op_52(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if (MIN_LMSB == 0) {
        // MULXU.W Rs,ERd
        debug("mulxu.w r%d, er%d\n", MIN_H, MIN_LCHK);
        write_reg32(ctx, MIN_LCHK, mulxuw(read_reg32(ctx, MIN_LCHK), read_reg16(ctx, MIN_H)));
        INTERNAL_STATES(20);
        ctx->ip += 2;
        STATES(1, 0, 0, 0, 0, 20);
    }

}

static void op_53(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if(MIN_LMSB == 0) {
        // DIVXU.W Rs,ERd
        debug("divxu.w r%d, er%d\n", MIN_H, MIN_LCHK);
        write_reg32(ctx, MIN_LCHK, divxuw(ctx, read_reg32(ctx, MIN_LCHK), read_reg16(ctx, MIN_H)));
        INTERNAL_STATES(20);
        ctx->ip += 2;
        STATES(1, 0, 0, 0, 0, 20);
    }
}

static void op_54(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if (MIN == 0x70) {
        // RTS
        debug("rts\n");
        read16(ctx, ctx->ip + 2);
        INTERNAL_STATES(2);
        ctx->ip = POPIP(ctx);
        STATES(2, 0, 1, 0, 0, 2);
    }
}

static void op_55(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // BSR d:8
    uint32_t target = ((int32_t)(int8_t)MIN) + addr + 2;
    uint32_t next = addr+2;
    debug("bsr %x:8\n", target);
    branch(ctx, target, next);
    read16(ctx, next);
    STATES(2, 0, 1, 0, 0, 0);
}

static void op_56(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if(MIN == 0x70) {
        // RTE
        debug("rte\n");
        // TODO: check if this is right
        read16(ctx, ctx->ip + 2);
        INTERNAL_STATES(2);
        set_ccr(ctx, popw(ctx));
        ctx->ip = POPIP(ctx);
        ctx->int_enabled = 1;
        STATES(2, 0, 2, 0, 0, 2);
    }
}

static void op_57(pw_context_t *ctx, uint16_t addr, uint16_t instr) {

}

static void op_58(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if (MIN_L == 0) {
        // Bxx d:16
        uint32_t target = sign16_32(read16(ctx, addr+2)) + addr + 4;
        debug("%s %x\n", branch_mnemonics[MIN_H], target);
        INTERNAL_STATES(2);
        if (branch_condition(ctx, MIN_H)) {
            ctx->ip = target;
        } else {
            ctx->ip += 4;
        }
        STATES(2, 0, 0, 0, 0, 2);
    }
}

static void op_59(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if(MIN_HMSB == 0 && MIN_L == 0) {
        // JMP @ERn
        debug("jmp @er%d\n", MIN_HCHK);
        read16(ctx, ctx->ip+2);
        ctx->ip = read_reg32(ctx, MIN_HCHK);
        STATES(2, 0, 0, 0, 0, 0);
    }
}

static void op_5A(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // JMP @aa:24
    BIG_INSTRUCTION;
    uint16_t target = (MIN << 8) | THR_FUR;
    debug("jmp @%x\n", ABS24(target));
    INTERNAL_STATES(2);
    ctx->ip = ABS24(target);
    STATES(2, 0, 0, 0, 0, 2);
}

static void op_5B(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // JMP @@aa:8
    debug("jmp @@%x:8\n", MIN);
    INTERNAL_STATES(2);
    STATES(2, 1, 0, 0, 0, 2);
}

static void op_5C(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // BSR d:16
    BIG_INSTRUCTION;
    if (MIN == 0) {
        uint32_t target = ((int32_t)(int16_t)THR_FUR) + addr + 4;
        debug("bsr %x:16\n", target);
        INTERNAL_STATES(2);
        branch(ctx, target, addr+4);
        STATES(2, 0, 1, 0, 0, 2);
    }
}

static void op_5D(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if (MIN_HMSB == 0 && MIN_L == 0) {
        // JSR @ERn
        debug("jsr @er%d\n", MIN_HCHK);
        read16(ctx, ctx->ip+2);
        branch(ctx, read_reg32(ctx, MIN_HCHK), ctx->ip+2);
        STATES(2, 0, 1, 0, 0, 0);
    }
}

static void op_5E(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // JSR @aa:24
    BIG_INSTRUCTION;
    uint32_t target = (MIN << 16) | THR_FUR;
    debug("jsr @%x:24\n", target);
    INTERNAL_STATES(2);
    branch(ctx, target, ctx->ip+4);
    STATES(2, 0, 1, 0, 0, 2);
}

static void op_5F(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // JSR @@aa:8
    debug("jsr @@%x:8\n", MIN);
    STATES(2, 1, 1, 0, 0, 0);
}

static void op_60(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // BSET Rn,Rd
    debug("bset r%d, r%d\n", MIN_H, MIN_L);
    write_reg8(ctx, MIN_L, read_reg8(ctx, MIN_L) | (1 << (read_reg8(ctx, MIN_H) & 7)));
    ctx->ip += 2;
    STATES(1, 0, 0, 0, 0, 0);
}

static void op_61(pw_context_t *ctx, uint16_t addr, uint16_t instr) {

}

static void op_62(pw_context_t *ctx, uint16_t addr, uint16_t instr) {

}

static void op_63(pw_context_t *ctx, uint16_t addr, uint16_t instr) {

}

static void op_64(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // OR.W Rs,Rd
    OP_R16_R16("or.w", orw);
}

static void op_65(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // XOR.W Rs,Rd
    OP_R16_R16("xor.w", xorw);
}

static void op_66(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // AND.W Rs,Rd
    OP_R16_R16("and.w", andw);
}

static void op_67(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if (MIN_HMSB == 0) {
        // BST #xx:3,Rd
        OP_IMM3_REG8("bst", bst);
    }
}

static void op_68(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if (MIN_HMSB == 0) {
        // MOV.B @ERs,Rd
        debug("mov.b @er%d, r%d\n", MIN_HCHK, MIN_L);
        write_reg8(ctx, MIN_L, movb(ctx, 0, read8(ctx, read_reg32(ctx, MIN_HCHK))));
        ctx->ip += 2;
        STATES(1, 0, 0, 1, 0, 0);
    } else {
        // MOV.B Rs,@ERd
        debug("mov.b r%d, @er%d\n", MIN_L, MIN_HCHK);
        write8(ctx, read_reg32(ctx, MIN_HCHK), movb(ctx, 0, read_reg8(ctx, MIN_L)));
        ctx->ip += 2;
        STATES(1, 0, 0, 1, 0, 0);
    }
}

static void op_69(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if (MIN_HMSB == 0) {
        // MOV.W @ERs,Rd
        debug("mov.w @er%d, r%d\n", MIN_HCHK, MIN_L);
        write_reg16(ctx, MIN_L, movw(ctx, 0, read16(ctx, read_reg32(ctx, MIN_HCHK))));
        ctx->ip += 2;
        STATES(1, 0, 0, 0, 1, 0);
    } else {
        // MOV.W Rs,@ERd
        debug("mov.w r%d, @er%d\n", MIN_L, MIN_HCHK);
        write16(ctx, read_reg32(ctx, MIN_HCHK), movw(ctx, 0, read_reg16(ctx, MIN_L)));
        ctx->ip += 2;
        STATES(1, 0, 0, 0, 1, 0);
    } 
}

static void op_6A(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    BIG_INSTRUCTION;
    switch(MIN_H) {
        case 0:
            // MOV.B @aa:16,Rd
            debug("mov.b @%x:16, r%d\n", ABS16(THR_FUR), MIN_L);
            write_reg8(ctx, MIN_L, movb(ctx, 0, read8(ctx, ABS16(THR_FUR))));
            ctx->ip += 4;
            STATES(2, 0, 0, 1, 0, 0);
            break;
        case 2:
            if (THR == 0) {
                // MOV.B @aa:24,Rd
                debug("mov.b @%x:24, r%d\n", peek24(ctx, addr+3), MIN_L);
            }
            break;
        case 4:
            // MOVFPE @aa:16,Rd
            debug("movfpe @%x:16, r%d\n", ABS16(THR_FUR), MIN_L);
        case 8:
            // MOV.B Rs,@aa:16
            debug("mov.b r%d, @%x:16\n", MIN_L, ABS16(THR_FUR));
            write8(ctx, ABS16(THR_FUR), movb(ctx, 0, read_reg8(ctx, MIN_L)));
            ctx->ip += 4;
            STATES(2, 0, 0, 1, 0, 0);
            break;
        case 0xA:
            if (THR == 0) {
                // MOV.B Rs,@aa:24
                debug("mov.b r%d, @%x:24\n", MIN_L, peek24(ctx, addr+3));
            }
            break;
        case 0xC:
            // MOVTPE Rs,@aa:16
            debug("movtpe r%d, @%x:16\n", MIN_L, ABS16(THR_FUR));
    }
}

static void op_6B(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    BIG_INSTRUCTION;
    switch(MIN_H) {
        case 0:
            // MOV.W @aa:16,Rd
            debug("mov.w @%x:16, r%d\n", ABS16(THR_FUR), MIN_L);
            write_reg16(ctx, MIN_L, movw(ctx, 0, read16(ctx, ABS16(THR_FUR))));
            ctx->ip += 4;
            STATES(2, 0, 0, 0, 1, 0);
            break;
        case 2:
            if (THR == 0) {
                // MOV.W @aa:24,Rd
                debug("mov.w @%x:24, r%d\n", ABS24(peek24(ctx, addr+3)), MIN_L);
                write_reg16(ctx, MIN_L, movw(ctx, 0, ABS24(read24(ctx, addr+3))));
                ctx->ip += 6;
                STATES(3, 0, 0, 0, 1, 0);
            }
        case 8:
            // MOV.W Rs,@aa:16
            debug("mov.w r%d, @%x:16\n", MIN_L, ABS16(THR_FUR));
            write16(ctx, ABS16(THR_FUR), movw(ctx, 0, read_reg16(ctx, MIN_L)));
            ctx->ip += 4;
            STATES(2, 0, 0, 0, 1, 0);
        case 0xA:
            if (THR == 0) {
                // MOV.W Rs,@aa:24
                debug("mov.w r%d, @%x:24\n", MIN_L, ABS24(peek24(ctx, addr+3)));
                write16(ctx, ABS24(read24(ctx, addr+3)), movw(ctx, 0, read_reg16(ctx, MIN_L)));
                ctx->ip += 6;
                STATES(3, 0, 0, 0, 1, 0);
            }
    }
}

static void op_6C(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if (MIN_HMSB == 0) {
        // MOV.B @ERs+,Rd
        debug("mov.b @er%d+, r%d\n", MIN_HCHK, MIN_L);
        uint32_t indir_addr = read_reg32(ctx, MIN_HCHK);
        write_reg8(ctx, MIN_L, movb(ctx, 0, read8(ctx, indir_addr)));
        write_reg32(ctx, MIN_HCHK, indir_addr+1);
        INTERNAL_STATES(2);
        ctx->ip += 2;
        STATES(1, 0, 0, 1, 0, 2);
    } else {
        // MOV.B Rs,@-ERd
        debug("mov.b r%d, @-er%d\n", MIN_L, MIN_HCHK);
        uint32_t indir_addr = read_reg32(ctx, MIN_HCHK) - 1;
        write8(ctx, indir_addr, movb(ctx, 0, read_reg8(ctx, MIN_L)));
        write_reg32(ctx, MIN_HCHK, indir_addr);
        INTERNAL_STATES(2);
        ctx->ip += 2;
        STATES(1, 0, 0, 1, 0, 2);
    }
}

static void op_6D(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if(MIN_HMSB == 0) {
        // MOV.W @ERs+,Rd
        debug("mov.w @er%d+, r%d\n", MIN_HCHK, MIN_L);
        uint16_t ers = read_reg32(ctx, MIN_HCHK);
        write_reg16(ctx, MIN_L, movw(ctx, 0, read16(ctx, ers)));
        write_reg16(ctx, MIN_HCHK, ers+2);
        INTERNAL_STATES(2);
        ctx->ip += 2;
        STATES(1, 0, 0, 0, 1, 2);
    } else if(MIN_H == 0xF) {
        // PUSH.W Rn
        debug("push.w r%d\n", MIN_L);
        pushw(ctx, movw(ctx, 0, read_reg16(ctx, MIN_L)));
        INTERNAL_STATES(2);
        ctx->ip += 2;
        STATES(1, 0, 0, 0, 1, 2);
    }
}

static void op_6E(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    BIG_INSTRUCTION;
    if (MIN_HMSB == 0) {
        // MOV.B @(d:16,ERs),Rd
        debug("mov.b @(%x,er%d), r%d\n", THR_FUR, MIN_HCHK, MIN_L);
        write_reg8(ctx, MIN_L, movb(ctx, 0, read8(ctx, DISPI16(THR_FUR, MIN_HCHK))));
        ctx->ip += 4;
        STATES(2, 0, 0, 1, 0, 0);
    } else {
        // MOV.B Rs,@(d:16,ERd)
        debug("mov.b r%d, @(%x,er%d)\n", MIN_L, THR_FUR, MIN_HCHK);
        write8(ctx, DISPI16(THR_FUR, MIN_HCHK), movb(ctx, 0, read_reg8(ctx, MIN_L)));
        ctx->ip += 4;
        STATES(2, 0, 0, 1, 0, 0);
    }    
}

static void op_6F(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    BIG_INSTRUCTION;
    if (MIN_HMSB == 0) {
        // MOV.W @(d:16,ERs),Rd
        debug("mov.w @(%x, er%d), r%d\n", THR_FUR, MIN_HCHK, MIN_L);
        write_reg16(ctx, MIN_L, movw(ctx, 0, read16(ctx, DISPI16(THR_FUR, MIN_HCHK))));
        ctx->ip += 4;
        STATES(2, 0, 0, 0, 1, 0);
    } else {
        // MOV.W Rs,@(d:16,ERd)
        debug("mov.w r%d, @(%x, er%d)\n", MIN_L, THR_FUR, MIN_HCHK);
        write16(ctx, DISPI16(THR_FUR, MIN_HCHK), movw(ctx, 0, read_reg16(ctx, MIN_L)));
        ctx->ip += 4;
        STATES(2, 0, 0, 0, 1, 0);
    }
}

static void op_70(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // BSET #xx:3,Rd
    OP_IMM3_REG8("bset", bset);
}

static void op_71(pw_context_t *ctx, uint16_t addr, uint16_t instr) {

}

static void op_72(pw_context_t *ctx, uint16_t addr, uint16_t instr) {

}

static void op_73(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // BTST #xx:3,Rd
    debug("btst #%x, r%d\n", MIN_HCHK, MIN_L);
    set_ccr_bit(ctx, CCR_Z, ~(read_reg8(ctx, MIN_L) >> MIN_HCHK) & 1UL);
    ctx->ip += 2;
    STATES(1, 0, 0, 0, 0, 0);
}

static void op_74(pw_context_t *ctx, uint16_t addr, uint16_t instr) {

}

static void op_75(pw_context_t *ctx, uint16_t addr, uint16_t instr) {

}

static void op_76(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if(MIN_HMSB == 0) {
        // BAND #xx:3,Rd
        debug("band #%x:8, r\n", MIN_HCHK, MIN_L);
    }
}

static void op_77(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if(MIN_HMSB == 0) {
        // BLD #xx:3,Rd
        debug("bld #%x:3, r%d\n", MIN_HCHK, MIN_L);
        set_ccr_bit(ctx, CCR_C, (read_reg8(ctx, MIN_L) >> MIN_HCHK) & 1);
        ctx->ip += 2;
        STATES(1, 0, 0, 0, 0, 0);
    }
}

static void op_78(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    BIG_INSTRUCTION;
    if (THR == 0x6A) {
        if (FUR_H == 2) {
            // MOV.B @(d:24,ERs),Rd
            debug("mov.b disp FIXME\n");
        } else if (FUR_H == 0xA) {
            // MOV.B Rs,@(d:24,ERd)
            debug("mov.b disp FIXME\n");
        }
    } else if (THR == 0x6B) {
        if (FUR_H == 2) {
            // MOV.W @(d:24,ERs),Rd
            debug("mov.b disp FIXME\n");
        } else if (FUR_H == 0xA) {
            // MOV.W Rs,@(d:24,ERd)
            debug("mov.b disp FIXME\n");
        }
    }

}

static void op_79(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if(MIN_H == 0) {
        // MOV.W #xx:16,Rd
        OP_IMM16_R16("mov.w", movw);
    } else if (MIN_H == 1) {
        // ADD.W #xx:16,Rd
        OP_IMM16_R16("add.w", addw);
    } else if (MIN_H == 2) {
        // CMP.W #xx:16,Rd
        debug("cmp.w #%x:16, r%d\n", peek16(ctx, addr+2), MIN_L);
        subw(ctx, read_reg16(ctx, MIN_L), read16(ctx, addr+2));
        ctx->ip += 4;
        STATES(2, 0, 0, 0, 0, 0);
    } else if (MIN_H == 6) {
        // AND.W #xx:16,Rd
        OP_IMM16_R16("and.w", andw);
    } else if (MIN_H == 3) {
        // SUB.W #xx:16,Rd
        OP_IMM16_R16("sub.w", subw);
    } else if (MIN_H == 4) {
        // OR.W #xx:16,Rd
        OP_IMM16_R16("or.w", orw);
    }
}

static void op_7A(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    if(MIN_H == 1 && MIN_LMSB == 0) {
        // ADD.L #xx:32,ERd
        OP_IMM32_R32("add.l", addl);
    } else if (MIN_H == 2 && MIN_LMSB == 0) {
        // CMP.L #xx:32,ERd
        uint32_t imm = read32(ctx, addr+2);
        debug("cmp.l #%x:32, ER%d\n", imm, MIN_LCHK);
        subl(ctx, read_reg32(ctx, MIN_LCHK), imm);
        ctx->ip += 6;
        STATES(3, 0, 0, 0, 0, 0);
    } else if (MIN_H == 3 && MIN_LMSB == 0) {
        // SUB.L #xx:32,ERd
        debug("sub.l #%x:32, ER%d\n", imm, MIN_LCHK);
    } else if (MIN_H == 4 && MIN_LMSB == 0) {
        // OR.L #xx:32,ERd
        // FIXME or/xor ?
        debug("or.l #%x:32, ER%d FIXME\n", imm, MIN_LCHK);
    } else if (MIN_H == 6 && MIN_LMSB == 0) {
        // AND.L #xx:32,ERd
        OP_IMM32_R32("and.l", andl);
    } else if(MIN_H == 0 && MIN_LMSB == 0) {
        // MOV.L #xx:32,Rd
        OP_IMM32_R32("mov.l", movl);
    }
}

static void op_7B(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    BIG_INSTRUCTION;
    if (THR_FUR == 0x598F) {
        if (MIN == 0x5C) {
            // EEPMOV.B
            debug("EEPMOV.B\n");
        } else if (MIN == 0xD4) {
            // EEPMOV.W
            debug("EEPMOV.W\n");
        }
    }
}

static void op_7C(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    //BIG_INSTRUCTION;
    debug("7c fixme\n");
    // if (THR == 0x76) {
    //     // BAND #xx:3,@ERd
    //     debug("bset #%x:3, @er%d\n", FUR_HCHK, MIN_HCHK);
    // }
}

static void op_7D(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    BIG_INSTRUCTION;
    if (MIN_HMSB == 0 && MIN_L == 0 && THR == 0x70 &&
        FUR_HMSB == 0 && FUR_L == 0) {
        // BSET #xx:3,@ERd
        OP_IMM3_REGI8("bset", bset);
    } else if(MIN_HMSB == 0 && MIN_L == 0 && THR == 0x72 && 
        FUR_HMSB == 0 && FUR_L == 0) {
        // BCLR #xx:3,@ERd
        OP_IMM3_REGI8("bclr", bclr);
    } else if(THR == 0x67) {
        // BST #xx:3,@ERd
        OP_IMM3_REGI8("bst", bst);
    } else if(THR == 0x71) {
        // BNOT #xx:3,@ERd
        OP_IMM3_REGI8("bnot", bnot);
    }
}

static void op_7E(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    BIG_INSTRUCTION;
    switch(THR) {
        case 0x74:
            if (FUR_HMSB == 0) {
                // BOR #xx:3,@aa:8
                debug("bor #%x:3, @%x:8\n", FUR_HCHK, MIN);
            } else {
                // BIOR #xx:3,@aa:8
                debug("bior #%x:3, @%x:8\n", FUR_HCHK, MIN);
            }
            break;
        case 0x75:
            if (FUR_HMSB == 0) {
                // BXOR #xx:3,@aa:8
                debug("bxor #%x:3, @%x:8\n", FUR_HCHK, MIN);
            } else {
                // BIXOR #xx:3,@aa:8
                debug("bixor #%x:3, @%x:8\n", FUR_HCHK, MIN);
            }
            break;
        case 0x76:
            if (FUR_HMSB == 0) {
                // BAND #xx:3,@aa:8
                debug("band #%x:3, @%x:8\n", FUR_HCHK, MIN);
                set_ccr_bit(ctx, CCR_C, ((read8(ctx, ABS8(MIN)) >> FUR_HCHK) & 1) & get_ccr_bit(ctx, CCR_C));
                ctx->ip += 4;
                STATES(2, 0, 0, 1, 0, 0);
            } else {
                // BIAND #xx:3,@aa:8
                debug("biand #%x:3, @%x:8\n", FUR_HCHK, MIN);
            }
            break;
        case 0x77:
            if (FUR_HMSB == 0) {
                // BLD #xx:3,@aa:8
                debug("bld #%x:3, @%x:8\n", FUR_HCHK, MIN);
                set_ccr_bit(ctx, CCR_C, (read8(ctx, ABS8(MIN)) >> FUR_HCHK) & 1);
                ctx->ip += 4;
                STATES(2, 0, 0, 1, 0, 0);
            } else {
                // BILD #xx:3,@aa:8
                debug("bild #%x:3, @%x:8\n", FUR_HCHK, MIN);
            }
            break;
    }  
}

static void op_7F(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    BIG_INSTRUCTION;
    if(THR == 0x70) {
        // BSET #xx:3,@aa:8
        OP_IMM3_ABS8("bset", bset);
    } else if(THR == 0x72) {
        // BCLR #xx:3,@aa:8
        OP_IMM3_ABS8("bclr", bclr);
    }
}

static void op_8x(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // ADD.B #xx:8,Rd
    OP_IMM8_R8("add.b", addb);
}

static void op_9x(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // ADDX #xx:8,Rd
    OP_IMM8_R8("addx", addx);
}

static void op_Ax(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // CMP.B #xx:8,Rd
    debug("cmp.b #%x:8, r%d\n", MIN, MAJ_L);
    subb(ctx, read_reg8(ctx, MAJ_L), MIN);
    ctx->ip += 2;
    STATES(1, 0, 0, 0, 0, 0);
}

static void op_Bx(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // SUBX #xx:8,Rd
    OP_IMM8_R8("subx", subx);
}

static void op_Cx(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // OR.B #xx:8,Rd
    OP_IMM8_R8("or.b", orb);
}

static void op_Dx(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // XOR.B #xx:8,Rd
    OP_IMM8_R8("xor.b", xorb);
}

static void op_Ex(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // AND.B #xx:8,Rd
    OP_IMM8_R8("and.b", andb);
}

static void op_Fx(pw_context_t *ctx, uint16_t addr, uint16_t instr) {
    // MOV.B #xx:8,Rd
    OP_IMM8_R8("mov.b", movb);
}

static instr_handler handlers[256] = {
/* 0 */ op_00, op_01, op_02, op_03, op_04, op_05, op_06, op_07, op_08, op_09, op_0A, op_0B, op_0C, op_0D, op_0E, op_0F,
/* 1 */ op_10, op_11, op_12, op_13, op_14, op_15, op_16, op_17, op_18, op_19, op_1A, op_1B, op_1C, op_1D, op_1E, op_1F,
/* 2 */ op_2x, op_2x, op_2x, op_2x, op_2x, op_2x, op_2x, op_2x, op_2x, op_2x, op_2x, op_2x, op_2x, op_2x, op_2x, op_2x,
/* 3 */ op_3x, op_3x, op_3x, op_3x, op_3x, op_3x, op_3x, op_3x, op_3x, op_3x, op_3x, op_3x, op_3x, op_3x, op_3x, op_3x,
/* 4 */ op_4x, op_4x, op_4x, op_4x, op_4x, op_4x, op_4x, op_4x, op_4x, op_4x, op_4x, op_4x, op_4x, op_4x, op_4x, op_4x,
/* 5 */ op_50, op_51, op_52, op_53, op_54, op_55, op_56, op_57, op_58, op_59, op_5A, op_5B, op_5C, op_5D, op_5E, op_5F,
/* 6 */ op_60, op_61, op_62, op_63, op_64, op_65, op_66, op_67, op_68, op_69, op_6A, op_6B, op_6C, op_6D, op_6E, op_6F,
/* 7 */ op_70, op_71, op_72, op_73, op_74, op_75, op_76, op_77, op_78, op_79, op_7A, op_7B, op_7C, op_7D, op_7E, op_7F,
/* 8 */ op_8x, op_8x, op_8x, op_8x, op_8x, op_8x, op_8x, op_8x, op_8x, op_8x, op_8x, op_8x, op_8x, op_8x, op_8x, op_8x,
/* 9 */ op_9x, op_9x, op_9x, op_9x, op_9x, op_9x, op_9x, op_9x, op_9x, op_9x, op_9x, op_9x, op_9x, op_9x, op_9x, op_9x,
/* A */ op_Ax, op_Ax, op_Ax, op_Ax, op_Ax, op_Ax, op_Ax, op_Ax, op_Ax, op_Ax, op_Ax, op_Ax, op_Ax, op_Ax, op_Ax, op_Ax,
/* B */ op_Bx, op_Bx, op_Bx, op_Bx, op_Bx, op_Bx, op_Bx, op_Bx, op_Bx, op_Bx, op_Bx, op_Bx, op_Bx, op_Bx, op_Bx, op_Bx,
/* C */ op_Cx, op_Cx, op_Cx, op_Cx, op_Cx, op_Cx, op_Cx, op_Cx, op_Cx, op_Cx, op_Cx, op_Cx, op_Cx, op_Cx, op_Cx, op_Cx,
/* D */ op_Dx, op_Dx, op_Dx, op_Dx, op_Dx, op_Dx, op_Dx, op_Dx, op_Dx, op_Dx, op_Dx, op_Dx, op_Dx, op_Dx, op_Dx, op_Dx,
/* E */ op_Ex, op_Ex, op_Ex, op_Ex, op_Ex, op_Ex, op_Ex, op_Ex, op_Ex, op_Ex, op_Ex, op_Ex, op_Ex, op_Ex, op_Ex, op_Ex,
/* F */ op_Fx, op_Fx, op_Fx, op_Fx, op_Fx, op_Fx, op_Fx, op_Fx, op_Fx, op_Fx, op_Fx, op_Fx, op_Fx, op_Fx, op_Fx, op_Fx,
};

static void ssu_dummy_write(pw_context_t *ctx, uint8_t byte) {
    printf("SSU UNK WRITE %x\n", byte);
}

static uint8_t ssu_dummy_read(pw_context_t *ctx) {
    printf("SSU UNK READ\n");
    return 0;
}

#define SSU_CB(ssu, read_cb, write_cb, data_ptr) \
    ssu_callbacks(ssu, (ssu_read_callback_t)read_cb, (ssu_write_callback_t)write_cb, data_ptr)

static uint8_t io2_get_pdr1(pw_context_t *ctx) {
    return ctx->pdr1;
}

static void io2_set_pdr1(pw_context_t *ctx, uint8_t val) {
    if (!(val & 4)) {
        SSU_CB(&ctx->ssu, eeprom_spi_read, eeprom_spi_write, &ctx->eeprom);
    } else if (!(val & 1)) {
        if (val & 2) {
            SSU_CB(&ctx->ssu, NULL, lcd_data, &ctx->lcd);
        } else {
            SSU_CB(&ctx->ssu, NULL, lcd_cmd, &ctx->lcd);
        }
    } else {
        SSU_CB(&ctx->ssu, ssu_dummy_read, ssu_dummy_write, ctx);
        eeprom_stop(&ctx->eeprom);
    }
    ctx->pdr1 = val;
}

static uint8_t io2_get_pdr9(pw_context_t *ctx) {
    return ctx->pdr9;
}

static void io2_set_pdr9(pw_context_t *ctx, uint8_t val) {
    if (val & 1) {
        SSU_CB(&ctx->ssu, ssu_dummy_read, ssu_dummy_write, ctx);
        accel_stop(&ctx->accel);
    } else {
        SSU_CB(&ctx->ssu, accel_read, accel_write, &ctx->accel);
    }
    ctx->pdr9 = val;
}

static void verifyStates(pw_context_t *ctx, uint16_t ip) {
    int ba = ctx->l;
    int wa = ctx->i + ctx->j + ctx->k + ctx->m;
    int in = ctx->n;
    if (ctx->byte_access != ba || ctx->word_access != wa || ctx->internal_states != in) {
        printf("WRONG STATES AT %04x (B:%d W:%d I:%d) - I:%d J:%d K:%d L:%d M:%d N:%d\n", 
            ip, ctx->byte_access, ctx->word_access, ctx->internal_states, ctx->i, ctx->j, ctx->k, ctx->l, ctx->m, ctx->n);
    }
    ctx->byte_access = 0;
    ctx->word_access = 0;
    ctx->internal_states = 0;
}

static void interrupt(pw_context_t *ctx, enum interrupts inter) {
    uint8_t ccr = get_ccr(ctx);

    // Interrupt handling routine
    //
    // first read the "prefetch address"
    // which is "2 plus PC value pushed on the stack"
    // but "replaced by internal op when recovering from sleep or standby"
    read16(ctx, ctx->ip + 2);
    INTERNAL_STATES(2);
    PUSHIP(ctx, ctx->ip);
    // push ccr twice in normal mode for alignment
    // (the higher byte is ignored by rte)
    pushw(ctx, (ccr << 8) | ccr);
    ctx->ip = read16(ctx, inter*2);
    INTERNAL_STATES(2);
    // finally read the first opcode of the handler
    ctx->instr_prefetch = read16(ctx, ctx->ip);

    // TODO: UI bits and SYSCR
    ctx->int_enabled = 0;

    STATES(2, 1, 2, 0, 0, 4);
    verifyStates(ctx, ctx->ip);

    printf("INT %s %x\n", int_names[inter], ctx->ip);
}

int halt = 0;

static void intHandler(int dummy) {
    halt = 1;
}

void pw_init(pw_context_t *ctx, int *should_redraw) {
    // load ROM and EEPROM from file
    FILE *romf = fopen("rom.bin", "rb");
    fread(ctx->rom, 1, 1 << 16, romf);
    fclose(romf);

    FILE *eepromf = fopen("eeprom.bin", "rb");
    fread(ctx->eeprom_data, 1, 1 << 16, eepromf);
    fclose(eepromf);

    // init all modules
    ssu_init(&ctx->ssu);
    eeprom_init(&ctx->eeprom, ctx->eeprom_data);
    lcd_init(&ctx->lcd, should_redraw);
    accel_init(&ctx->accel);
    rtc_init(&ctx->rtc);
    SSU_CB(&ctx->ssu, ssu_dummy_read, ssu_dummy_write, ctx);

    ctx->syscr1 = 3;
    ctx->syscr2 = 0xF0;
    ctx->ckstpr1 = 3;
    ctx->ckstpr2 = 4;
    ctx->osccr = 0;

    ctx->iegr  = 0;
    ctx->ienr1 = 0;
    ctx->ienr2 = 0;
    ctx->irr1  = 0;
    ctx->irr2  = 0;

    ctx->tmrw = ~TMRW_TMRW_MASK & 0xFF;
    ctx->tcrw = 0;
    ctx->tierw = ~TMRW_TIERW_MASK & 0xFF;
    ctx->tsrw = ~TMRW_TSRW_MASK & 0xFF;
    ctx->tior0 = ~TMRW_TIOR0_MASK & 0xFF;
    ctx->tior1 = ~TMRW_TIOR1_MASK & 0xFF;
    ctx->tcnt = 0;
    ctx->gra = 0xFFFF;
    ctx->grb = 0xFFFF;
    ctx->grc = 0xFFFF;
    ctx->grd = 0xFFFF;

    ctx->tmrw_rem = 0;

    
    ctx->mode = MODE_ACTIVE_HIGH;

    ctx->byte_access = 0;
    ctx->word_access = 0;
    ctx->internal_states = 0;

    // Reset routine for normal mode
    //
    // First read the entrypoint from the vector table
    uint16_t entrypoint = read16(ctx, 0);
    ctx->ip = entrypoint;
    // then a 2 state internal operation
    INTERNAL_STATES(2);
    // then we read the first opcode
    ctx->instr_prefetch = read16(ctx, entrypoint);

    // FIXME: should not immediately allow interrupts
    ctx->int_enabled = 1;

    // init I bit of CCR
    set_ccr_bit(ctx, CCR_I, 1);

    STATES(1, 1, 0, 0, 0, 2);
    verifyStates(ctx, 0);
}

static void pw_step(pw_context_t *ctx) {
    uint16_t oip = ctx->ip;

    // if (ctx->int_enabled) {
    //     int rtc_int = rtc_poll_int(&ctx->rtc);
    //     if (rtc_int != -1) {
    //         interrupt(ctx, rtc_int);
    //         goto VERIFY;
    //     }
    // }
    
    debug("%4x ", ctx->ip);
    debug("%.4x ", instr);
    uint16_t instr = ctx->instr_prefetch;
    handlers[instr >> 8](ctx, ctx->ip, instr);
    ctx->instr_prefetch = read16(ctx, ctx->ip);

    ctx->prev_ip = oip;

//VERIFY:
    print_state(ctx);
    verifyStates(ctx, ctx->prev_ip);
    
    if (oip == ctx->ip || ctx->ip > 0xBAC4) {
        printf("Something went wrong at %x\n", ctx->ip);
        for (int i = 0; i < 10; i++) {
            printf("%.2x ", peek8(ctx, ctx->ip + i));
        }
        puts("");
        halt = 1;
        return;
    }

    //rtc_update(&ctx.rtc);
}

static enum keys sdl_scancode_to_key(SDL_Scancode code) {
    switch (code) {
        case SDL_SCANCODE_A:
        case SDL_SCANCODE_LEFT:
            return (1 << 2);
        case SDL_SCANCODE_S:
        case SDL_SCANCODE_DOWN:
        case SDL_SCANCODE_RETURN:
            return (1 << 0);
        case SDL_SCANCODE_D:
        case SDL_SCANCODE_RIGHT:
            return (1 << 4);
        default:
            return 0;
    }
}

static enum keys mouse_to_button() {
    int winx, x, button;
    SDL_GL_GetDrawableSize(gWindow, &winx, NULL);
    SDL_GetMouseState(&x, NULL);
    button = x * 3 / winx;
    printf("x: %d / %d [%d]\n", x, winx, button);
    switch (button) {
        case 0:
            return (1 << 2);
        case 1:
            return (1 << 0);
        case 2:
            return (1 << 4);
        default:
            return 0;
    }

}

static uint8_t sdl_poll(uint8_t keys_pressed, int *should_redraw) {
    SDL_Event e;

    while (SDL_PollEvent(&e) != 0) {
        switch(e.type) {
            case SDL_QUIT:
                halt = 1;
                break;
            case SDL_WINDOWEVENT:
                *should_redraw = 1;
                break;
            case SDL_KEYDOWN:
                keys_pressed |= sdl_scancode_to_key(((SDL_KeyboardEvent*)&e)->keysym.scancode);
                break;
            case SDL_KEYUP:
                keys_pressed &= ~sdl_scancode_to_key(((SDL_KeyboardEvent*)&e)->keysym.scancode);
                break;
            case SDL_MOUSEBUTTONDOWN:
                keys_pressed |= mouse_to_button();
                break;
            case SDL_MOUSEBUTTONUP:
                keys_pressed &= ~mouse_to_button();
                break;
            default:
                break;
        }
    }
    return keys_pressed;
}

#define STATES_PER_SECOND 1600000
#define EXEC_BATCH_MS (1000 / 60)
#define STATES_PER_BATCH (STATES_PER_SECOND * (EXEC_BATCH_MS / 1000.0))

static void tmrw_update(pw_context_t *ctx, int states) {
    if (!ctx->int_enabled) {
        return;
    }
    uint16_t tcnt = ctx->tcnt;
    if (ctx->tmrw & (1 << TMRW_TMRW_CTS_BIT)) {
        int fullStates = states + ctx->tmrw_rem;
        ctx->tcnt += fullStates / 100;
        ctx->tmrw_rem = fullStates % 100;

        if ( ctx->tcnt >= ctx->gra && 
            ((tcnt < ctx->tcnt && tcnt < ctx->gra) ||
             (tcnt > ctx->tcnt && tcnt > ctx->gra))) {
            if (ctx->tcrw & (1 << TMRW_TCRW_CCLR_BIT)) {
                ctx->tcnt = 0;
                ctx->tmrw_rem = 0;
            }
            ctx->tsrw |= (1 << TMRW_TSRW_IMFA_BIT);
            if (ctx->tierw & (1 << TMRW_TIERW_IMIEA_BIT)) {
                interrupt(ctx, INT_TIMER_W);
            }
        }
    }
    //printf("tcnt %d\n", ctx->tcnt);
}

typedef struct render_context_t {
    pw_context_t* ctx;
    int* should_redraw;
    long* count;
} render_context_t;

#ifdef __EMSCRIPTEN__
void loop(void *render_ctx) {
#else
void loop(uintptr_t render_ctx) {
#endif
#ifndef __EMSCRIPTEN__
    Uint32 start = SDL_GetPerformanceCounter();
#endif // !__EMSCRIPTEN__
    render_context_t *context = (render_context_t*)render_ctx;
    while ((*(*context).ctx).states < STATES_PER_BATCH) {
        int old_states = (*(*context).ctx).states;
        pw_step((*context).ctx);
        tmrw_update((*context).ctx, (*(*context).ctx).states - old_states);
        (*(*context).count)++;
    }
    //rtc_update(&ctx.rtc);
    //int old_keys = ctx.keys_pressed;
    (*(*context).ctx).keys_pressed = sdl_poll((*(*context).ctx).keys_pressed, (*context).should_redraw);
    // TODO: maybe invert keys pressed ?
    portb_update(&(*(*context).ctx).portb, (*(*context).ctx).keys_pressed);
    if ((*(*context).should_redraw)) {
        sdl_draw(&(*(*context).ctx).lcd);
        (*(*context).should_redraw) = 0;
    }

#ifndef __EMSCRIPTEN__
    Uint32 end = SDL_GetPerformanceCounter();
    float seconds_elapsed = (end - start) / (float)SDL_GetPerformanceFrequency();
    int ms_to_sleep = EXEC_BATCH_MS - (int)(seconds_elapsed * 1000);

    // printf("Batch complete: %d states in %.6f s; sleeping for %d ms\n", (*(*context).ctx).states, seconds_elapsed, ms_to_sleep);

    if (ms_to_sleep > 0) {
        SDL_Delay(ms_to_sleep);
    }
#endif // !__EMSCRIPTEN__

    (*(*context).ctx).states = 0;
}

int main(int argc, char *argv[]) {
    pw_context_t ctx;
    int should_redraw = 0;

    if (!sdl_init()) {
        return 1;
    }

    signal(SIGINT, intHandler);

    pw_init(&ctx, &should_redraw);

    for(int i = 0; i < NUM_INTERRUPT_SOURCES; i++) {
        uint16_t addr = peek16(&ctx, i*2);
        int imm_rte = addr != 0xffff && peek16(&ctx, addr) == 0x5670;
        printf("[%2d] %-18s: %.4x%s\n", i, int_names[i], addr, imm_rte ? " [RTE]" : "");
    }
    
    long count = 0;

    render_context_t render_context;
    render_context.ctx = &ctx;
    render_context.should_redraw = &should_redraw;
    render_context.count = &count;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(loop, &render_context, -1, 1);
#else
    while (!halt) {
        loop(&render_context);
    }
#endif
    
    printf("Executed %ld steps!\n", count);
    sdl_quit();
}