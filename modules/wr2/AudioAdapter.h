#ifndef TRACKER_CE_WR2_AUDIO_ADAPTER_H
#define TRACKER_CE_WR2_AUDIO_ADAPTER_H
#include "WeightedRandom.h"
#include <atomic>

namespace wr2 { namespace audio {
// No firmware opt-in exists in this phase: neither this file nor any instance is
// referenced by the CE Makefiles/native code. There is no global runtime state.
constexpr bool kFirmwareIntegrationEnabled = false;
static_assert(std::atomic<std::uint32_t>::is_always_lock_free, "WR2 requires lock-free 32-bit atomics");
static_assert(sizeof(std::atomic<std::uint32_t>) == 4, "Recheck WR2 publication budget");

struct Snapshot {
    InstrumentState state;
    std::uint32_t topology = 0;
};
enum class PublishResult { Published, Invalid, Exhausted };

// SINGLE publisher per instrument; any number of independent snapshot readers.
// All shared fields are atomic, INCLUDING payload. Sequential consistency is
// intentional: do not weaken ordering without another proof and ARM inspection.
// One bounded read attempt, no spinning. A failed read leaves output unchanged.
class PublishedInstrument {
public:
    PublishedInstrument();
    PublishResult publish(const InstrumentState& state, std::uint32_t topology);
    bool read(Snapshot& output) const;
    // Single atomic load: lifecycle validity, not a payload snapshot.
    std::uint32_t context() const { return topology_.load(); }
    bool writable() const { const auto seq=sequence_.load(); return !(seq&1u)&&seq<0xfffffffeu; }
    // Fail closed if a lifecycle clear cannot publish. Permanent until reboot.
    void retire() { sequence_.store(0xfffffffeu); topology_.store(0); }
private:
    std::atomic<std::uint32_t> sequence_{0};
    std::atomic<std::uint32_t> topology_{0};
    std::atomic<std::uint32_t> words_[25]; // 24 owner/weight pairs + count
    friend struct PublicationTestAccess;
};
struct PublishedBank { PublishedInstrument instrument[kMaxInstruments]; };
static_assert(sizeof(PublishedInstrument) == 108, "Publication row budget changed");
static_assert(sizeof(PublishedBank) == 5184, "Publication bank budget changed");

enum class Product : std::uint8_t { Original, Mini, Plus };
struct Request {
    Product product = Product::Plus;
    bool enabled = false; // harness opt-in only; NOT a firmware build switch
    unsigned instrument = 0;
    int note = 24; // CE native int8_t domain
    unsigned sliceCount = 0;
    bool beatSlice = true;
    bool directSlice = false; // captured AFTER native FX precedence resolution
    unsigned forcedSlice = 0;
    std::uint32_t topology = 0; // nonzero, never reused for a new topology
};
enum class Kind : std::uint8_t { Native, Slice, WholeSample, Reject };
struct Selection {
    std::uint32_t topology = 0;
    std::uint8_t instrument = 0;
    std::uint8_t slice = 0;
    std::int8_t note = 0;
    Kind kind = Kind::Native;
    bool direct = false;
    bool randomized = false;
    std::uint8_t sliceCount = 0;
};
static_assert(sizeof(Selection) == 12, "Trigger descriptor budget changed");

// Call ONCE per accepted logical trigger, then COPY the result to both channels.
// rng is caller-owned and must not be shared by unsynchronised trigger contexts.
Selection resolve(const PublishedBank& bank, const Request& request, Random& rng);
// No RNG access. Count, instrument, mode and topology are checked again at the
// actual start. Changed cut positions keep topology; inserted/deleted/reordered
// slices MUST advance it. Stale delayed events are dropped, never re-randomised.
bool canStart(const Selection& selection, const Request& live);

enum class EnvelopePhase : std::uint8_t { Idle, Delay, Active, Forced };
struct Dispatch { Selection selection; bool startNow = false; };
// Prototype sidecar matching the observed CE envelope branch semantics. This
// does not modify/replace the actual CE envelope or implement its DSP timing.
class DeferredSelection {
public:
    Dispatch submit(const Selection& selection, EnvelopePhase phase);
    Dispatch resume(const Request& live);
    void cancel() { pending_ = false; }
    bool pending() const { return pending_; }
private:
    Selection stacked_;
    bool pending_ = false;
};
} } // namespace wr2::audio
#endif
