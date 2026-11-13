#include "../minimal/LocalSelection.h"
#include "../../../modules/wr2/UiModel.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <optional>
#include <limits>
#include <tuple>
#include <vector>
using std::uint8_t;using std::uint16_t;using std::uint32_t;using std::int8_t;using std::int16_t;using std::int32_t;
using namespace wr2;using namespace wr2::audio;
using std::clamp;
#ifdef A1_FIRMWARE_BRIDGE
#include "../../../core/AudioEngine/NativeBridge.h"
#endif
static unsigned long long checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::abort();}}while(0)
#define AUDIO_CORE_RAM
#define AUDIO_CORE_FLASH
// Native loops use signed counters with vector::size; preserve their exact bodies.
#pragma warning(disable:4018)
template<class T>T mapClamped(T v,T a,T b,T x,T y){v=std::clamp(v,a,b);return x+(v-a)*(y-x)/(b-a);}
constexpr unsigned kSampleInstrumentsCount=48,MAX_SLICE_NUMBER=48;
constexpr int MAX_16BIT=65535;
constexpr int MAX_NOTE=120;
constexpr int FIRST_BEAT_SLICE_NOTE=24;
constexpr int MIN_GRANULAR_LENGTH=1;
enum {playModeSingleShot=0,playModeLoopForward,playModeLoopBackward,playModePingpong,playModeSlice,playModeBeatSlice,playModeWavetable,playModeGranular};
enum {granularShapeSquare,granularShapeGauss,granularShapeTriangle};
constexpr unsigned ACTIVE_ENVELOPES=7,envAmp=0,envPan=1,envCutoff=2,envWtPos=3,envGranPos=4,envFinetune=5,envGranLen=6;
constexpr unsigned lfoShapeRandom=9;constexpr int bandPass=0,highPass=1,lowPass=2;
enum class controlType{sequencerMode=0,sequencerMode2=1,performanceMode=2};
enum class parameterList{slice=0,lfoAmp=1,endPoint=2,startPoint=3,wavetablePosition=4,tune=5,filterAttack=6};
constexpr unsigned MOST_SIGNIFICANT_FX=0,LEAST_SIGNIFICANT_FX=1;
enum {envelopePhaseIdle,envelopePhaseDelay,envelopePhaseAttack,envelopePhaseHold,envelopePhaseDecay,envelopePhaseSustain,envelopePhaseRelease,envelopePhaseForced};
struct Sequencer {
    struct strFxConsts {
#include "fx-enum.inc"
    };
    enum{STEP_NOTE_OFF_FADE=-1,STEP_NOTE_OFF_CUT=-3,STEP_NOTE_OFF_DEFAULT=-2};
    void tick(float);
    float getStepPeriodMs(){return 125;}
}sequencer;
struct Env{uint8_t loop=0,enable=1;float release=100;};
namespace envelopeGenerator {using strEnv=Env;}
static Env passEnvelope;
struct Lfo{uint8_t speed=0,shape=0,wtBend=0,wtSust=0;};
struct Instrument{struct{uint32_t length=65536;char file_name[64]{};uint8_t type=0;uint16_t wavetable_window_size=32,wavetableWindowNumber=32;}sample;
    struct{uint8_t type=0,shape=0;uint16_t currentPosition=32768,grainLength=100;}granular;
    float glide=0,tune=0,tuneResample=0,fineTune=0;uint16_t wavetableCurrentWindow=0;uint8_t selectedSlice=1;
    uint8_t isActive=1,sliceNumber=4,playMode=5;bool slicePlayThru=false;std::array<uint16_t,48>slices{};uint16_t startPoint=0,endPoint=65535,loopPoint1=0,loopPoint2=65535;uint32_t sampleLength=65536,pcmRevision=1;Env envelope[8];Lfo lfo[8];bool isEnvEnabled(unsigned i){return envelope[i].enable!=0;}};
struct {std::array<Instrument,48>instrument;struct Memory{int16_t*address[2]{};uint32_t preCacheFrames=65536;bool isStreamed=false,isStereo=false;uint32_t sampleHash=1,ramFrames=0;};std::array<Memory,48>sampleMemory;}mtProject;
static std::function<void()>restoreHook;static unsigned restoreCountdown=0;
struct Registers {
    static unsigned mask,primask,context;static bool nvic,pending;static std::function<void()>isr;
    static unsigned basepri(){return mask;}static void raise(unsigned m){if(!mask||m<mask)mask=m;}
    static bool canRun(){return !primask&&nvic&&(!mask||mask>0x30)&&context>3;}
    static void service(){if(pending&&canRun()){pending=false;auto old=context;context=3;isr();context=old;}}
    static void restore(unsigned m){mask=m;if(restoreHook&&canRun()&&restoreCountdown&&--restoreCountdown==0){auto f=std::move(restoreHook);restoreHook={};f();}service();}static void barrier(){}
    static bool audioEnabled(){return nvic;}static void audioEnable(bool b){nvic=b;service();}
    static void reset(){mask=primask=0;context=16;nvic=true;pending=false;isr=[]{};}
};
unsigned Registers::mask=0,Registers::primask=0,Registers::context=16;bool Registers::nvic=true,Registers::pending=false;std::function<void()>Registers::isr=[]{};
static unsigned boundary=0,injectBoundary=~0u;
static void interruptPoint(){if(boundary++==injectBoundary){injectBoundary=~0u;Registers::pending=true;}Registers::service();}
static void AudioNoInterrupts(){Registers::audioEnable(false);}
static void AudioInterrupts(){Registers::audioEnable(true);interruptPoint();}
constexpr unsigned AUDIO_BLOCK_SAMPLES=128;
struct alignas(4) audio_block_t{int16_t data[AUDIO_BLOCK_SAMPLES]{};};
struct AudioStream{static void release(audio_block_t*){}};
static int32_t signed_multiply_32x16b(int32_t,int32_t){return 0;}static int32_t signed_multiply_32x16t(int32_t,int32_t){return 0;}static uint32_t pack_16b_16b(int32_t,int32_t){return 0;}
static int32_t evalWavetableEnvelopeFixed30(uint32_t,uint8_t,uint8_t){return 0;}
struct Timer{unsigned micros(){return 123;}};
struct utilMathRandom{static void randomSeed(unsigned){}static int32_t random(int32_t a,int32_t){return a;}};
constexpr float SAMPLES_PER_MSEC=44.1f;
struct FakeEnv{void setIsRandom(bool){}void setWavetableParams(int,int,int){}void start(){}void stop(){}void killToZero(){}};
struct Start{unsigned instrument=0,slice=0,channel=0;bool technical=false;uint32_t start=0,end=0;unsigned mode=0;uint32_t loop1=0,loop2=0,span=0;};
static std::vector<Start> starts;
static unsigned slowUnderMask=0,missingPlan=0,slowCalls=0,pcmWithAnyMask=0;
static bool resolveBlockers=false,notReady=false;
static bool baselineNative=false,hoistFxPreparation=false,nativeDomains=false;
static bool baselineTechnical=false;
static unsigned snapTotal=0,snapOutputMasked=0,snapBasepriMasked=0,pcmVisits=0,pcmOutputMasked=0,pcmBasepriMasked=0;
static void resetWork(){snapTotal=snapOutputMasked=snapBasepriMasked=pcmVisits=pcmOutputMasked=pcmBasepriMasked=0;}

// Negative test only: execute/count the prohibited preparation to expose the
// native FX-only entry. Successful candidate scenarios never permit it.
static bool expectMaskedPreparation=false;
static unsigned maskedPreparationCalls=0;
struct GeometryKey{
    std::uintptr_t address=0;
    uint32_t length=0,pcmRevision=0;
    uint16_t rawStart=0,rawEnd=0;
    uint8_t instrument=0,mode=0,count=0,slice=0;
    bool snap=false;
};
static bool sameKey(const GeometryKey&a,const GeometryKey&b){return a.address==b.address&&a.length==b.length&&a.pcmRevision==b.pcmRevision&&a.rawStart==b.rawStart&&a.rawEnd==b.rawEnd&&a.instrument==b.instrument&&a.mode==b.mode&&a.count==b.count&&a.slice==b.slice&&a.snap==b.snap;}
struct Bounds{unsigned instrument=0,slice=0,count=0,length=65536;uint16_t rawStart=0,rawEnd=65535;uint32_t start=0,end=65536;bool valid=false;};
// Fixture geometry computation: same stubbed snap result as native callee, done
// before the fence; actual PCM search remains outside this host model.
static Bounds prepareBounds(const Selection&s){Bounds b;b.instrument=s.instrument;b.slice=s.slice;b.count=s.sliceCount;b.valid=s.kind==Kind::Slice||s.kind==Kind::WholeSample;
    if(b.valid&&b.count){b.rawStart=mtProject.instrument[b.instrument].slices[b.slice];if(!mtProject.instrument[b.instrument].slicePlayThru&&b.slice+1<b.count)b.rawEnd=mtProject.instrument[b.instrument].slices[b.slice+1];}
    b.start=static_cast<uint32_t>(static_cast<uint64_t>(b.rawStart)*b.length/65535);b.end=static_cast<uint32_t>(static_cast<uint64_t>(b.rawEnd)*b.length/65535);return b;}
struct Context{Selection main;Selection technical[2];Bounds mainBounds,technicalBounds[2];GeometryKey mainKey[2],technicalKey[2];bool hasTechnical[2]{},technicalPlanned[2]{};bool isTechnical=false,preclosed=false;unsigned plans=0;};
static Context*current=nullptr;
static std::function<void()>beforeCommit;
static bool refreshTechnical=false;
class playerEngine;
static void prepareTechnical(playerEngine&,Context&);
static bool bindLive(playerEngine&,Context&);
#ifdef A1_IO
#include "io-stream-declarations.inc"
#else
struct {template<class...T>void notifyNoteOn(T...){}template<class...T>void getSlicePreCache(T&&...){}}voiceStreamManager;
static uint32_t getMaxReadableFramesForAddress(int16_t*,uint32_t n){return n;}
#endif
class AudioPlayMemory:public AudioStream {
public:
#ifdef A1_FIRMWARE_BRIDGE
    enum {envelopePhaseIdle=::envelopePhaseIdle,envelopePhaseDelay=::envelopePhaseDelay,envelopePhaseForced=::envelopePhaseForced};
    wr2native::State wr2State;
    void wr2Queue(uint8_t,int8_t);void wr2Cancel();void wr2ClearActive();uint8_t wr2Slice(uint8_t,uint8_t)const;bool wr2Prepared(uint8_t,uint16_t,uint16_t);void wr2Commit(uint8_t);
#endif
#include "envelope-fields.inc"
    struct {bool enable=false;uint8_t instr_idx=0;int8_t note=0;bool isPrev=false;}stackedPlay;
    unsigned channel=0;uint8_t currentInstrIdx=0,forcedSlice=0;bool sliceForcedFlag=false,playing=false;
    Selection active,pending;bool activeValid=false,pendingValid=false;unsigned actualInstr=0,actualSlice=0;Timer clock;Timer*timer=&clock;
    Selection fixtureSelection;Bounds fixtureBounds;bool fixtureHasPoints=false,fixtureUseSelection=true;
    GeometryKey fixtureKey,committedKey;bool committedValid=false;
    const int16_t*currentSliceCacheAddress=nullptr;uint32_t currentSliceCacheFrames=0;
    int currentInstrIdxInPlay=0,lastInstrIdxInPlay=0,lastNote=0;bool pointsForceFlag=false,currentIsStreamed=false,reverseDirectionFlag=false;int voiceIndex=-1,currentPlayMode=0,iPitchCounter=0;
    uint16_t forcedStartPoint=0,forcedEndPoint=65535,forcedLoopPoint1=0,forcedLoopPoint2=65535;uint32_t currentSampleLength=65536,length=0;float pitchControl=1;
    inline static int16_t samples[65536]{};int16_t*currentStartAddress=samples;int16_t*next=nullptr;AudioPlayMemory*samplePointsSource=nullptr;
    struct{uint32_t start=0,end=0,loop1=0,loop2=0;}pointsInSamples;
    struct{uint32_t endPoint=0,loopPoint1=0,loopPoint2=0,loopLength=0;}constrainsInSamples;
    audio_block_t storageBlock;
    void setCurrentInstrIdx(uint8_t i){currentInstrIdx=i;}
    void setForcedPoints(int32_t,int32_t,int32_t,int32_t);void setPointsForceFlag();
    void setSliceForcedFlag(){sliceForcedFlag=true;}void clearSliceForcedFlag(){sliceForcedFlag=false;}
    void setForcedSlice(uint8_t v){auto c=mtProject.instrument[currentInstrIdx].sliceNumber;forcedSlice=static_cast<uint8_t>(c?std::min<unsigned>(v,c-1):0);}
#ifdef A1_REAL_AMP
    void envelopeSetSustain();uint8_t envelopeGetState();
#endif
    void envelopeSetIsRandom(bool b){envelope.isRandom=b;}
    void setSlideOnPlay(bool,int,float){}
    void envelopeRelease(float){}void stop(){playing=false;}
    audio_block_t*allocate(){return &storageBlock;}void transmit(audio_block_t*){}
    void envelopeNoteOnBody(uint8_t,int8_t);void envelopeNoteOnForPrev(uint8_t,int8_t);void envelopeNoteOffBody();void envelopeSetIdle();void envelopeUpdate(audio_block_t*);
    Selection eventSelection(uint8_t i,int8_t n){Selection s;s.instrument=i;s.sliceCount=mtProject.instrument[i].sliceNumber;s.kind=mtProject.instrument[i].playMode==5?(s.sliceCount?Kind::Slice:Kind::WholeSample):Kind::Native;s.slice=s.sliceCount?static_cast<uint8_t>(sliceForcedFlag?forcedSlice:std::clamp<int>(n-24,0,s.sliceCount-1)):0;
        if(current&&!(nativeDomains&&mtProject.instrument[i].playMode!=5)){if(current->isTechnical){if(current->hasTechnical[channel]&&current->technical[channel].instrument==i)s=current->technical[channel];else if(current->main.instrument==i)s=current->main;}else s=current->main;}return s;}
    void envelopeNoteOn(uint8_t i,int8_t n){auto s=eventSelection(i,n);if(envelope.state!=envelopePhaseIdle&&envelope.state!=envelopePhaseDelay&&envelope.state!=envelopePhaseForced){pending=s;pendingValid=true;}else pendingValid=false;envelopeNoteOnBody(i,n);}
    void envelopeNoteOff(){pendingValid=false;envelopeNoteOffBody();}
    void envelopeSwitchPhase(uint8_t);uint16_t envelopeMilliseconds2count(float);
    void playForPrev(uint8_t i,int8_t n){play(i,n);}
    void playSingleShot(uint8_t,int8_t);void playLoopForward(uint8_t,int8_t);
    void playBeatSlice(uint8_t,int8_t);void playSlice(uint8_t,int8_t);void playWavetable(uint8_t,int8_t);void playGranular(uint8_t,int8_t);
    static std::tuple<uint16_t,uint16_t,uint16_t,uint16_t>calculateGranularPoints(uint16_t,uint16_t,uint32_t);
    inline static float wt32Note[120]{},wt64Note[120]{},wt128Note[120]{},wt256Note[120]{},wt512Note[120]{},wt1024Note[120]{},wt2048Note[120]{};float*wt_notes=wt32Note;
    inline static uint16_t squareTab[1000]{},gaussTab[1000]{},triangleTab[1000]{};
    uint16_t wavetableWindowSize=32;uint32_t currentWindow=0;bool wavetableWindowForceFlag=false;uint32_t forcedWavetableWindow=0;
    uint8_t granularLoopType=0;const uint16_t*granularEnvelopeTab=nullptr;bool granularForcedFlag=false,granularLengthForcedFlag=false,granularPositionRefreshFlag=false;uint16_t forcedGranularPosition=0,forcedGranularLength=100,currentGranularPosition=0,currentGranularLength=100;

#ifdef A1_IO
    bool needSmoothingFlag=false,loopBackwardFlag=false,glideForceFlag=false,tuneForceFlag=false,fineTuneForceFlag=false;
    float fPitchCounter=0,currentGlide=0,forcedGlide=0,currentTune=0,forcedTune=0,currentFineTune=0,forcedFineTune=0;
    unsigned pitchSmoothingControlCounter=0,currentPreCacheFrames=0;
    void refreshStartParameters();
#else
    void refreshStartParameters(){
#ifdef A1_IO
        const auto&m=mtProject.sampleMemory[currentInstrIdx];currentIsStreamed=m.isStreamed;currentStartAddress=m.address[channel];currentSliceCacheAddress=nullptr;currentSliceCacheFrames=0;
#endif
        currentPlayMode=mtProject.instrument[currentInstrIdx].playMode;currentSampleLength=mtProject.instrument[currentInstrIdx].sampleLength;}
#endif
    void constrainCurrentTune(int8_t){}void setStartPitch(int8_t){}void applyFinetuneOnPitch(int8_t){}void calculateGlidePitch(int8_t){}
    inline static bool isSnapToZeroEnabled=true;static constexpr int16_t kSnapToZeroThreshold=10;
    static uint32_t snapNativeBody(uint16_t,uint32_t,int16_t*);
    static std::tuple<int16_t*,int16_t*,int16_t*>calculatePositionZoneAddresses(uint16_t,uint32_t,int16_t*);
    static bool isSampleInZeroCrossing(int16_t&,const int16_t*,const int16_t*);
    GeometryKey requestedKey(const Selection&s)const{
        const auto&i=mtProject.instrument[s.instrument];GeometryKey k;k.address=reinterpret_cast<std::uintptr_t>(samples);k.length=i.sampleLength;k.pcmRevision=i.pcmRevision;k.instrument=s.instrument;k.mode=i.playMode;k.snap=isSnapToZeroEnabled;
        if(i.playMode==5){k.count=i.sliceNumber;k.slice=s.slice;if(k.count){k.rawStart=i.slices[s.slice];k.rawEnd=(!i.slicePlayThru&&s.slice+1<k.count)?i.slices[s.slice+1]:65535;}else{k.rawStart=0;k.rawEnd=65535;}}
        else{k.rawStart=pointsForceFlag?forcedStartPoint:i.startPoint;k.rawEnd=pointsForceFlag?forcedEndPoint:i.endPoint;}
        return k;
    }
    static Bounds computeOutside(const Selection&s,const GeometryKey&k){
        const bool masked=Registers::mask||Registers::primask||!Registers::nvic;
        CHECK(!masked||expectMaskedPreparation);
        if(masked)maskedPreparationCalls+=2;
        Bounds b;b.valid=true;b.instrument=s.instrument;b.slice=s.slice;b.count=s.sliceCount;b.length=k.length;b.rawStart=k.rawStart;b.rawEnd=k.rawEnd;
        ++slowCalls;b.start=snapNativeBody(k.rawStart,k.length,reinterpret_cast<int16_t*>(k.address));
        ++slowCalls;b.end=snapNativeBody(k.rawEnd,k.length,reinterpret_cast<int16_t*>(k.address));return b;
    }
    Bounds nativeBounds()const{Bounds b;b.valid=true;b.instrument=active.instrument;b.slice=active.slice;b.count=active.sliceCount;b.length=committedKey.length;b.rawStart=committedKey.rawStart;b.rawEnd=committedKey.rawEnd;b.start=pointsInSamples.start;b.end=pointsInSamples.end;return b;}
    // Commit metadata BEFORE the native method enables IRQ. Derived geometry stays
    // in pointsInSamples; only a freshness key is added, no duplicate PCM cache.
    void commitNativeGeometry(uint8_t i,uint16_t rawStart,uint16_t rawEnd){
        wr2minimal::Guard<Registers> guard;
        committedKey=requestedKey(fixtureSelection);committedKey.instrument=i;committedKey.rawStart=rawStart;committedKey.rawEnd=rawEnd;committedKey.length=currentSampleLength;committedKey.mode=static_cast<uint8_t>(currentPlayMode);committedValid=true;
        if(resolveBlockers&&!(nativeDomains&&currentPlayMode!=5)){active=fixtureSelection;activeValid=true;actualInstr=i;actualSlice=active.slice;starts.push_back({i,active.slice,channel,current&&current->isTechnical,pointsInSamples.start,pointsInSamples.end,static_cast<unsigned>(currentPlayMode),pointsInSamples.loop1,pointsInSamples.loop2,constrainsInSamples.endPoint});}
    }
    bool applyPrepared(uint16_t rawStart,uint16_t rawEnd){
        if(!resolveBlockers||(nativeDomains&&currentPlayMode!=5))return false;
        const auto live=requestedKey(fixtureSelection);
        CHECK(fixtureHasPoints&&fixtureBounds.valid&&sameKey(live,fixtureKey)&&rawStart==fixtureBounds.rawStart&&rawEnd==fixtureBounds.rawEnd);
        pointsInSamples.start=fixtureBounds.start;pointsInSamples.end=fixtureBounds.end;return true;
    }
    uint32_t calculatePointPositionAndSnapToZero(uint16_t p,uint32_t len,int16_t*address){
        if(baselineNative||(nativeDomains&&currentPlayMode!=5))return snapNativeBody(p,len,address);
        if(resolveBlockers){++slowCalls;if(Registers::mask)++slowUnderMask;if(Registers::mask||Registers::primask||!Registers::nvic)++pcmWithAnyMask;CHECK(!Registers::mask&&!Registers::primask&&Registers::nvic);return snapNativeBody(p,len,address);}
        if(currentPlayMode==5&&fixtureHasPoints&&fixtureBounds.valid&&fixtureBounds.instrument==currentInstrIdx&&fixtureBounds.slice==fixtureSelection.slice&&fixtureBounds.length==len){if(p==fixtureBounds.rawStart)return fixtureBounds.start;if(p==fixtureBounds.rawEnd)return fixtureBounds.end;}
        ++slowCalls;if(Registers::mask)++slowUnderMask;return static_cast<uint32_t>((static_cast<uint64_t>(p)*len)/65535);
    }
    bool technicalTrace()const{
#ifdef A1_FIRMWARE_BRIDGE
        return wr2State.frame&&wr2State.frame->technicalMode_;
#else
        return false;
#endif
    }
    void play(uint8_t i,int8_t n){
#ifdef A1_FIRMWARE_BRIDGE
        if(mtProject.instrument[i].playMode!=5)wr2ClearActive();
        wr2native::Frame wr2Playback(*this,i,n);
#endif
        // The two resume paths run in AudioIRQ/OfflineRender. Prepare before the
        // legacy play function disables output IRQ; no new draw on resume.
        Bounds resumeBounds;GeometryKey resumeKey;bool resumePrepared=false;
        if(resolveBlockers&&!current&&pendingValid){resumeKey=requestedKey(pending);resumeBounds=computeOutside(pending,resumeKey);resumePrepared=true;}
        AudioNoInterrupts();
        Selection use=eventSelection(i,n);
        bool planned=false;
        if(current&&!(nativeDomains&&mtProject.instrument[i].playMode!=5)){
            if(current->isTechnical){
                if(current->hasTechnical[channel]&&current->technical[channel].instrument==i){use=current->technical[channel];planned=current->technicalPlanned[channel];}
                else if(current->main.instrument==i){use=current->main;planned=current->plans;}
            }else{use=current->main;planned=current->plans;}
        }else if(pendingValid){use=pending;pendingValid=false;}
        CHECK(use.instrument==i);
        fixtureSelection=use;fixtureHasPoints=planned;fixtureBounds={};
        if(current&&planned){if(current->isTechnical&&current->hasTechnical[channel]&&current->technical[channel].instrument==i){fixtureBounds=current->technicalBounds[channel];fixtureKey=current->technicalKey[channel];}else{fixtureBounds=current->mainBounds;fixtureKey=current->mainKey[channel];}}
        if(resumePrepared){fixtureHasPoints=true;fixtureBounds=resumeBounds;fixtureKey=resumeKey;}
        if(mtProject.instrument[i].playMode>=1&&mtProject.instrument[i].playMode<=3){playLoopForward(i,n);}
        else if(mtProject.instrument[i].playMode==4){playSlice(i,n);}
        else if(mtProject.instrument[i].playMode==6){playWavetable(i,n);}
        else if(mtProject.instrument[i].playMode==7){playGranular(i,n);}
        else if(mtProject.instrument[i].playMode!=5){playSingleShot(i,n);if(!resolveBlockers&&current&&current->plans)++missingPlan;}
        else {playBeatSlice(i,n);if(!planned&&current&&current->plans)++missingPlan;}
#ifdef A1_FIRMWARE_BRIDGE
        if(wr2State.activeValid&&mtProject.instrument[i].playMode==5)use=wr2State.active;
#endif
        if(!resolveBlockers||(nativeDomains&&mtProject.instrument[i].playMode!=5)){active=use;activeValid=true;actualInstr=i;actualSlice=use.slice;playing=true;starts.push_back({i,use.slice,channel,baselineTechnical||(current&&current->isTechnical)||technicalTrace(),pointsInSamples.start,pointsInSamples.end,static_cast<unsigned>(currentPlayMode),pointsInSamples.loop1,pointsInSamples.loop2,constrainsInSamples.endPoint});}
        AudioInterrupts();
    }
};
class playerEngine {
public:
    using controlType=::controlType;using parameterList=::parameterList;
    AudioPlayMemory left,right;std::vector<AudioPlayMemory*>playMemPtr{&left,&right};
    FakeEnv env[8];std::array<FakeEnv*,8>envelopePtr{};
    bool isBlockedNoteOn[48]{},fenceAtPair=false;uint8_t currentInstrument_idx=0;int8_t currentNote=24,stepVelocity=127;
    uint8_t lastSeqFx[2]{},lastSeqVal[2]{};bool trackControlParameter[3][32]{};bool isTrackDisplayed=false,onEndDisplay=false,envelopePassFlag=false;
    unsigned stubCalls=0,currentEnvelopeModification[8]{},envelopesControlValue[8]{};
    struct{int endPoint=0,startPoint=0,wavetablePosition=0,tune=0,lfoAmpRate=0;}performanceMod;
    struct{uint8_t slice=0,lfoAmpRate=0,lfoAmpEnable=0;}currentSeqModValues;
    playerEngine(){right.channel=1;right.samplePointsSource=&left;for(unsigned i=0;i<8;++i){envelopePtr[i]=&env[i];envelopesControlValue[i]=i;}for(auto p:playMemPtr){p->envelope.attack_count=1;p->envelope.decay_count=1;p->envelope.sustain_mult=0x20000000;p->envelope.release_count=1;p->envelope.release_forced_count=1;}}
    uint8_t noteOn(uint8_t,int8_t,int8_t);uint8_t noteOn(uint8_t,int8_t,int8_t,uint8_t,uint8_t,uint8_t,uint8_t);
    void noteOff(int8_t=-2);void noteOffFade();void noteOffCut();void noteOffOrdinary();
    void seqFx(uint8_t,uint8_t,uint8_t);
#ifndef A1_NATIVE_BASELINE
    void seqFxBody(uint8_t,uint8_t,uint8_t);
#endif
    void endFx(uint8_t,uint8_t);uint8_t isFxVelocity(uint8_t);
    void fxSampleSlice(uint8_t,uint8_t);void endFxSlice(uint8_t);void setFxSlice();void clearFxSlice();
    void fxVolumeLFO(uint8_t,uint8_t);void endFxVolumeLFO(uint8_t);void syncFxAmpLFOBody();
    void syncFxAmpLFO(){
#ifdef A1_FIRMWARE_BRIDGE
        {const bool saved=baselineTechnical;baselineTechnical=true;syncFxAmpLFOBody();baselineTechnical=saved;return;}
#endif
        if(baselineNative||(nativeDomains&&mtProject.instrument[currentInstrument_idx].playMode!=5)){const bool saved=baselineTechnical;baselineTechnical=true;syncFxAmpLFOBody();baselineTechnical=saved;return;}
        Context local;auto old=current;
        std::optional<wr2minimal::StereoFence<Registers>> fence;
        if(!current){local.main.instrument=currentInstrument_idx;local.main.sliceCount=mtProject.instrument[currentInstrument_idx].sliceNumber;local.main.kind=Kind::Slice;local.main.slice=local.main.sliceCount?static_cast<uint8_t>(std::clamp<int>(currentNote-24,0,local.main.sliceCount-1)):0;
            for(unsigned ch=0;ch<2;++ch)if(playMemPtr[ch]->activeValid){local.hasTechnical[ch]=true;local.technical[ch]=playMemPtr[ch]->active;}
            if(resolveBlockers){local.main=left.eventSelection(currentInstrument_idx,currentNote);local.plans=2;for(unsigned ch=0;ch<2;++ch)local.mainKey[ch]=playMemPtr[ch]->requestedKey(local.main);local.mainBounds=AudioPlayMemory::computeOutside(local.main,local.mainKey[0]);prepareTechnical(*this,local);fence.emplace();if(!bindLive(*this,local)){notReady=true;return;}}current=&local;}
        else if(resolveBlockers)CHECK(bindLive(*this,*current));
        bool prior=current->isTechnical;current->isTechnical=true;syncFxAmpLFOBody();current->isTechnical=prior;current=old;
    }
    void handleInitNoteOnAllEnvelopes(){AudioNoInterrupts();AudioInterrupts();}
    void handleInitFxNoteOnAllEnvelopes(){handleInitNoteOnAllEnvelopes();}
    void handleNoteOnFilter(){AudioNoInterrupts();AudioInterrupts();}
    void handleNoteOnGain(){AudioNoInterrupts();AudioInterrupts();}
    void handleNoteOnPanning(){}void handleNoteOnDelaySend(){}void handleNoteOnReverbSend(){}void handleNoteOnBitCrusher(){}
    void handleFxNoteOnFilter(){handleNoteOnFilter();}void handleFxNoteOnGain(){handleNoteOnGain();}void handleFxNoteOnPanning(){}void handleFxNoteOnBitDepth(){}void handleFxNoteOnDelaySend(){}void handleFxNoteOnReverbSend(){}
    bool isActiveEnvelope(unsigned i){return mtProject.instrument[currentInstrument_idx].isEnvEnabled(i);}
    void changeEndPointPerformanceMode(float){}void changeStartPointPerformanceMode(int){}void changeWavetableWindowPerformanceMode(int){}void changeTunePerformanceMode(int){}
    void setSyncParamsLFO(unsigned){}void setFxAmpRateLFO(){}
#ifdef A1_REAL_AMP
    Env lfoBasedEnvelope[8];
    void clearFxAmpRateLFO();void changeAmpLfoRatePerformanceMode(int){}
    void calcLfoBasedEnvelope(Env*,Lfo*,int,int,int){}
    void initEnvelopesParameters(unsigned,Env*){}
#else
    void clearFxAmpRateLFO(){}
#endif
#include "fx-stubs.inc"
};
#ifdef A1_FIRMWARE_BRIDGE
#include "../../../core/AudioEngine/NativeBridge.cpp"
#endif
#pragma warning(push)
#pragma warning(disable:4244)
#ifdef A1_NATIVE_BASELINE
#include "native-baseline.inc"
#else
#include "native.inc"
#endif
#pragma warning(pop)
static bool usable(const Selection&s){if(s.instrument>=48)return false;const auto&i=mtProject.instrument[s.instrument];return ((i.playMode==0&&s.kind==Kind::Native)||(i.playMode==5&&s.sliceCount==i.sliceNumber&&((i.sliceNumber==0&&s.kind==Kind::WholeSample)||(i.sliceNumber>0&&s.kind==Kind::Slice&&s.slice<i.sliceNumber))));}
static void prepareTechnical(playerEngine&e,Context&c){
    // Reuse native derived points when the freshness tag matches; otherwise
    // prepare one local replacement OUTSIDE the fence. No persistent copy cache.
    for(unsigned ch=0;ch<2;++ch){auto&p=*e.playMemPtr[ch];bool already=false;
        if(nativeDomains&&(!p.activeValid||mtProject.instrument[p.active.instrument].playMode!=5)){c.hasTechnical[ch]=false;continue;}
        {wr2minimal::Guard<Registers>g;if(!p.activeValid){c.hasTechnical[ch]=false;continue;}c.hasTechnical[ch]=true;c.technical[ch]=p.active;c.technicalKey[ch]=p.requestedKey(p.active);
            if(usable(p.active)&&p.committedValid&&sameKey(p.committedKey,c.technicalKey[ch])){c.technicalBounds[ch]=p.nativeBounds();already=true;}}
        if(!already&&usable(c.technical[ch])){
            if(hoistFxPreparation&&ch==1&&p.samplePointsSource==e.playMemPtr[0]&&c.technicalPlanned[0]&&sameKey(c.technicalKey[0],c.technicalKey[1]))c.technicalBounds[1]=c.technicalBounds[0];
            else c.technicalBounds[ch]=AudioPlayMemory::computeOutside(c.technical[ch],c.technicalKey[ch]);
        }
        c.technicalPlanned[ch]=usable(c.technical[ch])&&c.technicalBounds[ch].valid;
    }
}
static bool bindLive(playerEngine&e,Context&c){
    // Caller holds the fence: observe identity + existing points + key together.
    for(unsigned ch=0;ch<2;++ch){auto&p=*e.playMemPtr[ch];if(nativeDomains&&p.activeValid&&mtProject.instrument[p.active.instrument].playMode!=5){c.hasTechnical[ch]=false;continue;}if(!p.activeValid){c.hasTechnical[ch]=false;continue;}
        if(!usable(p.active))return false;const auto live=p.requestedKey(p.active);
        if(p.committedValid&&sameKey(p.committedKey,live)){c.technical[ch]=p.active;c.technicalKey[ch]=live;c.technicalBounds[ch]=p.nativeBounds();c.hasTechnical[ch]=c.technicalPlanned[ch]=true;}
        else if(!(c.hasTechnical[ch]&&c.technicalPlanned[ch]&&c.technical[ch].instrument==p.active.instrument&&c.technical[ch].kind==p.active.kind&&c.technical[ch].slice==p.active.slice&&sameKey(c.technicalKey[ch],live)))return false;
    }
    return true;
}
#ifndef A1_NATIVE_BASELINE
void playerEngine::seqFx(uint8_t id,uint8_t value,uint8_t slot){
#ifdef A1_FIRMWARE_BRIDGE
    seqFxBody(id,value,slot);return;
#endif
    if(!resolveBlockers||!hoistFxPreparation||current||(nativeDomains&&mtProject.instrument[currentInstrument_idx].playMode!=5)){seqFxBody(id,value,slot);return;}
    Context c;
    {wr2minimal::Guard<Registers> snapshot;
        c.main=left.activeValid&&left.active.instrument==currentInstrument_idx?left.active:
            left.pendingValid&&left.pending.instrument==currentInstrument_idx?left.pending:left.eventSelection(currentInstrument_idx,currentNote);
    }
    prepareTechnical(*this,c);
    for(unsigned ch=0;ch<2;++ch)c.mainKey[ch]=playMemPtr[ch]->requestedKey(c.main);
    if(c.hasTechnical[0]&&c.technical[0].instrument==c.main.instrument&&c.technical[0].slice==c.main.slice&&sameKey(c.technicalKey[0],c.mainKey[0]))c.mainBounds=c.technicalBounds[0];
    else c.mainBounds=AudioPlayMemory::computeOutside(c.main,c.mainKey[0]);
    c.plans=2;
    if(beforeCommit){auto hook=std::move(beforeCommit);beforeCommit={};hook();}
    wr2minimal::StereoFence<Registers> fence;
    if(!bindLive(*this,c)){notReady=true;return;}
    auto saved=current;current=&c;seqFxBody(id,value,slot);current=saved;
}
#endif
static Request request(unsigned i,int note){Request r;r.enabled=true;r.instrument=i;r.note=note;r.sliceCount=mtProject.instrument[i].sliceNumber;r.topology=1;r.beatSlice=mtProject.instrument[i].playMode==5;return r;}
static void reset(){restoreHook={};restoreCountdown=0;nativeDomains=false;hoistFxPreparation=false;baselineNative=false;resetWork();Registers::reset();starts.clear();slowUnderMask=missingPlan=slowCalls=boundary=pcmWithAnyMask=0;injectBoundary=~0u;current=nullptr;beforeCommit={};refreshTechnical=resolveBlockers=notReady=false;AudioPlayMemory::isSnapToZeroEnabled=true;for(auto&i:mtProject.instrument){i.isActive=1;i.sliceNumber=4;i.playMode=5;i.startPoint=0;i.endPoint=65535;i.sampleLength=65536;i.pcmRevision=1;for(unsigned n=0;n<48;++n)i.slices[n]=static_cast<uint16_t>(n*1000);}}
static PublishedBank bank;static Random rng(1234);
// Candidate integration ONLY in fixture: actual NoteOn body called whole.
static uint8_t trigger(playerEngine&e,unsigned i,int note,bool fx=false,unsigned f1=0,unsigned v1=0,unsigned f2=0,unsigned v2=0,unsigned planned=0,bool pairOnly=false,int velocity=-1){
    bool preclosed=false;
    if(nativeDomains){
        if(i>=48||!mtProject.instrument[i].isActive||e.isBlockedNoteOn[i])return 0;
        if(mtProject.instrument[i].playMode!=5)return fx?e.noteOn(static_cast<uint8_t>(i),static_cast<int8_t>(note),static_cast<int8_t>(velocity),static_cast<uint8_t>(f1),static_cast<uint8_t>(v1),static_cast<uint8_t>(f2),static_cast<uint8_t>(v2)):e.noteOn(static_cast<uint8_t>(i),static_cast<int8_t>(note),127);
        if(!fx){e.endFx(e.lastSeqFx[0],0);e.endFx(e.lastSeqFx[1],1);preclosed=true;}
    }
    auto r=request(i,note);r.directSlice=fx&&(f1==Sequencer::strFxConsts::FX_TYPE_SAMPLE_SLICE||f2==Sequencer::strFxConsts::FX_TYPE_SAMPLE_SLICE);
    if(r.directSlice)r.forcedSlice=r.sliceCount?(f1==Sequencer::strFxConsts::FX_TYPE_SAMPLE_SLICE?v1:v2)%r.sliceCount:0;
    Context c;c.preclosed=preclosed;c.main=wr2minimal::choose<Registers>(wr2minimal::prepare(bank,r),rng);c.plans=planned;if(planned)c.mainBounds=prepareBounds(c.main);
    for(unsigned ch=0;ch<2;++ch)if(e.playMemPtr[ch]->activeValid){c.hasTechnical[ch]=true;c.technical[ch]=e.playMemPtr[ch]->active;c.technicalPlanned[ch]=planned>=2&&c.technical[ch].kind!=Kind::Native;if(c.technicalPlanned[ch])c.technicalBounds[ch]=prepareBounds(c.technical[ch]);}
    if(resolveBlockers){for(unsigned ch=0;ch<2;++ch)c.mainKey[ch]=e.playMemPtr[ch]->requestedKey(c.main);c.mainBounds=AudioPlayMemory::computeOutside(c.main,c.mainKey[0]);c.plans=2;prepareTechnical(e,c);}
    if(beforeCommit){auto hook=std::move(beforeCommit);beforeCommit={};hook();}
    auto saved=current;current=&c;uint8_t result;
    // pairOnly deliberately omits the fence to expose preparation interleaving.
    auto call=[&]{if(fx)return e.noteOn(static_cast<uint8_t>(i),static_cast<int8_t>(note),static_cast<int8_t>(velocity),static_cast<uint8_t>(f1),static_cast<uint8_t>(v1),static_cast<uint8_t>(f2),static_cast<uint8_t>(v2));return e.noteOn(static_cast<uint8_t>(i),static_cast<int8_t>(note),127);};
    if(pairOnly)result=call();else{wr2minimal::StereoFence<Registers>f;
        if(resolveBlockers){bool valid=true;for(unsigned ch=0;ch<2;++ch)valid=valid&&sameKey(c.mainKey[ch],e.playMemPtr[ch]->requestedKey(c.main));if(!valid||!bindLive(e,c)){current=saved;notReady=true;return 0;}}
        if(refreshTechnical)for(unsigned ch=0;ch<2;++ch){const auto&live=e.playMemPtr[ch]->active;if(c.hasTechnical[ch]&&(live.instrument!=c.technical[ch].instrument||live.slice!=c.technical[ch].slice)){c.technical[ch]=live;c.technicalPlanned[ch]=false;}}
        result=call();}
    current=saved;return result;
}
static void setup(){InstrumentState s;s.reset(4);create(s,0);add(s,0,1);add(s,0,2);setWeight(s,0,0,0);CHECK(bank.instrument[0].publish(s,1)==PublishResult::Published);CHECK(bank.instrument[1].publish(s,1)==PublishResult::Published);}
static void basic(){
    reset();setup();playerEngine e;auto before=rng.state();CHECK(trigger(e,0,24));CHECK(rng.state()!=before);CHECK(e.left.actualSlice==e.right.actualSlice);CHECK(starts.size()==2);
    auto S=static_cast<uint8_t>(Sequencer::strFxConsts::FX_TYPE_SAMPLE_SLICE);
    for(unsigned a=0;a<8;++a)for(unsigned b=0;b<8;++b){playerEngine f;before=rng.state();CHECK(trigger(f,0,24,true,S,a,S,b));CHECK(f.left.actualSlice==a%4&&f.right.actualSlice==a%4);CHECK(rng.state()==before);}
    for(auto fx:{Sequencer::strFxConsts::FX_TYPE_VOLUME,Sequencer::strFxConsts::FX_TYPE_ROLL}){playerEngine f;before=rng.state();CHECK(trigger(f,0,24,true,S,3,fx,7,2,false,63));CHECK(f.lastSeqFx[1]==Sequencer::strFxConsts::FX_TYPE_VOLUME);CHECK(f.lastSeqVal[1]==63&&f.stepVelocity==63);CHECK(f.left.actualSlice==3&&f.right.actualSlice==3);CHECK(rng.state()==before);}
    std::puts("PASS full NoteOn entries + native seqFx/endFx/S2: one draw and FX1 precedence; S2 no WR2 draw");
}
static void pending(bool fixed=false){
    for(unsigned wt:{0u,2u}){reset();setup();playerEngine e;
        resolveBlockers=fixed;
        for(auto p:e.playMemPtr){p->envelope.state=envelopePhaseSustain;p->envelope.wtMode=static_cast<uint8_t>(wt);}
        CHECK(trigger(e,0,24));auto after=rng.state();CHECK(e.left.stackedPlay.enable&&e.right.stackedPlay.enable);CHECK(starts.empty());
        for(auto p:e.playMemPtr){p->envelope.count=0;p->envelopeUpdate(&p->storageBlock);CHECK(!p->stackedPlay.enable);CHECK(p->playing);}
        CHECK(e.left.actualSlice==e.right.actualSlice);CHECK(rng.state()==after);
        for(auto p:e.playMemPtr)p->envelope.state=envelopePhaseSustain;
        CHECK(trigger(e,0,24));CHECK(e.left.stackedPlay.enable);e.noteOff(Sequencer::STEP_NOTE_OFF_CUT);CHECK(!e.left.stackedPlay.enable&&!e.right.stackedPlay.enable);
    }
    std::puts("PASS full native envelopeUpdate: both Forced consumers, deferred stereo, no resume draw and native NoteOff cancellation");
}
static void armLfo(playerEngine&e){auto L=static_cast<uint8_t>(Sequencer::strFxConsts::FX_TYPE_VOLUME_LFO);e.lastSeqFx[0]=e.lastSeqFx[1]=static_cast<uint8_t>(L);e.lastSeqVal[1]=9;e.trackControlParameter[0][static_cast<unsigned>(parameterList::lfoAmp)]=true;e.trackControlParameter[1][static_cast<unsigned>(parameterList::lfoAmp)]=true;}
static void lfo(){
    auto L=static_cast<uint8_t>(Sequencer::strFxConsts::FX_TYPE_VOLUME_LFO);
    for(bool fx:{false,true})for(unsigned plans:{0u,1u,2u}){
        reset();setup();InstrumentState s;s.reset(4);create(s,0);add(s,0,2);setWeight(s,0,0,0);CHECK(bank.instrument[0].publish(s,1)==PublishResult::Published);
        playerEngine e;CHECK(trigger(e,0,25));CHECK(e.left.actualSlice==1);armLfo(e);starts.clear();slowUnderMask=missingPlan=0;
        auto expected=rng;CHECK(trigger(e,0,24,fx,0,0,L,8,plans));expected.percent();CHECK(rng.state()==expected.state());CHECK(starts.size()==4);
        for(unsigned ch=0;ch<2;++ch){CHECK(starts[ch].technical&&starts[ch].slice==1&&starts[ch].instrument==0);CHECK(starts[ch+2].slice==2&&!starts[ch+2].technical);}
        if(plans==0)CHECK(slowUnderMask==4);else if(plans==1){CHECK(slowUnderMask==2);CHECK(missingPlan==2);}else CHECK(slowUnderMask==0&&missingPlan==0);
        auto before=rng.state();starts.clear();e.syncFxAmpLFO();CHECK(rng.state()==before);CHECK(starts.size()==2);CHECK(starts[0].slice==2&&starts[1].slice==2);
    }
    // A restart inside preparation for another instrument uses this NEW musical
    // frame, not the old instrument's descriptor. It must never draw separately.
    reset();setup();playerEngine e;CHECK(trigger(e,0,25));armLfo(e);starts.clear();auto expected=rng;
    CHECK(trigger(e,1,24,true,0,0,L,8,2));expected.percent();CHECK(rng.state()==expected.state());CHECK(starts.size()==4);
    for(auto&s:starts)CHECK(s.instrument==1);CHECK(e.left.actualSlice==e.right.actualSlice);
    std::puts("PASS native Amp LFO: old same-instrument slice retained; new-instrument frame never borrows old identity; one new-trigger draw, zero technical draws");
    std::puts("REPRO primary-only preparation: two technical starts need old active slice plan; two extra slow calls remain under fence");
}
static void interleavings(){
    reset();setup();playerEngine probe;CHECK(trigger(probe,0,24,true));const auto points=boundary;CHECK(points>4);
    unsigned inconsistent[3]{};
    for(unsigned mode=0;mode<3;++mode)for(unsigned at=0;at<points;++at){
        reset();setup();playerEngine e;e.fenceAtPair=mode==2;Registers::isr=[&]{CHECK(trigger(e,1,24,true,0,0,0,0,2));};injectBoundary=at;
        CHECK(trigger(e,0,24,true,0,0,0,0,2,mode!=1));
        if(mode==1){CHECK(e.left.actualInstr==1&&e.right.actualInstr==1);CHECK(e.currentInstrument_idx==1);CHECK(e.left.actualSlice==e.right.actualSlice);CHECK(Registers::mask==0);}
        else if(e.currentInstrument_idx!=e.left.actualInstr||e.left.actualInstr!=e.right.actualInstr)++inconsistent[mode];
    }
    CHECK(inconsistent[0]>0&&inconsistent[2]>0);std::printf("PASS %u fixture re-enable boundaries swept: inconsistent unguarded=%u, late-pair=%u, whole-entry=0\n",points,inconsistent[0],inconsistent[2]);
}
static void crossModeBlock(){
    reset();setup();playerEngine e;mtProject.instrument[2].playMode=0;CHECK(e.noteOn(2,60,127));armLfo(e);
    starts.clear();slowUnderMask=missingPlan=slowCalls=0;
    // Incoming target IS Beat Slice, but plain NoteOn closes the old LFO before
    // currentInstrument_idx changes. The native restart therefore plays slot 2.
    CHECK(trigger(e,0,24,false,0,0,0,0,2));
    CHECK(starts.size()==4);CHECK(starts[0].technical&&starts[0].instrument==2);CHECK(starts[1].technical&&starts[1].instrument==2);CHECK(starts[2].instrument==0&&starts[3].instrument==0);
    CHECK(slowUnderMask==2&&missingPlan==2);CHECK(e.currentInstrument_idx==0);
    std::puts("BLOCK REPRO: incoming Beat Slice NoteOn -> endFx(old LFO) -> syncFxAmpLFO -> native playSingleShot(old instrument) -> 2 zero-crossing calls under A1 fence");
    std::puts("MINIMUM REQUIRED: prepare/commit must cover technical old-mode playback too; preparing only incoming/active Beat Slice cannot satisfy no-PCM-under-mask");
}
static void edges(){
    reset();setup();playerEngine e;CHECK(e.noteOn(255,24,127)==0);CHECK(e.noteOn(255,24,127,0,0,0,0)==0);
    mtProject.instrument[0].isActive=0;CHECK(e.noteOn(0,24,127)==0);CHECK(e.noteOn(0,24,127,0,0,0,0)==0);mtProject.instrument[0].isActive=1;
    e.isBlockedNoteOn[0]=true;CHECK(e.noteOn(0,24,127)==0);CHECK(e.noteOn(0,24,127,0,0,0,0)==0);e.isBlockedNoteOn[0]=false;
    for(unsigned count=0;count<=48;++count){mtProject.instrument[0].sliceNumber=static_cast<uint8_t>(count);playerEngine f;auto before=rng.state();auto S=static_cast<uint8_t>(Sequencer::strFxConsts::FX_TYPE_SAMPLE_SLICE);CHECK(trigger(f,0,24,true,S,255,S,254,2));CHECK(rng.state()==before);CHECK(f.left.actualSlice==(count?255%count:0));CHECK(f.left.actualSlice==f.right.actualSlice);}
    mtProject.instrument[0].sliceNumber=4;
    for(unsigned phase:{envelopePhaseIdle,envelopePhaseDelay,envelopePhaseSustain,envelopePhaseForced}){playerEngine f;for(auto p:f.playMemPtr)p->envelope.state=static_cast<uint8_t>(phase);CHECK(trigger(f,0,24,true,0,0,0,0,2));CHECK(f.left.stackedPlay.enable==(phase==envelopePhaseSustain));CHECK(f.left.stackedPlay.enable==f.right.stackedPlay.enable);}
    for(int option:{-1,-2,-3}){playerEngine f;for(auto p:f.playMemPtr)p->envelope.state=envelopePhaseSustain;CHECK(trigger(f,0,24));f.noteOff(static_cast<int8_t>(option));CHECK(!f.left.stackedPlay.enable&&!f.right.stackedPlay.enable);CHECK(!f.left.pendingValid&&!f.right.pendingValid);}
    std::puts("PASS native bounds/guards, count 0..48 S2, all dispatch phases and fade/ordinary/cut cancellation");
}
static void stalePreparation(){
    auto L=static_cast<uint8_t>(Sequencer::strFxConsts::FX_TYPE_VOLUME_LFO);
    for(bool refresh:{false,true}){reset();setup();playerEngine e;CHECK(trigger(e,0,25));CHECK(e.left.actualSlice==1);armLfo(e);
        unsigned liveBefore=0;
        beforeCommit=[&]{Registers::pending=true;Registers::isr=[&]{CHECK(trigger(e,0,26,false,0,0,0,0,2));};Registers::service();liveBefore=e.left.actualSlice;CHECK(liveBefore==2);starts.clear();slowUnderMask=missingPlan=0;};
        refreshTechnical=refresh;CHECK(trigger(e,0,24,true,0,0,L,8,2));CHECK(starts.size()==4);
        if(!refresh){CHECK(starts[0].technical&&starts[0].slice==1);CHECK(starts[0].slice!=liveBefore);CHECK(slowUnderMask==0);}
        else{CHECK(starts[0].technical&&starts[0].slice==liveBefore);CHECK(slowUnderMask==2&&missingPlan==2);}
    }
    std::puts("BLOCK REPRO TOCTOU: IRQ replaces active slice 1->2 after preparation; keeping prepared token restarts stale slice 1");
    std::puts("BLOCK REPRO refresh: reading live slice 2 inside fence preserves identity but invalidates prepared bounds and invokes 2 slow calls under mask");
    std::puts("MINIMUM REQUIRED: publish the committed prepared geometry WITH active/pending identity, validate geometry revision, and reuse the matched pair; no token-only refresh");
}
static void delayedS2AndRetrigger(bool fixed=false){
    for(unsigned wt:{0u,2u}){reset();setup();playerEngine e;for(auto p:e.playMemPtr){p->envelope.state=envelopePhaseSustain;p->envelope.wtMode=static_cast<uint8_t>(wt);}
        resolveBlockers=fixed;
        auto S=static_cast<uint8_t>(Sequencer::strFxConsts::FX_TYPE_SAMPLE_SLICE);auto before=rng.state();CHECK(trigger(e,0,24,true,S,2,0,0,2));CHECK(rng.state()==before);
        e.currentSeqModValues.slice=0;e.setFxSlice();
        for(auto p:e.playMemPtr){p->envelope.count=0;p->envelopeUpdate(&p->storageBlock);CHECK(!p->stackedPlay.enable);CHECK(p->actualSlice==2&&p->active.direct);CHECK(p->sliceForcedFlag&&p->forcedSlice==0);}
        CHECK(rng.state()==before);
    }
    reset();setup();resolveBlockers=fixed;playerEngine e;for(auto p:e.playMemPtr)p->envelope.state=envelopePhaseSustain;
    auto expected=rng;CHECK(trigger(e,0,24));expected.percent();CHECK(e.left.stackedPlay.enable);CHECK(trigger(e,0,24));expected.percent();CHECK(rng.state()==expected.state());CHECK(!e.left.stackedPlay.enable&&!e.right.stackedPlay.enable);CHECK(!e.left.pendingValid&&!e.right.pendingValid);CHECK(e.left.actualSlice==e.right.actualSlice);
    std::puts("PASS full native delayed S2: frozen direct selection, live S2 untouched, zero RNG; identical Forced retrigger is one NEW draw");
}
static void fixedCrossMode(){
    for(bool snap:{false,true})for(bool edit:{false,true}){
        reset();setup();resolveBlockers=true;AudioPlayMemory::isSnapToZeroEnabled=snap;
        std::fill(std::begin(AudioPlayMemory::samples),std::end(AudioPlayMemory::samples),int16_t{1000});AudioPlayMemory::samples[16390]=0;
        auto&i=mtProject.instrument[2];i.playMode=0;i.startPoint=16384;i.endPoint=49152;playerEngine e;auto before=rng.state();CHECK(trigger(e,2,60));CHECK(rng.state()==before);armLfo(e);
        if(edit)i.startPoint=16416;
        const auto expectedStart=AudioPlayMemory::snapNativeBody(i.startPoint,65536,AudioPlayMemory::samples),expectedEnd=AudioPlayMemory::snapNativeBody(i.endPoint,65536,AudioPlayMemory::samples);
        starts.clear();slowUnderMask=pcmWithAnyMask=missingPlan=0;auto expectedRng=rng;
        CHECK(trigger(e,0,24,false,0,0,0,0,2));expectedRng.percent();CHECK(rng.state()==expectedRng.state());CHECK(starts.size()==4);
        for(unsigned ch=0;ch<2;++ch){CHECK(starts[ch].technical&&starts[ch].instrument==2);CHECK(starts[ch].start==expectedStart&&starts[ch].end==expectedEnd);CHECK(starts[ch+2].instrument==0);}
        CHECK(!notReady);CHECK(slowUnderMask==0&&pcmWithAnyMask==0&&missingPlan==0);CHECK(!Registers::mask&&Registers::nvic);
    }
    std::puts("FIXED original cross-mode counterexample: native Single Shot points reused or refreshed outside mask, exact native snap bounds, no foreign selection or extra draw");
}
static void fixedStalePreparation(){
    auto L=static_cast<uint8_t>(Sequencer::strFxConsts::FX_TYPE_VOLUME_LFO);
    for(unsigned repeat=0;repeat<64;++repeat){reset();setup();resolveBlockers=true;playerEngine e;CHECK(trigger(e,0,25));armLfo(e);uint32_t expectedStart=0,expectedEnd=0;
        beforeCommit=[&]{Registers::pending=true;Registers::isr=[&]{CHECK(trigger(e,0,26,false,0,0,0,0,2));};Registers::service();CHECK(e.left.active.slice==2&&e.right.active.slice==2);expectedStart=e.left.pointsInSamples.start;expectedEnd=e.left.pointsInSamples.end;starts.clear();slowUnderMask=pcmWithAnyMask=missingPlan=0;};
        auto expected=rng;CHECK(trigger(e,0,24,true,0,0,L,8,2));expected.percent();CHECK(rng.state()==expected.state());CHECK(starts.size()==4);
        for(unsigned ch=0;ch<2;++ch){CHECK(starts[ch].technical&&starts[ch].slice==2);CHECK(starts[ch].start==expectedStart&&starts[ch].end==expectedEnd);}
        CHECK(slowUnderMask==0&&pcmWithAnyMask==0&&missingPlan==0);CHECK(!notReady);
    }
    std::puts("FIXED original TOCTOU counterexample: live selection and native points read as one pair, 64 repetitions, no stale slice/geometry or masked PCM");
}
static void dirtyGeometryAfterPrepare(){
    reset();setup();resolveBlockers=true;playerEngine e;auto&i=mtProject.instrument[2];i.playMode=0;CHECK(trigger(e,2,60));armLfo(e);starts.clear();
    beforeCommit=[&]{Registers::pending=true;Registers::isr=[&]{for(auto p:e.playMemPtr){p->setForcedPoints(12000,0,65535,54000);p->setPointsForceFlag();}};Registers::service();slowUnderMask=pcmWithAnyMask=missingPlan=0;};
    CHECK(trigger(e,0,24,false,0,0,0,0,2)==0);CHECK(notReady);CHECK(starts.empty());CHECK(e.currentInstrument_idx==2);CHECK(slowUnderMask==0&&pcmWithAnyMask==0);
    CHECK(!Registers::mask&&Registers::nvic);
    std::puts("DEFENSIVE test only: arbitrary IRQ forced-point mutation refuses stale playback; production reachability NOT established, NOT used as an integration blocker");
}
static void nativeFxOnlyEntry(){
    reset();setup();resolveBlockers=true;playerEngine e;auto&i=mtProject.instrument[2];i.playMode=0;i.startPoint=16384;i.endPoint=49152;
    CHECK(trigger(e,2,60));armLfo(e);
    // Foreground edits Single Shot geometry, then an FX-only sequencer step
    // updates slot 0 Amp LFO. seqFx itself masks output IRQ BEFORE endFx calls
    // syncFxAmpLFO; there is no outer NoteOn preparation context.
    i.startPoint=16416;starts.clear();auto before=rng.state();
    expectMaskedPreparation=true;maskedPreparationCalls=0;
    Registers::context=3;
    e.seqFx(Sequencer::strFxConsts::FX_TYPE_VOLUME_LFO,8,0);
    Registers::context=16;expectMaskedPreparation=false;
    CHECK(maskedPreparationCalls>0);CHECK(starts.size()==2);CHECK(rng.state()==before);
    for(const auto&s:starts){CHECK(s.technical&&s.instrument==2);CHECK(s.start==AudioPlayMemory::snapNativeBody(i.startPoint,65536,AudioPlayMemory::samples));}
    CHECK(!Registers::mask&&Registers::nvic);
    std::printf("REMAINING native FX-only counterexample: seqFx -> endFxVolumeLFO -> syncFxAmpLFO performs %u real snap calls with output IRQ disabled; zero WR2 RNG, correct identity, temporal contract FAIL\n",maskedPreparationCalls);
}
static void fixedFenceBoundaries(){
    reset();setup();resolveBlockers=true;playerEngine probe;CHECK(trigger(probe,0,24,true));const auto points=boundary;
    for(unsigned at=0;at<points;++at){
        reset();setup();resolveBlockers=true;playerEngine e;
        Registers::isr=[&]{CHECK(trigger(e,1,24,true,0,0,0,0,2));};injectBoundary=at;
        CHECK(trigger(e,0,24,true,0,0,0,0,2));
        CHECK(e.currentInstrument_idx==1&&e.left.actualInstr==1&&e.right.actualInstr==1);
        CHECK(e.left.actualSlice==e.right.actualSlice&&!notReady);
        CHECK(!pcmWithAnyMask&&!slowUnderMask&&!missingPlan&&!Registers::mask&&Registers::nvic);
    }
    reset();setup();resolveBlockers=true;playerEngine e;CHECK(trigger(e,0,25));armLfo(e);starts.clear();auto expected=rng;
    CHECK(trigger(e,1,24,true,0,0,Sequencer::strFxConsts::FX_TYPE_VOLUME_LFO,8,2));expected.percent();CHECK(rng.state()==expected.state());
    CHECK(starts.size()==4);for(const auto&s:starts)CHECK(s.instrument==1);
    CHECK(!pcmWithAnyMask&&!slowUnderMask&&!missingPlan&&!notReady);
    std::printf("PASS corrected candidate: %u re-enable boundaries, no post-IRQ overwrite; new-instrument LFO never borrows previous identity\n",points);
}
static int previousMain(){starts.reserve(4096);basic();pending();lfo();interleavings();crossModeBlock();edges();stalePreparation();delayedS2AndRetrigger();fixedCrossMode();fixedStalePreparation();pending(true);delayedS2AndRetrigger(true);fixedFenceBoundaries();dirtyGeometryAfterPrepare();nativeFxOnlyEntry();std::printf("PASS %llu historical regression checks; pre-hoist FX-only version reproduced; NOT hardware validation\n",checks);return 0;}

static std::vector<Start> fxDifferential(bool fixed,bool edit=true,bool previous=false){
    reset();setup();baselineNative=!fixed;resolveBlockers=fixed;hoistFxPreparation=fixed;
    // Explicitly reproduce the PCM left by fixedCrossMode before the original
    // nativeFxOnlyEntry test: avoid dependence on previous test execution order.
    std::fill(std::begin(AudioPlayMemory::samples),std::end(AudioPlayMemory::samples),int16_t{1000});AudioPlayMemory::samples[16390]=0;
    playerEngine e;auto&i=mtProject.instrument[2];i.playMode=0;i.startPoint=16384;i.endPoint=49152;
    if(fixed)CHECK(trigger(e,2,60));else CHECK(e.noteOn(2,60,127));armLfo(e);
    if(edit)i.startPoint=16416;
    if(previous){hoistFxPreparation=false;expectMaskedPreparation=true;}
    starts.clear();resetWork();maskedPreparationCalls=0;auto before=rng.state();Registers::context=3;
    e.seqFx(Sequencer::strFxConsts::FX_TYPE_VOLUME_LFO,8,0);
    Registers::context=16;expectMaskedPreparation=false;CHECK(starts.size()==2);CHECK(rng.state()==before);CHECK(!notReady);
    CHECK(!Registers::mask&&Registers::nvic);CHECK(starts[0].technical&&starts[1].technical);
    CHECK(starts[0].instrument==2&&starts[1].instrument==2);CHECK(starts[0].start==starts[1].start&&starts[0].end==starts[1].end);
    std::printf("DIFFERENTIAL %s edit=%d snap=%u output-masked=%u BASEPRI-masked=%u PCM-visits=%u output-masked-visits=%u BASEPRI-masked-visits=%u restarts=2 order=L,R start=%u end=%u\n",previous?"WR2-before":fixed?"WR2-hoisted":"native",edit,snapTotal,snapOutputMasked,snapBasepriMasked,pcmVisits,pcmOutputMasked,pcmBasepriMasked,starts[0].start,starts[0].end);
    if(previous){CHECK(snapTotal==(edit?6u:2u)&&snapOutputMasked==snapTotal&&snapBasepriMasked==0);}
    else if(fixed){CHECK(snapTotal==(edit?2u:0u));CHECK(snapOutputMasked==0&&snapBasepriMasked==0&&pcmOutputMasked==0&&pcmBasepriMasked==0);}
    else{CHECK(snapTotal==2&&snapOutputMasked==2&&snapBasepriMasked==0);}
    return starts;
}
// Production blocker: old Loop is valid CE playback. The candidate currently
// rejects it in bindLive even though the NEW musical target is Beat Slice.
static void oldLoopDifferential(bool candidate){
    reset();setup();nativeDomains=candidate;playerEngine e;auto&i=mtProject.instrument[2];i.playMode=1;i.startPoint=1000;i.loopPoint1=2000;i.loopPoint2=5000;i.endPoint=60000;
    baselineNative=true;CHECK(e.noteOn(2,60,127));baselineNative=false;armLfo(e);
    starts.clear();resetWork();auto before=rng.state();unsigned accepted=0;
    if(candidate){resolveBlockers=true;hoistFxPreparation=true;accepted=trigger(e,0,24,false,0,0,0,0,2);CHECK(accepted==1&&!notReady&&starts.size()==4);CHECK(rng.state()!=before);}
    else{baselineNative=true;accepted=e.noteOn(0,24,127);CHECK(accepted&&starts.size()==4&&!notReady);CHECK(rng.state()==before);CHECK(starts[0].instrument==2&&starts[1].instrument==2&&starts[2].instrument==0&&starts[3].instrument==0);CHECK(starts[0].technical&&starts[1].technical);}
    CHECK(!Registers::mask&&Registers::nvic);
    std::printf("LOOP-DIFFERENTIAL %s accepted=%u starts=%zu RNG-advanced=%d notReady=%d snap=%u output-masked=%u BASEPRI-masked=%u\n",candidate?"WR2-candidate":"native",accepted,starts.size(),rng.state()!=before,notReady,snapTotal,snapOutputMasked,snapBasepriMasked);
}
static void fxPreemption(){
    reset();setup();resolveBlockers=hoistFxPreparation=true;playerEngine e;CHECK(trigger(e,0,25));armLfo(e);const auto L=static_cast<uint8_t>(Sequencer::strFxConsts::FX_TYPE_VOLUME_LFO);auto expected=rng;
    beforeCommit=[&]{Registers::isr=[&]{CHECK(trigger(e,1,24,true,L,8,L,9,2));};Registers::pending=true;Registers::service();starts.clear();};
    e.seqFx(L,8,0);expected.percent();CHECK(rng.state()==expected.state());CHECK(!notReady&&starts.size()==2);
    for(const auto&s:starts)CHECK(s.technical&&s.instrument==1&&s.slice==e.left.active.slice);
    CHECK(e.left.active.slice==e.right.active.slice&&!Registers::mask&&Registers::nvic);
    std::puts("PASS hoisted standalone seqFx preemption: IRQ replaces instrument before commit; restart uses live paired identity/points, no technical draw");
}
static void modeTransitions(bool candidate){
    for(unsigned mode=0;mode<8;++mode)for(bool lfo:{false,true})for(bool fx:{false,true}){
        reset();setup();std::fill(std::begin(AudioPlayMemory::samples),std::end(AudioPlayMemory::samples),int16_t{0});nativeDomains=candidate;resolveBlockers=candidate;hoistFxPreparation=candidate;baselineNative=!candidate;
        playerEngine e;auto&i=mtProject.instrument[2];i.playMode=static_cast<uint8_t>(mode);i.startPoint=1000;i.loopPoint1=2000;i.loopPoint2=5000;i.endPoint=60000;i.selectedSlice=1;
        if(candidate)CHECK(trigger(e,2,25));else CHECK(e.noteOn(2,25,127));
        if(lfo)armLfo(e);starts.clear();resetWork();auto expected=rng;auto before=rng.state();unsigned accepted;
        const auto L=static_cast<uint8_t>(Sequencer::strFxConsts::FX_TYPE_VOLUME_LFO);
        if(candidate){accepted=trigger(e,0,24,fx,0,0,lfo?L:0,8,2);expected.percent();CHECK(rng.state()==expected.state());}
        else{accepted=fx?e.noteOn(0,24,-1,0,0,static_cast<uint8_t>(lfo?L:0),8):e.noteOn(0,24,127);CHECK(rng.state()==before);}
        CHECK(accepted==1&&!notReady);CHECK(starts.size()==(lfo?4u:2u));CHECK(!Registers::mask&&Registers::nvic);
        CHECK(starts[starts.size()-2].slice==starts.back().slice);
        std::printf("MODE %s old=%u lfo=%u fx=%u accepted=%u starts=%zu draws=%u snap=%u output=%u basepri=%u\n",candidate?"candidate":"original",mode,lfo,fx,accepted,starts.size(),candidate?1u:0u,snapTotal,snapOutputMasked,snapBasepriMasked);
        for(const auto&t:starts)std::printf("EVENT instrument=%u channel=%u tech=%u mode=%u slice=%u start=%u end=%u loop1=%u loop2=%u span=%u\n",t.instrument,t.channel,t.technical,t.mode,t.slice,t.start,t.end,t.loop1,t.loop2,t.span);
    }
}
#ifdef A1_FIRMWARE_BRIDGE
static void firmwareSetup(){
    reset();setup();baselineNative=true;std::fill(std::begin(AudioPlayMemory::samples),std::end(AudioPlayMemory::samples),int16_t{0});
    for(auto&m:mtProject.sampleMemory)m.address[0]=m.address[1]=AudioPlayMemory::samples;
    InstrumentState s;s.reset(4);create(s,0);add(s,0,1);add(s,0,2);setWeight(s,0,0,0);
    CHECK(wr2native::publication().instrument[0].publish(s,1)==PublishResult::Published);
    CHECK(wr2native::publication().instrument[1].publish(s,1)==PublishResult::Published);
}
static void firmwareExtra(){
    const auto L=static_cast<uint8_t>(Sequencer::strFxConsts::FX_TYPE_VOLUME_LFO),S=static_cast<uint8_t>(Sequencer::strFxConsts::FX_TYPE_SAMPLE_SLICE);
    // Actual production bridge, same FX-only native scenario (not model scopes).
    for(bool beat:{false,true}){
        firmwareSetup();std::fill(std::begin(AudioPlayMemory::samples),std::end(AudioPlayMemory::samples),int16_t{1000});AudioPlayMemory::samples[16390]=0;
        playerEngine e;unsigned instrument=beat?0:2;auto&i=mtProject.instrument[instrument];i.playMode=static_cast<uint8_t>(beat?5:0);i.startPoint=16384;i.endPoint=49152;
        CHECK(e.noteOn(static_cast<uint8_t>(instrument),beat?25:60,127));armLfo(e);if(beat)i.slices[1]+=16;else i.startPoint=16416;
        auto before=wr2native::randomState();starts.clear();resetWork();Registers::context=3;e.seqFx(L,8,0);Registers::context=16;
        CHECK(snapTotal==2&&snapOutputMasked==2&&snapBasepriMasked==0);CHECK(starts.size()==2&&wr2native::randomState()==before);
        CHECK(starts[0].technical&&starts[1].technical);CHECK(starts[0].instrument==instrument&&starts[1].instrument==instrument);
        std::printf("FIRMWARE FX-only beat=%d snap=%u output-masked=%u BASEPRI=%u visits=%u restarts=2 RNG=0\n",beat,snapTotal,snapOutputMasked,snapBasepriMasked,pcmVisits);
    }
    // In-place PCM edits may leave address/length/cut keys unchanged. Native
    // restart recalculates, so the bridge must not reuse an old crossing.
    firmwareSetup();std::fill(std::begin(AudioPlayMemory::samples),std::end(AudioPlayMemory::samples),int16_t{1000});AudioPlayMemory::samples[1050]=0;
    playerEngine pcm;CHECK(pcm.noteOn(0,25,127));CHECK(pcm.left.pointsInSamples.start==1050);
    AudioPlayMemory::samples[1050]=1000;AudioPlayMemory::samples[1060]=0;resetWork();pcm.syncFxAmpLFO();CHECK(pcm.left.pointsInSamples.start==1060&&pcm.right.pointsInSamples.start==1060);CHECK(snapTotal==2&&snapBasepriMasked==0);
    // Frozen pending S2 with a subsequent live S2 change, both native consumers.
    for(unsigned wt:{0u,2u}){
        firmwareSetup();playerEngine e;for(auto*v:e.playMemPtr){v->envelope.state=envelopePhaseSustain;v->envelope.wtMode=static_cast<uint8_t>(wt);}
        auto before=wr2native::randomState();CHECK(e.noteOn(0,24,-1,S,2,0,0));e.seqFx(S,0,0);
        for(auto*v:e.playMemPtr){v->envelope.count=0;v->envelopeUpdate(&v->storageBlock);CHECK(v->wr2State.active.slice==2);}
        CHECK(wr2native::randomState()==before);
    }
    // A technical restart adopts the accepted pending choice, including the
    // interval in which L has resumed and R is still pending.
    firmwareSetup();playerEngine queued;CHECK(queued.noteOn(0,25,127));for(auto*v:queued.playMemPtr)v->envelope.state=envelopePhaseSustain;
    CHECK(queued.noteOn(0,24,127));auto pending=queued.left.wr2State.pending.slice;auto before=wr2native::randomState();queued.syncFxAmpLFO();CHECK(queued.left.wr2State.active.slice==pending&&queued.right.wr2State.active.slice==pending&&wr2native::randomState()==before);
    for(auto*v:queued.playMemPtr)v->envelope.state=envelopePhaseSustain;
    CHECK(queued.noteOn(0,24,127));pending=queued.left.wr2State.pending.slice;queued.left.envelope.count=0;queued.left.envelopeUpdate(&queued.left.storageBlock);queued.syncFxAmpLFO();CHECK(queued.left.wr2State.active.slice==pending&&queued.right.wr2State.active.slice==pending);
    // Original TOCTOU pattern: IRQ replaces active slice after technical capture.
    firmwareSetup();playerEngine stale;CHECK(stale.noteOn(0,25,127));armLfo(stale);auto expected=Random(wr2native::randomState());expected.percent();
    restoreCountdown=5;restoreHook=[&]{Registers::pending=true;Registers::isr=[&]{CHECK(stale.noteOn(0,26,-1,L,8,L,9));};Registers::service();starts.clear();};
    CHECK(stale.noteOn(0,24,-1,0,0,L,8));CHECK(!restoreHook);CHECK(starts.size()==4);CHECK(starts[0].technical&&starts[1].technical&&starts[0].slice==2&&starts[1].slice==2);CHECK(wr2native::randomState()==expected.state());
    // Existing re-enable boundaries, now executing the production bridge.
    firmwareSetup();playerEngine probe;CHECK(probe.noteOn(0,24,127));const auto boundaries=boundary;
    for(unsigned at=0;at<boundaries;++at){
        firmwareSetup();playerEngine e;auto expect=Random(wr2native::randomState());expect.percent();expect.percent();
        Registers::isr=[&]{CHECK(e.noteOn(1,24,127));};injectBoundary=at;
        CHECK(e.noteOn(0,24,127));CHECK(e.currentInstrument_idx==1);CHECK(e.left.wr2State.active.instrument==1&&e.right.wr2State.active.instrument==1);CHECK(e.left.wr2State.active.slice==e.right.wr2State.active.slice);CHECK(wr2native::randomState()==expect.state());CHECK(!Registers::mask&&Registers::nvic);
    }
    // Cancel pair versus IRQ NoteOn: the new IRQ must not be cut on R only.
    for(int option:{-1,-2,-3}){
        firmwareSetup();playerEngine e;for(auto*v:e.playMemPtr)v->envelope.state=envelopePhaseSustain;
        CHECK(e.noteOn(0,24,127));auto cancelExpected=Random(wr2native::randomState());cancelExpected.percent();
        Registers::isr=[&]{CHECK(e.noteOn(1,24,127));};injectBoundary=boundary;
        e.noteOff(static_cast<int8_t>(option));CHECK(e.left.playing&&e.right.playing);CHECK(e.left.wr2State.active.instrument==1&&e.right.wr2State.active.instrument==1);CHECK(e.left.wr2State.active.slice==e.right.wr2State.active.slice);CHECK(!e.left.wr2State.pendingValid&&!e.right.wr2State.pendingValid);CHECK(wr2native::randomState()==cancelExpected.state());
    }
    // Native invalid note admission consumes no WR2 RNG.
    firmwareSetup();playerEngine rejected;auto rejectedBefore=wr2native::randomState();CHECK(rejected.noteOn(255,24,127)==0);CHECK(rejected.noteOn(255,24,-1,S,0,0,0)==0);mtProject.instrument[0].isActive=0;CHECK(rejected.noteOn(0,24,127)==0);mtProject.instrument[0].isActive=1;rejected.isBlockedNoteOn[0]=true;CHECK(rejected.noteOn(0,24,127)==0);CHECK(wr2native::randomState()==rejectedBefore);
    // Beat -> every native mode: preserve the old chosen technical restart,
    // and never draw for the incoming non-WR2 instrument.
    for(unsigned mode=0;mode<8;++mode){
        firmwareSetup();playerEngine e;CHECK(e.noteOn(0,24,127));auto selected=e.left.wr2State.active.slice;armLfo(e);
        auto&i=mtProject.instrument[2];i.playMode=static_cast<uint8_t>(mode);i.startPoint=1000;i.loopPoint1=2000;i.loopPoint2=5000;i.endPoint=60000;
        auto reverseBefore=wr2native::randomState();starts.clear();resetWork();CHECK(e.noteOn(2,25,127));CHECK(starts.size()==4&&wr2native::randomState()==reverseBefore);CHECK(starts[0].technical&&starts[0].slice==selected&&starts[1].slice==selected);CHECK(starts[2].instrument==2&&starts[3].instrument==2);CHECK(snapBasepriMasked==0);
    }
    // Cuts move without touching publication owners/weights.
    firmwareSetup();playerEngine cuts;Snapshot old,now;CHECK(wr2native::publication().instrument[0].read(old));mtProject.instrument[0].slices[1]+=32;CHECK(cuts.noteOn(0,24,127));CHECK(wr2native::publication().instrument[0].read(now));CHECK(std::memcmp(&old.state,&now.state,sizeof old.state)==0);
    for(unsigned mask:{0u,0x20u,0x40u})for(bool enabled:{false,true}){
        firmwareSetup();playerEngine e;Registers::mask=mask;Registers::nvic=enabled;CHECK(e.noteOn(0,24,127));CHECK(Registers::mask==mask&&Registers::nvic==enabled);
    }
    std::printf("PASS actual bridge extra: %u re-enable boundaries, PCM edit, S2 pending, half-resume, TOCTOU, cut retention, mask restoration\n",boundaries);
}
static void firmwareTests(){
    for(unsigned mode=0;mode<8;++mode)for(bool lfo:{false,true})for(bool fx:{false,true}){
        firmwareSetup();playerEngine e;auto&i=mtProject.instrument[2];i.playMode=static_cast<uint8_t>(mode);i.startPoint=1000;i.loopPoint1=2000;i.loopPoint2=5000;i.endPoint=60000;
        CHECK(e.noteOn(2,25,127));if(lfo)armLfo(e);starts.clear();resetWork();auto expected=Random(wr2native::randomState());expected.percent();auto L=static_cast<uint8_t>(Sequencer::strFxConsts::FX_TYPE_VOLUME_LFO);
        CHECK(fx?e.noteOn(0,24,-1,0,0,static_cast<uint8_t>(lfo?L:0),8):e.noteOn(0,24,127));
        CHECK(wr2native::randomState()==expected.state());CHECK(starts.size()==(lfo?4u:2u));CHECK(snapBasepriMasked==0);
        CHECK(e.left.wr2State.activeValid&&e.right.wr2State.activeValid);
        CHECK(e.left.wr2State.active.slice==e.right.wr2State.active.slice);CHECK(e.left.wr2State.active.randomized);
        CHECK(e.left.pointsInSamples.start==e.right.pointsInSamples.start);
        if(lfo)for(unsigned ch=0;ch<2;++ch)CHECK(starts[ch].instrument==(fx?0u:2u));
        std::printf("NATIVE-MODE old=%u lfo=%u fx=%u accepted=1 starts=%zu draws=1 snap=%u output=%u basepri=%u\n",mode,lfo,fx,starts.size(),snapTotal,snapOutputMasked,snapBasepriMasked);
        for(const auto&t:starts)std::printf("NATIVE-EVENT instrument=%u channel=%u tech=%u mode=%u slice=%u start=%u end=%u loop1=%u loop2=%u span=%u\n",t.instrument,t.channel,t.technical,t.mode,t.slice,t.start,t.end,t.loop1,t.loop2,t.span);
        CHECK(!Registers::mask&&Registers::nvic);
    }
    for(unsigned wt:{0u,2u}){
        firmwareSetup();playerEngine e;for(auto*v:e.playMemPtr){v->envelope.state=envelopePhaseSustain;v->envelope.wtMode=static_cast<uint8_t>(wt);}
        auto expected=Random(wr2native::randomState());CHECK(e.noteOn(0,24,127));expected.percent();CHECK(wr2native::randomState()==expected.state());
        CHECK(e.left.stackedPlay.enable&&e.right.stackedPlay.enable);CHECK(e.left.wr2State.pendingValid&&e.right.wr2State.pendingValid);
        auto chosen=e.left.wr2State.pending.slice;for(auto*v:e.playMemPtr){v->envelope.count=0;v->envelopeUpdate(&v->storageBlock);CHECK(v->wr2State.activeValid&&v->wr2State.active.slice==chosen);}
        CHECK(wr2native::randomState()==expected.state());
        e.syncFxAmpLFO();CHECK(wr2native::randomState()==expected.state());CHECK(e.left.wr2State.active.slice==chosen&&e.right.wr2State.active.slice==chosen);
    }
    firmwareSetup();playerEngine e;auto S=static_cast<uint8_t>(Sequencer::strFxConsts::FX_TYPE_SAMPLE_SLICE);auto before=wr2native::randomState();CHECK(e.noteOn(0,24,-1,S,2,S,1));CHECK(wr2native::randomState()==before);CHECK(e.left.wr2State.active.slice==2&&e.right.wr2State.active.slice==2);
    for(auto*v:e.playMemPtr)v->envelope.state=envelopePhaseSustain;
    CHECK(e.noteOn(0,24,127));CHECK(e.left.wr2State.pendingValid);e.noteOff(Sequencer::STEP_NOTE_OFF_CUT);CHECK(!e.left.wr2State.pendingValid&&!e.right.wr2State.pendingValid);
    firmwareExtra();
    std::printf("PASS actual native bridge: %llu checks; 8 modes, both NoteOn entries, stereo, both Forced consumers, technical restart, S2, cancel; HOST not hardware\n",checks);
}
#endif
#ifdef A1_REAL_AMP
static void realAmpTransitions(){
    unsigned cases=0;
    for(unsigned mode=0;mode<8;++mode)for(bool lfo:{false,true})for(bool fx:{false,true})for(unsigned phase:{1u,5u,7u}){
#ifdef A1_FIRMWARE_BRIDGE
        firmwareSetup();const bool candidate=true;
#else
        reset();setup();baselineNative=true;std::fill(std::begin(AudioPlayMemory::samples),std::end(AudioPlayMemory::samples),int16_t{0});const bool candidate=false;
#endif
        playerEngine e;auto&i=mtProject.instrument[2];i.playMode=static_cast<uint8_t>(mode);i.startPoint=1000;i.loopPoint1=2000;i.loopPoint2=5000;i.endPoint=60000;CHECK(e.noteOn(2,25,127));if(lfo)armLfo(e);
        for(auto*v:e.playMemPtr)v->envelope.state=static_cast<uint8_t>(phase);
        starts.clear();resetWork();
#ifdef A1_FIRMWARE_BRIDGE
        auto expected=Random(wr2native::randomState());expected.percent();
#endif
        const auto L=static_cast<uint8_t>(Sequencer::strFxConsts::FX_TYPE_VOLUME_LFO);
        CHECK(fx?e.noteOn(0,24,-1,0,0,lfo?L:0,8):e.noteOn(0,24,127));
        const bool delayed=lfo?!fx:phase==5;
        const auto immediate=starts.size();CHECK(immediate==(lfo?2u:0u)+(delayed?0u:2u));
        CHECK(e.left.stackedPlay.enable==delayed&&e.right.stackedPlay.enable==delayed);
#ifdef A1_FIRMWARE_BRIDGE
        CHECK(wr2native::randomState()==expected.state());CHECK(snapBasepriMasked==0);
        const unsigned chosen=delayed?e.left.wr2State.pending.slice:e.left.wr2State.active.slice;CHECK(chosen==1||chosen==2);
#else
        const unsigned chosen=0;
#endif
        if(delayed)for(auto*v:e.playMemPtr){v->envelope.count=0;v->envelopeUpdate(&v->storageBlock);}
        CHECK(starts.size()==(lfo?4u:2u));
#ifdef A1_FIRMWARE_BRIDGE
        CHECK(e.left.wr2State.active.slice==chosen&&e.right.wr2State.active.slice==chosen&&wr2native::randomState()==expected.state());CHECK(snapBasepriMasked==0);
#endif
        std::printf("REAL-MODE %s old=%u lfo=%u fx=%u phase=%u accepted=1 immediate=%zu starts=%zu pending=%u selected=%u snap=%u basepri=%u\n",candidate?"candidate":"original",mode,lfo,fx,phase,immediate,starts.size(),delayed,chosen,snapTotal,snapBasepriMasked);
        for(const auto&t:starts)std::printf("REAL-EVENT instrument=%u channel=%u tech=%u mode=%u slice=%u start=%u end=%u loop1=%u loop2=%u span=%u\n",t.instrument,t.channel,t.technical,t.mode,t.slice,t.start,t.end,t.loop1,t.loop2,t.span);
        ++cases;
    }
    for(unsigned slot:{0u,1u})for(bool newLfo:{false,true})for(unsigned phase:{0u,1u,5u,7u}){
#ifdef A1_FIRMWARE_BRIDGE
        firmwareSetup();const bool candidate=true;
#else
        reset();setup();baselineNative=true;std::fill(std::begin(AudioPlayMemory::samples),std::end(AudioPlayMemory::samples),int16_t{0});const bool candidate=false;
#endif
        playerEngine e;CHECK(e.noteOn(2,25,127));const auto L=static_cast<uint8_t>(Sequencer::strFxConsts::FX_TYPE_VOLUME_LFO);e.lastSeqFx[slot]=L;e.trackControlParameter[slot][static_cast<unsigned>(parameterList::lfoAmp)]=true;
        for(auto*v:e.playMemPtr)v->envelope.state=static_cast<uint8_t>(phase);
        starts.clear();resetWork();CHECK(e.noteOn(0,24,-1,0,0,newLfo?L:0,9));
        const bool sync=slot==0&&newLfo,delayed=!sync&&phase!=0;const auto immediate=starts.size();CHECK(immediate==(sync?4u:delayed?0u:2u));
        const auto snapBefore=snapTotal;CHECK(snapBefore==(sync?(candidate?2u:4u):!delayed?2u:0u));
        if(delayed)for(auto*v:e.playMemPtr){v->envelope.count=0;v->envelopeUpdate(&v->storageBlock);}
        CHECK(starts.size()==(sync?4u:2u));
#ifdef A1_FIRMWARE_BRIDGE
        CHECK(snapBasepriMasked==0);CHECK(e.left.wr2State.active.slice==e.right.wr2State.active.slice);
#endif
        std::printf("REAL-SINGLE %s slot=%u newLfo=%u phase=%u accepted=1 immediate=%zu starts=%zu pending=%u before=%u snap=%u basepri=%u\n",candidate?"candidate":"original",slot,newLfo,phase,immediate,starts.size(),delayed,snapBefore,snapTotal,snapBasepriMasked);
    }
    std::printf("PASS real Amp LFO helper: %u transitions, acceptance/delay/resume preserved; %llu checks\n",cases,checks);
}
#endif
#ifdef A1_IO
#include "io-tests.inc"
#endif
#ifdef A1_IMPORTS
#include "imports.inc"
#endif
#ifdef A1_UI_LIFECYCLE
static void uiLifecycle(){
    firmwareSetup();playerEngine e;wr2::ui::Editor editor(wr2native::publication());
    editor.bind(0,4,true);for(auto*v:e.playMemPtr)v->envelope.state=envelopePhaseSustain;
    CHECK(e.noteOn(0,24,127));auto random=wr2native::randomState();
    CHECK(e.left.wr2State.pendingValid&&e.left.wr2State.pending.slice!=0);
    CHECK(editor.invalidate(0));
    // New instrument/project with exactly the same mode/count. Old WR2 token
    // must become native fallback, not reuse a previous context's weighted slice.
    starts.clear();for(auto*v:e.playMemPtr){v->envelope.count=0;v->envelopeUpdate(&v->storageBlock);}
    CHECK(starts.size()==2&&starts[0].slice==0&&starts[1].slice==0);
    CHECK(wr2native::randomState()==random);
    std::printf("PASS UI lifecycle native pending fallback: %llu checks; same-count replacement no stale WR2 selection\n",checks);
}
#endif
int main(){
#ifdef A1_IMPORTS
    importPlaybackTests();return 0;
#endif
#ifdef A1_UI_LIFECYCLE
    uiLifecycle();return 0;
#endif
#ifdef A1_IO
    ioTests();return 0;
#endif
    starts.reserve(4096);
#ifdef A1_REAL_AMP
    realAmpTransitions();return 0;
#endif
#ifdef A1_FIRMWARE_BRIDGE
    if(false){previousMain();fxDifferential(false);fxPreemption();oldLoopDifferential(false);modeTransitions(false);}firmwareTests();return 0;
#endif
#ifdef A1_NATIVE_BASELINE
    if(false){previousMain();fxPreemption();}fxDifferential(false);fxDifferential(false,false);oldLoopDifferential(false);modeTransitions(false);
#else
    previousMain();fxDifferential(true,true,true);fxDifferential(true,false,true);fxDifferential(true);fxDifferential(true,false);fxPreemption();oldLoopDifferential(true);modeTransitions(true);
#endif
    std::printf("PASS differential fixture; %llu total checks; no hardware validation\n",checks);return 0;
}
