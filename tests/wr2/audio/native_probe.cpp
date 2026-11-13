// Fixture definitions only. Native method bodies are generated from current CE.
// Checked arrays stop immediately BEFORE an invalid access; no DSP is emulated.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../../../modules/wr2/AudioAdapter.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <cstdlib>
using std::uint8_t; using std::uint16_t;using std::uint32_t;using std::int8_t;using std::int16_t;
#define AUDIO_CORE_FLASH
#define AudioNoInterrupts() ((void)0)
#define AudioInterrupts() ((void)0)
constexpr unsigned kSampleInstrumentsCount=48;
// Suppress the native standalone FIRST_BEAT_SLICE_NOTE; expression warning.
#pragma warning(disable:4555)
constexpr int FIRST_BEAT_SLICE_NOTE=24;
constexpr uint16_t MAX_16BIT=65535;
struct Bounds : std::runtime_error { using std::runtime_error::runtime_error; };
template<class T,unsigned N> struct CheckedArray {
    T items[N]{};
    T& operator[](unsigned i){if(i>=N)throw Bounds("index="+std::to_string(i)+" capacity="+std::to_string(N));return items[i];}
};
struct Instrument { volatile uint8_t sliceNumber=0;uint8_t slicePlayThru=0;CheckedArray<uint16_t,48> slices; };
struct Project {CheckedArray<Instrument,48> instrument;} mtProject;
struct Stream {
    unsigned lastSlice[2]{},cacheCalls=0,notifications=0;
    void getSlicePreCache(uint8_t,uint8_t slice,int c,uint32_t,int16_t*&,uint32_t&){lastSlice[c]=slice;++cacheCalls;}
    template<class... T>void notifyNoteOn(T&&...){++notifications;}
} voiceStreamManager;
constexpr uint8_t envelopePhaseIdle=0,envelopePhaseDelay=1,envelopePhaseSustain=5,envelopePhaseForced=8;
class AudioPlayMemory {
public:
    uint8_t currentInstrIdx=0,lastInstrIdxInPlay=0,forcedSlice=0,sliceForcedFlag=0;
    int lastNote=0;AudioPlayMemory* samplePointsSource=nullptr;
    struct Points {uint32_t start=0,end=0,loop1=0,loop2=0,endPoint=0;} pointsInSamples,constrainsInSamples;
    int16_t samples[65536]{};int16_t* currentStartAddress=samples;int16_t* next=nullptr;
    uint32_t currentSampleLength=65536,length=0,currentSliceCacheFrames=0;
    int16_t* currentSliceCacheAddress=nullptr;int iPitchCounter=0;
    uint8_t reverseDirectionFlag=0,currentIsStreamed=0,currentPlayMode=0,playing=0;
    int channel=0,voiceIndex=-1;float pitchControl=1;
    struct {uint8_t pressedFlag=0,state=0,passFlag=0,endKillReleaseFlag=0;uint32_t wtPhase=0,wtReleaseStartPhase=0;} envelope;
    struct {bool enable=false;uint8_t instr_idx=0;int8_t note=0;bool isPrev=false;} stackedPlay;
    unsigned starts=0;
    wr2::audio::DeferredSelection descriptor;
    wr2::audio::Selection toStart,started;
    bool useDescriptor=false;
    void resumeOriginal1(){
#include "native_resume_1.inc"
    }
    void resumeOriginal2(){
#include "native_resume_2.inc"
    }
    void playForPrev(uint8_t i,int8_t n){play(i,n);}
    void submitResolved(const wr2::audio::Selection& token){
        using namespace wr2::audio;
        if(token.kind!=Kind::Slice&&token.kind!=Kind::WholeSample)return;
        auto phase=envelope.state==envelopePhaseIdle?EnvelopePhase::Idle:
                   envelope.state==envelopePhaseDelay?EnvelopePhase::Delay:
                   envelope.state==envelopePhaseForced?EnvelopePhase::Forced:EnvelopePhase::Active;
        auto dispatch=descriptor.submit(token,phase);
        toStart=dispatch.selection;useDescriptor=true;
        const auto before=starts;
        envelopeNoteOn(token.instrument,token.note); // EXACT native function body
        if((starts!=before)!=dispatch.startNow||stackedPlay.enable!=descriptor.pending())std::abort();
    }
    void resumeResolved(const wr2::audio::Request& live,unsigned variant){
        auto dispatch=descriptor.resume(live);
        if(!dispatch.startNow){stackedPlay.enable=false;return;}
        if(!stackedPlay.enable)std::abort();
        toStart=dispatch.selection;
        if(variant==1)resumeOriginal1();else resumeOriginal2(); // EXACT call-site snippets
    }
    void refreshStartParameters(){} void constrainCurrentTune(int8_t){} void setStartPitch(int8_t){}
    void applyFinetuneOnPitch(int8_t){}void calculateGlidePitch(int8_t){}
    uint32_t calculatePointPositionAndSnapToZero(uint16_t p,uint32_t,int16_t*){return p;}
    void envelopeSwitchPhase(uint8_t s){envelope.state=s;}
    void play(uint8_t i,int8_t n){
        ++starts;
        if(useDescriptor){
            started=toStart;
            if(started.kind!=wr2::audio::Kind::Slice&&started.kind!=wr2::audio::Kind::WholeSample)std::abort();
            // Proposed integration seam: event-owned selection/forced state, never
            // a second RNG call or the mutable FX state of another trigger.
            const auto savedFlag=sliceForcedFlag,savedSlice=forcedSlice;
            sliceForcedFlag=started.direct;forcedSlice=started.slice;
            i=started.instrument;n=static_cast<int8_t>(24+started.slice);
            playBeatSlice(i,n); // EXACT native Beat Slice function with stubbed HAL
            sliceForcedFlag=savedFlag;forcedSlice=savedSlice; // fixture-only scoped seam
        }
    }
    void playBeatSlice(uint8_t,int8_t);void setForcedSlice(uint8_t);void envelopeNoteOn(uint8_t,int8_t);
};
enum class controlType {sequencerMode=0};enum class parameterList {slice=0};
constexpr unsigned MOST_SIGNIFICANT_FX=0,LEAST_SIGNIFICANT_FX=1;
class playerEngine {
public:
    uint8_t currentInstrument_idx=0;bool trackControlParameter[2][1]{};
    struct {uint8_t slice=0;} currentSeqModValues;
    uint8_t lastSeqVal[2]{};void setFxSlice(){}void clearFxSlice(){}
    void fxSampleSlice(uint8_t,uint8_t);void endFxSlice(uint8_t);
};
#include "native_functions.inc"
static void prepare(unsigned count){for(unsigned j=0;j<48;++j){auto& i=mtProject.instrument[j];i.sliceNumber=static_cast<uint8_t>(count);for(unsigned s=0;s<48;++s)i.slices[s]=static_cast<uint16_t>(s*1000);}}
static bool expectBounds(const char* name,void(*test)()) {try{test();std::printf("FAIL missing bounds: %s\n",name);return false;}catch(const Bounds& e){std::printf("REPRODUCED %s: %s\n",name,e.what());return true;}}
int main(int argc,char** argv){ std::setvbuf(stdout,nullptr,_IONBF,0); SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if(argc==2 && std::strcmp(argv[1],"fx-zero")==0){prepare(0);playerEngine p;p.fxSampleSlice(7,0);return 99;}
    if(argc==2 && std::strcmp(argv[1],"end-fx-zero")==0){prepare(0);playerEngine p;p.trackControlParameter[1][0]=true;p.lastSeqVal[1]=7;p.endFxSlice(0);return 99;}
    bool ok=true;
    ok&=expectBounds("playBeatSlice invalid instrument before clamp",[]{prepare(3);AudioPlayMemory a;a.playBeatSlice(255,24);});
    ok&=expectBounds("playBeatSlice corrupt slice count",[]{prepare(255);AudioPlayMemory a;a.playBeatSlice(0,100);});
    ok&=expectBounds("stale forced slice after shrink -> slices[48]",[]{prepare(48);AudioPlayMemory a;a.setForcedSlice(47);a.sliceForcedFlag=1;mtProject.instrument[0].sliceNumber=2;a.playBeatSlice(0,24);});
    ok&=expectBounds("setForcedSlice invalid current instrument",[]{prepare(3);AudioPlayMemory a;a.currentInstrIdx=255;a.setForcedSlice(0);});
    prepare(0);AudioPlayMemory empty;empty.playBeatSlice(0,24);
    if(empty.pointsInSamples.start!=0||empty.pointsInSamples.end!=65535){std::puts("FAIL empty geometry");ok=false;}
    std::puts("CONFIRMED count=0 in playBeatSlice alone: full-range fallback; no array OOB in fixture");
    AudioPlayMemory env;env.envelopeNoteOn(0,24);if(env.starts!=1)ok=false;
    env.envelope.state=2;env.envelopeNoteOn(0,25);if(!env.stackedPlay.enable||env.starts!=1)ok=false;
    env.envelopeNoteOn(0,26);if(env.stackedPlay.enable||env.starts!=2)ok=false;
    std::puts("CONFIRMED native forced-phase retrigger cancels old stackedPlay and starts new event");
    {
        using namespace wr2;using namespace wr2::audio;
        PublishedBank bank;InstrumentState state;state.reset(3);create(state,0);add(state,0,1);add(state,0,2);setWeight(state,0,0,0);
        if(bank.instrument[0].publish(state,1)!=PublishResult::Published)std::abort();
        Request r;r.enabled=true;r.sliceCount=3;r.topology=1;Random rng(0x12345678u);
        auto token=resolve(bank,r,rng);auto rngAfter=rng.state();
        prepare(3);AudioPlayMemory left,right;right.channel=1;right.samplePointsSource=&left;left.currentIsStreamed=1;right.currentIsStreamed=1;left.voiceIndex=0;
        left.envelope.state=2;right.envelope.state=2;
        left.submitResolved(token);right.submitResolved(token);
        if(left.starts||right.starts)std::abort();
        // Change weights, live S2 and only the cut positions while event is pending.
        setWeight(state,0,0,100);bank.instrument[0].publish(state,1);
        mtProject.instrument[0].slices[1]=1200;mtProject.instrument[0].slices[2]=2400;
        left.sliceForcedFlag=1;left.forcedSlice=0;right.sliceForcedFlag=1;right.forcedSlice=0;
        left.resumeResolved(r,1);right.resumeResolved(r,2);
        if(left.starts!=1||right.starts!=1||left.started.slice!=token.slice||right.started.slice!=token.slice||left.pointsInSamples.start!=right.pointsInSamples.start||rng.state()!=rngAfter)std::abort();
        if(voiceStreamManager.cacheCalls!=2||voiceStreamManager.lastSlice[0]!=token.slice||voiceStreamManager.lastSlice[1]!=token.slice||voiceStreamManager.notifications!=1)std::abort();
        // Native Forced branch cancels a pending event on both channels.
        left.envelope.state=2;right.envelope.state=2;left.submitResolved(token);right.submitResolved(token);
        auto newer=resolve(bank,r,rng);left.submitResolved(newer);right.submitResolved(newer);
        if(left.descriptor.pending()||right.descriptor.pending()||left.started.slice!=newer.slice||right.started.slice!=newer.slice)std::abort();
        // Capture direct S2, then clear live flags before the original resume sites.
        left.envelope.state=2;right.envelope.state=2;r.directSlice=true;r.forcedSlice=2;
        auto before=rng.state();auto direct=resolve(bank,r,rng);left.submitResolved(direct);right.submitResolved(direct);
        left.sliceForcedFlag=0;right.sliceForcedFlag=0;r.directSlice=false;
        left.resumeResolved(r,1);right.resumeResolved(r,2);
        if(left.started.slice!=2||right.started.slice!=2||!left.started.direct||rng.state()!=before)std::abort();
        // Reject a delayed event after topology change; no native table access.
        left.envelope.state=2;left.submitResolved(token);auto starts=left.starts;r.topology=2;
        left.resumeResolved(r,1);if(left.starts!=starts)std::abort();
        // Invalid new requests are rejected before calling the native envelope.
        auto startsBefore=right.starts;r.instrument=255;right.submitResolved(resolve(bank,r,rng));
        if(right.starts!=startsBefore)std::abort();
        r.instrument=0;r.sliceCount=255;right.submitResolved(resolve(bank,r,rng));
        if(right.starts!=startsBefore)std::abort();
        // Empty native geometry is whole-sample playback, including direct S2.
        prepare(0);r.sliceCount=0;r.directSlice=true;auto emptyToken=resolve(bank,r,rng);
        right.envelope.state=envelopePhaseIdle;right.samplePointsSource=nullptr;right.submitResolved(emptyToken);
        if(right.pointsInSamples.start!=0||right.pointsInSamples.end!=65535)std::abort();
        std::puts("PASS extracted CE envelope + both resume sites + Beat Slice: stereo, pending weights/cuts/FX, forced retrigger, S2 and stale topology");
    }
    return ok?0:1;
}