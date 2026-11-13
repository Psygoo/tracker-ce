#ifndef TRACKER_CE_WR2_WEIGHTED_RANDOM_H
#define TRACKER_CE_WR2_WEIGHTED_RANDOM_H

#include <cstdint>

// Standalone domain model. No firmware globals, IO, UI, allocation or IRQ code.
namespace wr2 {
constexpr unsigned kMaxSlices = 48;
constexpr unsigned kMaxInstruments = 48;
constexpr std::uint8_t kFree = 255;

struct InstrumentState {
    std::uint8_t sliceCount;
    std::uint8_t owner[kMaxSlices];
    std::uint8_t weight[kMaxSlices];
    InstrumentState();
    // A geometry change needs an explicit lifecycle policy in the future adapter.
    // Reset is deliberately destructive; invalid counts leave this object intact.
    bool reset(unsigned count);
};
static_assert(sizeof(InstrumentState) == 97, "WR2 state budget changed");

// Optional value container; this module never allocates or instantiates one globally.
struct ProjectState { InstrumentState instrument[kMaxInstruments]; };
static_assert(sizeof(ProjectState) == 4656, "WR2 project budget changed");

enum class Result { Applied, Unchanged, Invalid, ForeignGroup, WouldMute };
bool valid(const InstrumentState& state);
Result create(InstrumentState& state, unsigned slice);
Result add(InstrumentState& state, unsigned master, unsigned slice);
// Requested percentages saturate to 0..100. A singleton remains 100%.
Result setWeight(InstrumentState& state, unsigned master, unsigned slice, int requested);
Result remove(InstrumentState& state, unsigned master, unsigned slice);

class Random {
public:
    explicit Random(std::uint32_t seed) : state_(seed ? seed : 0x6d2b79f5u) {}
    std::uint32_t state() const { return state_; }
    unsigned percent();
private:
    std::uint32_t state_;
};

// Pure ticket mapping, useful for exhaustive testing; invalid input returns trigger.
unsigned selectTicket(const InstrumentState& state, unsigned trigger, unsigned ticket);
// Only valid Beat Slice master triggers consume RNG. S2 bypass and native fallback
// return the original slice verbatim (including invalid indices: caller owns native
// bounds checking). Caller must pass a stable snapshot; this API is NOT thread safe.
unsigned select(const InstrumentState& state, unsigned trigger, unsigned liveSliceCount,
                bool beatSlice, bool directSlice, Random& random);
} // namespace wr2
#endif
