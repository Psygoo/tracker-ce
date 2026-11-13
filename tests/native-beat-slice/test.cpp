#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
using std::uint8_t;using std::uint16_t;using std::uint32_t;using std::int8_t;using std::int16_t;
struct Bounds:std::exception{unsigned index,capacity;Bounds(unsigned i,unsigned c):index(i),capacity(c){}};
static unsigned long long checks=0,playbackCases=0,fxCases=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::abort();}}while(0)
#define AUDIO_CORE_FLASH
#define AudioNoInterrupts() fixtureOff()
#define AudioInterrupts() fixtureOn()
#pragma warning(disable:4555)
#define VARIANT_INC "baseline.inc"
namespace original{
#include "fixture.inc"
}
#undef VARIANT_INC
#define VARIANT_INC "current.inc"
namespace corrected{
#include "fixture.inc"
}
#undef VARIANT_INC
static void compare(){auto&a=original::voice;auto&b=corrected::voice;
 CHECK(a.pointsInSamples.start==b.pointsInSamples.start);CHECK(a.pointsInSamples.end==b.pointsInSamples.end);
 CHECK(a.constrainsInSamples.endPoint==b.constrainsInSamples.endPoint);CHECK(a.iPitchCounter==b.iPitchCounter);
 CHECK(a.next-a.samples==b.next-b.samples);CHECK(a.length==b.length);CHECK(a.playing==b.playing);CHECK(a.lastNote==b.lastNote);
 CHECK(a.currentInstrIdx==b.currentInstrIdx);CHECK(a.lastInstrIdxInPlay==b.lastInstrIdxInPlay);CHECK(a.tuneCalls==b.tuneCalls);
 CHECK(original::voiceStreamManager.calls==corrected::voiceStreamManager.calls);CHECK(original::voiceStreamManager.slice==corrected::voiceStreamManager.slice);CHECK(original::voiceStreamManager.notifications==corrected::voiceStreamManager.notifications);
 CHECK(original::irqOff==corrected::irqOff&&original::irqOn==corrected::irqOn);
}
static void validPlayback(){for(unsigned count=0;count<=48;++count){original::prepare(count);corrected::prepare(count);
 for(unsigned options=0;options<16;++options){for(unsigned slot:{0u,47u}){for(int note=-128;note<=127;++note){
 auto&a=original::voice;auto&b=corrected::voice;a.reset();b.reset();
 a.reverseDirectionFlag=b.reverseDirectionFlag=static_cast<uint8_t>(options&1);a.currentIsStreamed=b.currentIsStreamed=static_cast<uint8_t>((options>>1)&1);
 original::mtProject.instrument[slot].slicePlayThru=corrected::mtProject.instrument[slot].slicePlayThru=static_cast<uint8_t>((options>>2)&1);
 if(options&8){a.samplePointsSource=&original::source;b.samplePointsSource=&corrected::source;}
 a.playBeatSlice(static_cast<uint8_t>(slot),static_cast<int8_t>(note));b.playBeatSlice(static_cast<uint8_t>(slot),static_cast<int8_t>(note));compare();++playbackCases;
 }}}
 for(unsigned forced=0;forced<(count?count:1);++forced)for(unsigned thru=0;thru<2;++thru){auto&a=original::voice;auto&b=corrected::voice;a.reset();b.reset();a.currentInstrIdx=b.currentInstrIdx=0;a.setForcedSlice(static_cast<uint8_t>(forced));b.setForcedSlice(static_cast<uint8_t>(forced));a.sliceForcedFlag=b.sliceForcedFlag=1;a.currentIsStreamed=b.currentIsStreamed=1;original::mtProject.instrument[0].slicePlayThru=corrected::mtProject.instrument[0].slicePlayThru=static_cast<uint8_t>(thru);a.playBeatSlice(0,24);b.playBeatSlice(0,24);compare();++playbackCases;}
 }
 std::printf("PASS baseline differential playback: %llu cases (empty, notes, slots, S2, PlayThru, reverse, streaming, stereo source)\n",playbackCases);
}
static void compareFx(const original::playerEngine&a,const corrected::playerEngine&b){CHECK(a.currentSeqModValues.slice==b.currentSeqModValues.slice);CHECK(a.trackControlParameter[0][0]==b.trackControlParameter[0][0]);CHECK(a.trackControlParameter[1][0]==b.trackControlParameter[1][0]);CHECK(a.sets==b.sets);CHECK(a.clears==b.clears);}
static void validFx(){for(unsigned count=1;count<=48;++count){original::prepare(count);corrected::prepare(count);for(unsigned value=0;value<256;++value)for(unsigned n=0;n<2;++n)for(unsigned flags=0;flags<4;++flags){
 original::playerEngine a;corrected::playerEngine b;a.trackControlParameter[0][0]=b.trackControlParameter[0][0]=(flags&1)!=0;a.trackControlParameter[1][0]=b.trackControlParameter[1][0]=(flags&2)!=0;
 a.fxSampleSlice(static_cast<uint8_t>(value),static_cast<uint8_t>(n));b.fxSampleSlice(static_cast<uint8_t>(value),static_cast<uint8_t>(n));compareFx(a,b);
 a.lastSeqVal[0]=b.lastSeqVal[0]=static_cast<uint8_t>(value);a.lastSeqVal[1]=b.lastSeqVal[1]=static_cast<uint8_t>(value);a.endFxSlice(static_cast<uint8_t>(n));b.endFxSlice(static_cast<uint8_t>(n));compareFx(a,b);++fxCases;
 }}std::printf("PASS baseline differential S2 precedence/end: %llu cases\n",fxCases);}
static void regression(){
 corrected::prepare(48);auto&v=corrected::voice;v.currentInstrIdx=0;v.setForcedSlice(47);v.sliceForcedFlag=1;v.currentIsStreamed=1;corrected::mtProject.instrument[0].sliceNumber=2;v.playBeatSlice(0,24);CHECK(v.pointsInSamples.start==1000);CHECK(v.pointsInSamples.end==65535);CHECK(corrected::voiceStreamManager.slice==1);
 for(unsigned count=0;count<256;++count){corrected::prepare(count);for(unsigned slot=0;slot<256;++slot)for(int note:{-128,23,24,71,127}){v.reset();v.sliceForcedFlag=1;v.forcedSlice=255;v.currentIsStreamed=1;v.playBeatSlice(static_cast<uint8_t>(slot),static_cast<int8_t>(note));if(slot>=48||count>48){CHECK(v.playing==0);CHECK(corrected::irqOff==0);}else{CHECK(v.playing==1);CHECK(corrected::irqOff==1&&corrected::irqOn==1);if(count)CHECK(corrected::voiceStreamManager.slice<count);else CHECK(v.pointsInSamples.start==0&&v.pointsInSamples.end==65535);}}
 for(unsigned slot=0;slot<256;++slot){v.currentInstrIdx=static_cast<uint8_t>(slot);v.forcedSlice=255;v.setForcedSlice(255);CHECK(v.forcedSlice==((slot>=48||!count||count>48)?0:count-1));}
 }
 corrected::prepare(0);for(unsigned n=0;n<2;++n){corrected::playerEngine p;p.fxSampleSlice(255,static_cast<uint8_t>(n));CHECK(p.currentSeqModValues.slice==0&&p.sets==1);p.trackControlParameter[!n][0]=true;p.endFxSlice(static_cast<uint8_t>(n));CHECK(p.currentSeqModValues.slice==0&&p.sets==2);}
 for(unsigned slot=0;slot<256;++slot)for(unsigned n:{0u,1u,2u,255u}){corrected::prepare(3);corrected::playerEngine p;p.currentInstrument_idx=static_cast<uint8_t>(slot);p.fxSampleSlice(255,static_cast<uint8_t>(n));p.endFxSlice(static_cast<uint8_t>(n));if(slot>=48||n>1)CHECK(p.sets==0&&p.clears==0&&p.currentSeqModValues.slice==17);}
 for(unsigned slot=0;slot<256;++slot){corrected::prepare(3);v.reset();auto status=v.play(static_cast<uint8_t>(slot),24);CHECK(status==(slot<48));corrected::playerEngine p;CHECK(p.noteBoundary0(static_cast<uint8_t>(slot))==(slot<48));CHECK(p.noteBoundary1(static_cast<uint8_t>(slot))==(slot<48));}
 corrected::prepare(255);v.reset();CHECK(v.play(0,24)==0);CHECK(v.playing==0);
 for(unsigned count=0;count<=48;++count)for(unsigned mode=0;mode<8;++mode){original::prepare(count);corrected::prepare(count);original::mtProject.instrument[0].playMode=corrected::mtProject.instrument[0].playMode=static_cast<uint8_t>(mode);CHECK(original::voice.play(0,24)==corrected::voice.play(0,24));CHECK(original::voice.otherMode==corrected::voice.otherMode);}
 // Differential setter clamp and note-entry active/blocked behaviour.
 for(unsigned count=0;count<=48;++count){original::prepare(count);corrected::prepare(count);
  for(unsigned value=0;value<256;++value){original::voice.currentInstrIdx=corrected::voice.currentInstrIdx=47;original::voice.setForcedSlice(static_cast<uint8_t>(value));corrected::voice.setForcedSlice(static_cast<uint8_t>(value));CHECK(original::voice.forcedSlice==corrected::voice.forcedSlice);}}
 for(unsigned slot=0;slot<48;++slot)for(unsigned active=0;active<2;++active)for(unsigned blocked=0;blocked<2;++blocked){original::prepare(3);corrected::prepare(3);original::mtProject.instrument[slot].isActive=corrected::mtProject.instrument[slot].isActive=static_cast<uint8_t>(active);original::playerEngine a;corrected::playerEngine b;a.isBlockedNoteOn[slot]=b.isBlockedNoteOn[slot]=blocked!=0;CHECK(a.noteBoundary0(static_cast<uint8_t>(slot))==b.noteBoundary0(static_cast<uint8_t>(slot)));CHECK(a.noteBoundary1(static_cast<uint8_t>(slot))==b.noteBoundary1(static_cast<uint8_t>(slot)));}
 std::puts("PASS corrected edge cases: invalid slot/count, stale S2, zero-count FX, dispatch and note-entry boundaries");
}
static void baselineRepro(){unsigned reproduced=0;auto expect=[&](auto fn){try{fn();}catch(const Bounds&e){std::printf("BASELINE REPRO index=%u capacity=%u\n",e.index,e.capacity);++reproduced;}};
 expect([]{original::prepare(3);original::voice.playBeatSlice(255,24);});expect([]{original::prepare(255);original::voice.playBeatSlice(0,100);});expect([]{original::prepare(48);original::voice.currentInstrIdx=0;original::voice.setForcedSlice(47);original::voice.sliceForcedFlag=1;original::mtProject.instrument[0].sliceNumber=2;original::voice.playBeatSlice(0,24);});expect([]{original::prepare(3);original::voice.currentInstrIdx=255;original::voice.setForcedSlice(0);});CHECK(reproduced==4);
}
int main(int argc,char**argv){SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);std::setvbuf(stdout,nullptr,_IONBF,0);
 if(argc==2){original::prepare(0);original::playerEngine p;if(std::strcmp(argv[1],"baseline-fx-zero")==0)p.fxSampleSlice(7,0);else{p.trackControlParameter[1][0]=true;p.lastSeqVal[1]=7;p.endFxSlice(0);}std::printf("Unexpected baseline result: %u\n",p.currentSeqModValues.slice);return 99;}
 baselineRepro();validPlayback();validFx();regression();std::printf("PASS %llu checks\n",checks);
}