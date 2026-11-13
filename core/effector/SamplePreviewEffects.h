#pragma once
#if TRACKER_CE_ENABLE_SD_MONO || TRACKER_CE_ENABLE_SD_EDITS
#include "mtEffect.h"
#include <cstddef>
namespace samplepreview
{
// Registration describes object lifetime, never a second DSP implementation.
// Registered kernels must be length-preserving, instance-local and in-place safe.
struct EffectCopy
{
    size_t bytes;
    mtEffect* (*copy)(void*, const mtEffect&);
    void (*destroy)(mtEffect*);
    bool (*start)(mtEffect*, const int[2], bool);
    bool fullSelectionPeaks;
    void (*commitHistory)(mtEffect*, const mtEffect*);
    void (*processChannel)(mtEffect*, unsigned, int16_t*, int16_t*, unsigned);
};
const EffectCopy* effectCopy(unsigned effect);
}
#endif
