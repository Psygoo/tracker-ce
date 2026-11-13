#pragma once
// OFFLINE prototype. No firmware includes/instances or automatic context detection.
#include "../../../modules/wr2/AudioAdapter.h"
namespace wr2minimal {
using namespace wr2;using namespace wr2::audio;
// Registers policy supplies CMSIS equivalents. No body may lower BASEPRI.
// Critical users: foreground and priority >=3 only; higher IRQs must NOT use WR2.
template<class Registers> class Guard {
public:
    Guard():saved_(Registers::basepri()){Registers::raise(0x30);Registers::barrier();}
    ~Guard(){Registers::barrier();Registers::restore(saved_);Registers::barrier();}
    Guard(const Guard&)=delete;Guard&operator=(const Guard&)=delete;
private:std::uint32_t saved_;
};
// Experimental fence for a NATIVE stereo transition, NOT just RNG. Calls native
// helpers that toggle output NVIC enable. Restores that bit while BASEPRI is raised.
// Timing of its native body is explicitly NOT established by the prototype.
template<class Registers> class StereoFence {
public:
    StereoFence():enabled_(Registers::audioEnabled()){}
    ~StereoFence(){Registers::audioEnable(enabled_);}
    StereoFence(const StereoFence&)=delete;StereoFence&operator=(const StereoFence&)=delete;
private:Guard<Registers> guard_;bool enabled_;
};
struct Prepared {Selection selection;InstrumentState snapshot;bool draw=false;};
// Immutable local snapshot. All publication reads/validation stay OUTSIDE mask.
inline Prepared prepare(const PublishedBank& bank,const Request&r){
    Prepared p;auto&s=p.selection;
    if(r.product!=Product::Plus||!r.enabled)return p;
    s.kind=Kind::Reject;
    if(r.instrument>=kMaxInstruments||r.note<-128||r.note>127)return p;
    s.instrument=static_cast<std::uint8_t>(r.instrument);s.note=static_cast<std::int8_t>(r.note);s.topology=r.topology;
    if(!r.beatSlice){s.kind=Kind::Native;return p;}
    if(!r.topology||r.sliceCount>kMaxSlices)return p;
    s.sliceCount=static_cast<std::uint8_t>(r.sliceCount);s.direct=r.directSlice;
    if(!r.sliceCount){s.kind=Kind::WholeSample;return p;}
    s.kind=Kind::Slice;const int index=r.note-24;
    const bool mapped=index>=0&&static_cast<unsigned>(index)<r.sliceCount;
    s.slice=static_cast<std::uint8_t>(index<0?0:static_cast<unsigned>(index)>=r.sliceCount?r.sliceCount-1:static_cast<unsigned>(index));
    if(r.directSlice){s.slice=static_cast<std::uint8_t>(r.forcedSlice<r.sliceCount?r.forcedSlice:r.sliceCount-1);return p;}
    if(mapped){Snapshot snapshot;
        if(bank.instrument[r.instrument].read(snapshot)&&snapshot.topology==r.topology&&snapshot.state.sliceCount==r.sliceCount&&snapshot.state.owner[s.slice]==s.slice){p.snapshot=snapshot.state;p.draw=true;}
    }
    return p;
}
// One call per NEW logical trigger. The caller keeps the returned value on its
// own call stack; never publish a pointer to it as an asynchronous pending event.
template<class Registers> Selection choose(const Prepared&p,Random&rng){
    Selection out=p.selection;if(!p.draw)return out;
    unsigned ticket;
    {Guard<Registers> guard;ticket=rng.percent();} // ONLY one xorshift and scaling
    out.slice=static_cast<std::uint8_t>(selectTicket(p.snapshot,out.slice,ticket));out.randomized=true;
    return out;
}
// Pure transition model. Caller holds Guard while accessing shared ChannelState.
// A native implementation must commit this WITH native stackedPlay fields.
struct ChannelState {
    Selection active,pending;bool activeValid=false,pendingValid=false;
    bool submit(const Selection&s,EnvelopePhase phase){
        if(phase==EnvelopePhase::Active){pending=s;pendingValid=true;return false;}
        pendingValid=false;active=s;activeValid=true;return true;
    }
    bool resume(const Request&live){
        if(!pendingValid)return false;
        pendingValid=false;
        if(!canStart(pending,live))return false;
        active=pending;activeValid=true;return true;
    }
    void cancel(){pendingValid=false;}
    // Caller supplies the ACTIVE event identity, not mutable engine.current*.
    bool technical(const Request&activeIdentity){cancel();return activeValid&&canStart(active,activeIdentity);}
};
struct Budget {PublishedBank bank;ChannelState channels[8][2];std::uint32_t revision[48];Random rng;};
} // namespace wr2minimal
