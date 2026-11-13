#pragma once
#if !defined(WR2_BRIDGE_HOST) && (defined(TRACKER_MINI) || (!defined(PLATFORM_IMX) && !defined(CPU_MIMXRT1051CVL5B)))
#error Experimental WR2 audio bridge is exclusive to Tracker+
#endif
// Tracker+ experimental audio sidecar. No UI, project fields or persistence.
#include "../../modules/wr2/AudioAdapter.h"
class AudioPlayMemory;
class playerEngine;
namespace wr2native {
struct Geometry {
    const int16_t* address=nullptr;
    uint32_t length=0;
    uint16_t start=0,end=65535;
    uint8_t instrument=0,count=0,slice=0;
    bool snap=false;
};
struct Points {Geometry key;uint32_t start=0,end=0;bool valid=false;};
class Frame;
struct Cancel {};
struct State {
    wr2::audio::Selection active,pending;
    Geometry key;
    Frame* frame=nullptr;
    bool activeValid=false,pendingValid=false;
};
wr2::audio::PublishedBank& publication();
void sampleChanged(uint8_t); // foreground WAV completion, no group mutation
uint32_t randomState(); // diagnostics; callers must use the same exclusion domain
class Frame {
public:
    Frame(playerEngine&,uint8_t,int8_t,bool=false,uint8_t=0,uint8_t=0,uint8_t=0,uint8_t=0,int8_t=-1);
    explicit Frame(playerEngine&); // technical restart, never draws
    Frame(playerEngine&,Cancel); // paired native NoteOff, no PCM or selection
    Frame(AudioPlayMemory&,uint8_t,int8_t); // native pending resume, never draws
    static void sampleChanged(playerEngine&,uint8_t); // invalidate prepared-point authority only
    ~Frame();
    Frame(const Frame&)=delete;
    Frame&operator=(const Frame&)=delete;
    wr2::audio::Selection selection(AudioPlayMemory&,uint8_t) const;
    const Points* points(AudioPlayMemory&,uint8_t) const;
private:
    AudioPlayMemory* voices_[2]{};
    Frame* previous_[2]{};
    Frame* parent_=nullptr;
    wr2::audio::Selection main_,technical_[2];
    Points mainPoints_,technicalPoints_[2];
    bool hasTechnical_[2]{},technicalMode_=false,parentMode_=false,locked_=false,audioEnabled_=false;
    unsigned saved_=0;
    uint32_t observedStart_=0,observedEnd_=0;
    void lock();
    void unlock();
    void install();
    void prepareTechnical(uint8_t);
    bool needMain(playerEngine&,bool,uint8_t,int8_t)const;
    bool needTechnical(playerEngine&,bool,uint8_t,int8_t)const;
    static Geometry geometry(const wr2::audio::Selection&);
    static Points prepare(const wr2::audio::Selection&,AudioPlayMemory* source=nullptr);
    static bool same(const Geometry&,const Geometry&);
    static bool sameSelection(const wr2::audio::Selection&,const wr2::audio::Selection&);
    static bool live(const wr2::audio::Selection&);
    friend class ::AudioPlayMemory;
};
}
