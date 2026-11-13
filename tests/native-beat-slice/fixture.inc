// Included twice in separate namespaces. This is a HAL/DSP fixture, not an emulator.
constexpr uint8_t kSampleInstrumentsCount=48,MAX_SLICE_NUMBER=48;
constexpr int FIRST_BEAT_SLICE_NOTE=24;constexpr uint16_t MAX_16BIT=65535;
enum {playModeSingleShot,playModeLoopForward,playModeLoopBackward,playModePingpong,playModeSlice,playModeBeatSlice,playModeGranular,playModeWavetable};
template<class T,unsigned N>struct Array{T data[N]{};T& operator[](unsigned i){if(i>=N)throw Bounds(i,N);return data[i];}};
struct Instrument{volatile uint8_t sliceNumber=0;uint8_t slicePlayThru=0,playMode=playModeBeatSlice,isActive=1;Array<uint16_t,48>slices;};
struct {Array<Instrument,48>instrument;}mtProject;
static unsigned irqOff=0,irqOn=0;
inline void fixtureOff(){++irqOff;}inline void fixtureOn(){++irqOn;}
struct Stream {unsigned calls=0,notifications=0,slice=0,slot=0,ch=0;void getSlicePreCache(uint8_t i,uint8_t s,int c,uint32_t,int16_t*&,uint32_t&){++calls;slot=i;slice=s;ch=c;}template<class...T>void notifyNoteOn(T&&...){++notifications;}}voiceStreamManager;
class AudioPlayMemory{public:
 uint8_t currentInstrIdx=7,lastInstrIdxInPlay=0,forcedSlice=0,sliceForcedFlag=0,reverseDirectionFlag=0,currentIsStreamed=0,currentPlayMode=0,playing=0;
 int lastNote=0,channel=0,voiceIndex=0,iPitchCounter=0;float pitchControl=1;
 struct Points{uint32_t start=0,end=0,loop1=0,loop2=0,endPoint=0;}pointsInSamples,constrainsInSamples;
 int16_t samples[65536]{};int16_t*currentStartAddress=samples;int16_t*next=nullptr;AudioPlayMemory*samplePointsSource=nullptr;
 uint32_t currentSampleLength=65536,length=0,currentSliceCacheFrames=0;int16_t*currentSliceCacheAddress=nullptr;
 struct{bool isSlideOnPlay=false;uint8_t upOrDown=0,dstValue=0;float time=0;}stackedSlide;
 unsigned tuneCalls=0,otherMode=0,slideCalls=0;
 void reset(){currentInstrIdx=7;lastInstrIdxInPlay=0;forcedSlice=0;sliceForcedFlag=0;reverseDirectionFlag=0;currentIsStreamed=0;playing=0;lastNote=0;channel=0;voiceIndex=0;iPitchCounter=0;pointsInSamples={};constrainsInSamples={};next=nullptr;length=0;samplePointsSource=nullptr;currentStartAddress=samples;currentSampleLength=65536;currentSliceCacheFrames=0;currentSliceCacheAddress=nullptr;tuneCalls=0;otherMode=0;slideCalls=0;stackedSlide={};voiceStreamManager={};irqOff=irqOn=0;}
 void refreshStartParameters(){}void constrainCurrentTune(int8_t){++tuneCalls;}void setStartPitch(int8_t){}void applyFinetuneOnPitch(int8_t){}void calculateGlidePitch(int8_t){}
 uint32_t calculatePointPositionAndSnapToZero(uint16_t p,uint32_t,int16_t*){return p;}
 void setSlidePitch(uint8_t,uint8_t,float){++slideCalls;}
 void playSingleShot(uint8_t,int8_t){otherMode=1;}void playLoopForward(uint8_t,int8_t){otherMode=2;}void playLoopBackward(uint8_t,int8_t){otherMode=3;}void playLoopPingpong(uint8_t,int8_t){otherMode=4;}void playSlice(uint8_t,int8_t){otherMode=5;}void playGranular(uint8_t,int8_t){otherMode=7;}void playWavetable(uint8_t,int8_t){otherMode=8;}
 void playBeatSlice(uint8_t,int8_t);void setForcedSlice(uint8_t);uint8_t play(uint8_t,int8_t);
};
enum class controlType{sequencerMode=0};enum class parameterList{slice=0};constexpr uint8_t MOST_SIGNIFICANT_FX=0,LEAST_SIGNIFICANT_FX=1;
class playerEngine{public:
 uint8_t currentInstrument_idx=0,lastSeqVal[2]{},lastSeqFx[2]{};Array<bool,48>isBlockedNoteOn;
 bool trackControlParameter[2][1]{};struct{uint8_t slice=17;}currentSeqModValues;
 unsigned sets=0,clears=0;void setFxSlice(){++sets;}void clearFxSlice(){++clears;}void endFx(uint8_t,uint8_t){}
 void fxSampleSlice(uint8_t,uint8_t);void endFxSlice(uint8_t);uint8_t noteBoundary0(uint8_t);uint8_t noteBoundary1(uint8_t);
};
static AudioPlayMemory voice,source;
static void prepare(unsigned count){for(unsigned i=0;i<48;++i){auto& instr=mtProject.instrument[i];instr.sliceNumber=static_cast<uint8_t>(count);instr.slicePlayThru=0;instr.playMode=playModeBeatSlice;instr.isActive=1;for(unsigned s=0;s<48;++s)instr.slices[s]=static_cast<uint16_t>(s*1000);}voice.reset();source.pointsInSamples.start=321;source.pointsInSamples.end=654;}
#include VARIANT_INC