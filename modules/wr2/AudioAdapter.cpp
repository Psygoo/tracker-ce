#include "AudioAdapter.h"
#include <limits>

namespace wr2 { namespace audio {
#ifdef WR2_AUDIO_TEST_HOOKS
// Deterministic interleaving instrumentation, absent from normal/ARM builds.
extern void publicationTestStep(bool writing, unsigned step);
#define WR2_STEP(w, n) publicationTestStep(w, n)
#else
#define WR2_STEP(w, n) ((void)0)
#endif

PublishedInstrument::PublishedInstrument() {
    for (auto& word : words_) word.store(0, std::memory_order_relaxed);
}

PublishResult PublishedInstrument::publish(const InstrumentState& state, std::uint32_t topology) {
    if (!topology || !valid(state)) return PublishResult::Invalid;
    const auto before = sequence_.load();
    // Sequence never wraps: reset/reconstruction requires complete quiescence.
    if ((before & 1u) || before >= std::numeric_limits<std::uint32_t>::max() - 1u)
        return PublishResult::Exhausted;
    std::uint32_t packed[25];
    for (unsigned i = 0; i < 24; ++i) {
        const unsigned a = 2 * i, b = a + 1;
        packed[i] = static_cast<std::uint32_t>(state.owner[a]) |
                    (static_cast<std::uint32_t>(state.weight[a]) << 8) |
                    (static_cast<std::uint32_t>(state.owner[b]) << 16) |
                    (static_cast<std::uint32_t>(state.weight[b]) << 24);
    }
    packed[24] = state.sliceCount;
    sequence_.store(before + 1u); // odd: publication in progress
    WR2_STEP(true, 0);
    for (unsigned i = 0; i < 25; ++i) { words_[i].store(packed[i]); WR2_STEP(true, i + 1); }
    topology_.store(topology); WR2_STEP(true, 26);
    sequence_.store(before + 2u); WR2_STEP(true, 27); // even: complete
    return PublishResult::Published;
}

bool PublishedInstrument::read(Snapshot& output) const {
    const auto before = sequence_.load();
    if (!before || (before & 1u)) return false;
    WR2_STEP(false, 0);
    Snapshot local;
    local.topology = topology_.load(); WR2_STEP(false, 1);
    for (unsigned i = 0; i < 24; ++i) {
        const auto value = words_[i].load();
        local.state.owner[2 * i] = static_cast<std::uint8_t>(value);
        local.state.weight[2 * i] = static_cast<std::uint8_t>(value >> 8);
        local.state.owner[2 * i + 1] = static_cast<std::uint8_t>(value >> 16);
        local.state.weight[2 * i + 1] = static_cast<std::uint8_t>(value >> 24);
        WR2_STEP(false, i + 2);
    }
    const auto count = words_[24].load(); WR2_STEP(false, 26);
    const auto after = sequence_.load();
    if (before != after || !local.topology || count > kMaxSlices) return false;
    local.state.sliceCount = static_cast<std::uint8_t>(count);
    if (!valid(local.state)) return false;
    output = local;
    return true;
}

Selection resolve(const PublishedBank& bank, const Request& r, Random& rng) {
    Selection result;
    // Product/feature gate before touching publication state or consuming RNG.
    if (r.product != Product::Plus || !r.enabled) return result;
    result.kind = Kind::Reject;
    if (r.instrument >= kMaxInstruments || r.note < -128 || r.note > 127) return result;
    result.instrument = static_cast<std::uint8_t>(r.instrument);
    result.note = static_cast<std::int8_t>(r.note);
    result.topology = r.topology;
    if (!r.beatSlice) { result.kind = Kind::Native; return result; }
    if (!r.topology || r.sliceCount > kMaxSlices) return result;
    result.sliceCount = static_cast<std::uint8_t>(r.sliceCount);
    result.direct = r.directSlice;
    if (!r.sliceCount) { result.kind = Kind::WholeSample; return result; }
    result.kind = Kind::Slice;
    const int index = r.note - 24;
    const bool mapped = index >= 0 && static_cast<unsigned>(index) < r.sliceCount;
    unsigned slice = index < 0 ? 0u : static_cast<unsigned>(index) >= r.sliceCount ?
                     r.sliceCount - 1 : static_cast<unsigned>(index);
    if (r.directSlice) {
        slice = r.forcedSlice < r.sliceCount ? r.forcedSlice : r.sliceCount - 1;
    } else if (mapped) {
        Snapshot snapshot;
        if (bank.instrument[r.instrument].read(snapshot) && snapshot.topology == r.topology &&
            snapshot.state.sliceCount == r.sliceCount && snapshot.state.owner[slice] == slice) {
            slice = wr2::select(snapshot.state, slice, r.sliceCount, true, false, rng);
            result.randomized = true;
        }
    }
    result.slice = static_cast<std::uint8_t>(slice);
    return result;
}

bool canStart(const Selection& s, const Request& live) {
    if (live.product != Product::Plus || !live.enabled || s.kind == Kind::Reject) return false;
    if (s.instrument >= kMaxInstruments || live.instrument != s.instrument) return false;
    if (s.kind == Kind::Native) return true; // actual native path still needs its own guards
    if (!live.beatSlice || !s.topology || live.topology != s.topology ||
        live.sliceCount != s.sliceCount || live.sliceCount > kMaxSlices) return false;
    if (s.kind == Kind::WholeSample) return live.sliceCount == 0;
    return live.sliceCount > 0 && s.slice < live.sliceCount;
}

Dispatch DeferredSelection::submit(const Selection& selection, EnvelopePhase phase) {
    // Rejected/special native requests are handled outside this Beat Slice sidecar.
    if (selection.kind != Kind::Slice && selection.kind != Kind::WholeSample) return {};
    if (phase == EnvelopePhase::Active) {
        stacked_ = selection;
        pending_ = true;
        return {};
    }
    // CE's Forced branch cancels the old stackedPlay and starts the newest event.
    pending_ = false;
    return {selection, true};
}

Dispatch DeferredSelection::resume(const Request& live) {
    if (!pending_) return {};
    pending_ = false;
    if (!canStart(stacked_, live)) return {};
    return {stacked_, true};
}
#undef WR2_STEP
} } // namespace wr2::audio
