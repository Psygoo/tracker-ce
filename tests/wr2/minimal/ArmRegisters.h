#pragma once
// Compiled ONLY as a detached Cortex-M7 object in this investigation.
#include "MIMXRT1051.h"
static_assert(__NVIC_PRIO_BITS==4,"Re-evaluate encoded priority threshold");
struct ArmRegisters {
    static unsigned basepri(){return __get_BASEPRI();}
    // Conservative register-update window. Preserve PRIMASK exactly; do not
    // assume an unverified Cortex-M7 revision has immediate BASEPRI updates.
    static void raise(unsigned n){const auto p=__get_PRIMASK();__disable_irq();__set_BASEPRI_MAX(n);__DSB();__ISB();__set_PRIMASK(p);}
    static void restore(unsigned n){const auto p=__get_PRIMASK();__disable_irq();__set_BASEPRI(n);__DSB();__ISB();__set_PRIMASK(p);}
    static void barrier(){__DSB();__ISB();}
    static bool audioEnabled(){return NVIC_GetEnableIRQ(DMA0_DMA16_IRQn)!=0;}
    static void audioEnable(bool on){if(on)NVIC_EnableIRQ(DMA0_DMA16_IRQn);else NVIC_DisableIRQ(DMA0_DMA16_IRQn);}
};
