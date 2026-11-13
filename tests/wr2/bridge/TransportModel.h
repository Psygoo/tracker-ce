#pragma once
// Offline candidate only. Not included in a CE build. One producer/one consumer.
#include <atomic>
#include <cstdint>
#include <type_traits>
namespace wr2model {
enum class Source : std::uint8_t { Pad, MidiDin, MidiUsb, Sequencer, InstrumentPreview, RawPreview, SdPreview };
enum class Op : std::uint8_t { NoteOn, NoteOff, Cancel, Reset };
struct Command {
    std::uint32_t serial=0, epoch=0;
    std::uint8_t voice=0,instrument=0;
    std::int8_t note=24,velocity=127;
    std::uint8_t fx1=0,value1=0,fx2=0,value2=0;
    Op op=Op::NoteOn;
    Source source=Source::Pad;
    std::uint8_t forced=0,flags=0;
    std::uint32_t submittedSample=0;
};
static_assert(sizeof(Command)==24,"Recheck ARM budget");
static_assert(std::is_trivially_copyable<Command>::value,"Value commands only");
static_assert(std::atomic<std::uint32_t>::is_always_lock_free,"32-bit atomics required");
template<unsigned Capacity> class Queue {
    static_assert(Capacity && !(Capacity&(Capacity-1)) && Capacity<(1u<<31),"Power of two bounded queue");
public:
    // No overwrite/drop/coalescing. On false the CALLER still owns the command.
    bool push(const Command& c) {
        const auto h=head_.load(std::memory_order_relaxed);
        if(h-tail_.load(std::memory_order_acquire)==Capacity)return false;
        slots_[h&(Capacity-1)]=c;
#ifdef WR2_TRANSPORT_TEST_HOOK
        if(beforePublish)beforePublish();
#endif
        head_.store(h+1,std::memory_order_release);return true;
    }
    bool pop(Command& c) {
        const auto t=tail_.load(std::memory_order_relaxed);
        if(t==head_.load(std::memory_order_acquire))return false;
        c=slots_[t&(Capacity-1)];
        tail_.store(t+1,std::memory_order_release);return true;
    }
    // Consumer-only observation, not permission to reset storage.
    bool empty()const{return tail_.load(std::memory_order_relaxed)==head_.load(std::memory_order_acquire);}
#ifdef WR2_TRANSPORT_TEST_HOOK
    void (*beforePublish)()=nullptr;
    void emptyAt(std::uint32_t n){head_.store(n);tail_.store(n);} // quiescent test only
#endif
private:
    Command slots_[Capacity];
    std::atomic<std::uint32_t> head_{0},tail_{0};
};
enum class Owner : std::uint32_t { Live, DrainRequested, Quiescent, Render };
static_assert(std::atomic<Owner>::is_always_lock_free && sizeof(std::atomic<Owner>)==4,"Recheck ownership atomic on target");
class Handover {
public:
    Owner owner()const{return owner_.load(std::memory_order_acquire);}
    // Producer stops admissions before request; no in-flight push from that producer.
    bool requestDrain(){return change(Owner::Live,Owner::DrainRequested);}
    // ONLY live executor, at event boundary, after draining queue. Not a NVIC fence.
    bool acknowledge(bool queueEmpty){return queueEmpty && change(Owner::DrainRequested,Owner::Quiescent);}
    bool takeRender(){return change(Owner::Quiescent,Owner::Render);}
    // ONLY render owner after finishing event/DSP and all its state writes.
    bool releaseRender(){return change(Owner::Render,Owner::Quiescent);}
    bool resumeLive(){return change(Owner::Quiescent,Owner::Live);}
private:
    bool change(Owner from,Owner to){return owner_.compare_exchange_strong(from,to,std::memory_order_acq_rel);}
    std::atomic<Owner> owner_{Owner::Live};
};
} // namespace wr2model
