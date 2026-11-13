#include "WeightedRandom.h"

namespace wr2 {
InstrumentState::InstrumentState() { reset(0); }
bool InstrumentState::reset(unsigned count) {
    if (count > kMaxSlices) return false;
    sliceCount = static_cast<std::uint8_t>(count);
    for (unsigned i = 0; i < kMaxSlices; ++i) { owner[i] = kFree; weight[i] = 0; }
    return true;
}

bool valid(const InstrumentState& s) {
    if (s.sliceCount > kMaxSlices) return false;
    std::uint16_t totals[kMaxSlices] = {};
    for (unsigned i = 0; i < kMaxSlices; ++i) {
        const unsigned owner = s.owner[i];
        if (i >= s.sliceCount || owner == kFree) {
            if (owner != kFree || s.weight[i] != 0) return false;
        } else {
            // Short-circuit the bounds check BEFORE indexing owner[].
            if (owner >= s.sliceCount || s.owner[owner] != owner || s.weight[i] > 100)
                return false;
            totals[owner] += s.weight[i];
        }
    }
    for (unsigned i = 0; i < s.sliceCount; ++i)
        if (s.owner[i] == i && totals[i] != 100) return false;
    return true;
}

namespace {
bool masterValid(const InstrumentState& s, unsigned master) {
    return valid(s) && master < s.sliceCount && s.owner[master] == master;
}

// Hamilton apportionment. Iteration in index order gives stable lower-index ties.
// Called only for a validated group; skip is either a real index or kMaxSlices.
bool scale(const InstrumentState& s, unsigned master, unsigned skip,
           unsigned target, bool protectPositive, std::uint8_t* out) {
    unsigned sum = 0, peers = 0, floorSum = 0;
    std::uint8_t rem[kMaxSlices] = {};
    bool awarded[kMaxSlices] = {};
    for (unsigned i = 0; i < s.sliceCount; ++i)
        if (s.owner[i] == master && i != skip) { sum += s.weight[i]; ++peers; }
    if (!peers) return false;
    if (!sum) {
        unsigned extra = target % peers;
        for (unsigned i = 0; i < s.sliceCount; ++i)
            if (s.owner[i] == master && i != skip) {
                out[i] = static_cast<std::uint8_t>(target / peers + (extra != 0));
                if (extra) --extra;
            }
        return true;
    }
    for (unsigned i = 0; i < s.sliceCount; ++i) {
        if (s.owner[i] != master || i == skip) continue;
        const unsigned product = s.weight[i] * target;
        out[i] = static_cast<std::uint8_t>(product / sum);
        rem[i] = static_cast<std::uint8_t>(product % sum);
        // Preserve the S13D Add refusal before awarding remainders.
        if (protectPositive && s.weight[i] && !out[i]) return false;
        floorSum += out[i];
    }
    for (unsigned left = target - floorSum; left; --left) {
        unsigned best = kMaxSlices;
        for (unsigned i = 0; i < s.sliceCount; ++i)
            if (s.owner[i] == master && i != skip && !awarded[i] &&
                (best == kMaxSlices || rem[i] > rem[best])) best = i;
        if (best == kMaxSlices) return false;
        ++out[best]; awarded[best] = true;
    }
    return true;
}

unsigned choose(const InstrumentState& s, unsigned master, unsigned ticket) {
    unsigned cumulative = 0;
    for (unsigned i = 0; i < s.sliceCount; ++i) {
        if (s.owner[i] != master) continue;
        cumulative += s.weight[i];
        if (ticket < cumulative) return i;
    }
    return master;
}
} // namespace

Result create(InstrumentState& s, unsigned slice) {
    if (!valid(s) || slice >= s.sliceCount) return Result::Invalid;
    if (s.owner[slice] == slice) return Result::Unchanged;
    if (s.owner[slice] != kFree) return Result::ForeignGroup;
    s.owner[slice] = static_cast<std::uint8_t>(slice); s.weight[slice] = 100;
    return Result::Applied;
}

Result add(InstrumentState& s, unsigned master, unsigned slice) {
    if (!masterValid(s, master) || slice >= s.sliceCount) return Result::Invalid;
    if (s.owner[slice] == master) return Result::Unchanged;
    if (s.owner[slice] != kFree) return Result::ForeignGroup;
    std::uint8_t next[kMaxSlices] = {};
    if (!scale(s, master, kMaxSlices, 90, true, next)) return Result::WouldMute;
    for (unsigned i = 0; i < s.sliceCount; ++i)
        if (s.owner[i] == master) s.weight[i] = next[i];
    s.owner[slice] = static_cast<std::uint8_t>(master); s.weight[slice] = 10;
    return Result::Applied;
}

Result setWeight(InstrumentState& s, unsigned master, unsigned slice, int requested) {
    if (!masterValid(s, master) || slice >= s.sliceCount) return Result::Invalid;
    if (s.owner[slice] != master) return Result::ForeignGroup;
    const unsigned value = requested < 0 ? 0u : requested > 100 ? 100u :
                           static_cast<unsigned>(requested);
    if (s.weight[slice] == value) return Result::Unchanged;
    std::uint8_t next[kMaxSlices] = {};
    if (!scale(s, master, slice, 100 - value, false, next)) return Result::Unchanged;
    next[slice] = static_cast<std::uint8_t>(value);
    for (unsigned i = 0; i < s.sliceCount; ++i)
        if (s.owner[i] == master) s.weight[i] = next[i];
    return Result::Applied;
}

Result remove(InstrumentState& s, unsigned master, unsigned slice) {
    if (!masterValid(s, master) || slice >= s.sliceCount) return Result::Invalid;
    if (s.owner[slice] != master) return Result::Unchanged;
    if (slice == master) {
        for (unsigned i = 0; i < s.sliceCount; ++i)
            if (s.owner[i] == master) { s.owner[i] = kFree; s.weight[i] = 0; }
        return Result::Applied;
    }
    std::uint8_t next[kMaxSlices] = {};
    if (!scale(s, master, slice, 100, false, next)) return Result::Invalid;
    for (unsigned i = 0; i < s.sliceCount; ++i)
        if (s.owner[i] == master) s.weight[i] = next[i];
    s.owner[slice] = kFree; s.weight[slice] = 0;
    return Result::Applied;
}

unsigned Random::percent() {
    std::uint32_t x = state_;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5; state_ = x;
    // S13D scaling, bounded execution; no rejection/retry loop.
    return static_cast<unsigned>((static_cast<std::uint64_t>(x) * 100u) >> 32);
}

unsigned selectTicket(const InstrumentState& s, unsigned trigger, unsigned ticket) {
    if (!valid(s) || trigger >= s.sliceCount || ticket >= 100 || s.owner[trigger] != trigger)
        return trigger;
    return choose(s, trigger, ticket);
}

unsigned select(const InstrumentState& s, unsigned trigger, unsigned liveSliceCount,
                bool beatSlice, bool directSlice, Random& random) {
    if (!beatSlice || directSlice || liveSliceCount != s.sliceCount || !valid(s) ||
        trigger >= s.sliceCount || s.owner[trigger] != trigger) return trigger;
    return choose(s, trigger, random.percent());
}
} // namespace wr2
