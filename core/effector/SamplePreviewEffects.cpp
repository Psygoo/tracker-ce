#if TRACKER_CE_ENABLE_SD_MONO || TRACKER_CE_ENABLE_SD_EDITS
#include "SamplePreviewEffects.h"
#include "mtEffectAmplifier.h"
#include "mtEffectBitcrusher.h"
#include "mtEffectCompresor.h"
#include "mtEffectEQ.h"
#include "mtEffectLimiter.h"
#include "mtEffectNormalizer.h"
#include "mtEffectOverdrive.h"
#include "mtSampleEditorEngine.h"
#if TRACKER_CE_ENABLE_SD_MONO
#include "mtEffectStereoMono.h"
#endif
#include <new>
#include <type_traits>
namespace samplepreview
{
template<class T>
static mtEffect* copy(void* p, const mtEffect& from)
{
    static_assert(std::is_trivially_destructible_v<T>,
                  "Preview copy must not share owned resources");
    return new (p) T(static_cast<const T&>(from));
}
template<class T>
static void destroy(mtEffect* p)
{
    static_cast<T*>(p)->~T();
}
template<class T>
static bool start(mtEffect* p, const int*, bool)
{
    return p->startProcess();
}
#if TRACKER_CE_ENABLE_SD_EDITS
template<>
bool start<mtEffectNormalizer>(mtEffect* p, const int* peaks, bool)
{
    return static_cast<mtEffectNormalizer*>(p)->startPreview(peaks);
}
#endif
#if TRACKER_CE_ENABLE_SD_MONO
template<>
bool start<mtEffectStereoMono>(mtEffect* p, const int*, bool stereo)
{
    static_cast<mtEffectStereoMono*>(p)->setInputStereo(stereo);
    return p->startProcess();
}
#endif
template<class T, bool peaks = false>
static const EffectCopy* entry()
{
    static const EffectCopy e{
        sizeof(T),
        copy<T>,
        destroy<T>,
        start<T>,
        peaks,
#if TRACKER_CE_ENABLE_SD_EDITS
        []() constexpr -> void (*)(mtEffect*, const mtEffect*)
        {
            if constexpr (std::is_same_v<T, mtEffectLimiter>)
                return [](mtEffect* to, const mtEffect* from)
                { static_cast<T*>(to)->commitSdHistory(*static_cast<const T*>(from)); };
            else
                return nullptr;
        }(),
#else
        nullptr,
#endif
#if TRACKER_CE_ENABLE_SD_EDITS
        []() constexpr -> void (*)(mtEffect*, unsigned, int16_t*, int16_t*, unsigned)
        {
            if constexpr (std::is_same_v<T, mtEffectEQ> || std::is_same_v<T, mtEffectLimiter> ||
                          std::is_same_v<T, mtEffectCompresor>)
                return [](mtEffect* p, unsigned ch, int16_t* in, int16_t* out, unsigned n)
                { static_cast<T*>(p)->processSdChannel(ch, in, out, n); };
            else
                return nullptr;
        }()
#else
        nullptr
#endif
    };
    return &e;
}
const EffectCopy* effectCopy(unsigned effect)
{
#if TRACKER_CE_ENABLE_SD_EDITS
    switch (effect)
    {
        case editorEffectNormalizer: return entry<mtEffectNormalizer, true>();
        case editorEffectAmplifier: return entry<mtEffectAmplifier>();
        case editorEffectOverdrive: return entry<mtEffectOverdrive>();
        case editorEffectBitcrusher: return entry<mtEffectBitcrusher>();
        case editorEffectEQ: return entry<mtEffectEQ>();
        case editorEffectLimiter: return entry<mtEffectLimiter>();
        case editorEffectCompresor: return entry<mtEffectCompresor>();
        default: break;
    }
#endif
#if TRACKER_CE_ENABLE_SD_MONO
    if (effect == editorEffectStereoMonoSD)
        return entry<mtEffectStereoMono>();
#endif
    return nullptr;
}
}
#endif
