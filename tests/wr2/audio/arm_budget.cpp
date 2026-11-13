#include "../../../modules/wr2/AudioAdapter.h"
#include <cstdint>
using namespace wr2;using namespace wr2::audio;
struct DoubleBank {ProjectState edit,audio;};
struct VersionedDoubleBank {DoubleBank states;std::atomic<std::uint32_t> generation[48];};
extern "C" const std::uint32_t wr2_audio_budget[] = {
    sizeof(ProjectState), sizeof(DoubleBank), sizeof(VersionedDoubleBank),
    sizeof(PublishedInstrument), sizeof(PublishedBank), sizeof(Snapshot),
    sizeof(Selection), sizeof(DeferredSelection), sizeof(DeferredSelection)*16,
    sizeof(Request), sizeof(Dispatch), sizeof(Random)
};
