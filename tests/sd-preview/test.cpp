#define SAMPLE_PREVIEW_TESTS 1
#pragma warning(push)
#pragma warning(disable:4715) // Included historical main is not executed.
#define main historicalEditMain
#include "../stereo-mono/test.cpp"
#undef main
#pragma warning(pop)
#pragma warning(push)
#pragma warning(disable:4244 4100)
#include <type_traits>
namespace nativePreview {
#include "preview-dsp.inc"
}
#pragma warning(pop)
using namespace nativePreview;
#include "preview-enums.inc"
#include "preview-copies.inc"
#include "../../core/FileManager/SamplePreview.h"
// The native reader contract: RAM or planar stream, <=1024 frames per request,
// no file writes and an injectable read failure. Generation avoids full input RAM.
static unsigned reads=0,maxRead=0,failRead=0;static uint32_t maxFrame=0;static bool distantPeak=false;
static bool previewRead(unsigned slot,unsigned channel,unsigned first,unsigned count,int16_t*out){
 ++reads;maxRead=std::max(maxRead,count);maxFrame=std::max(maxFrame,first+count);if(failRead&&reads==failRead)return false;
 const auto&m=mtProject.sampleMemory[slot];if(!m.isStreamed){std::memcpy(out,m.address[channel]+first,count*2);return true;}
 for(unsigned i=0;i<count;++i)out[i]=distantPeak?(first+i==900000?int16_t(channel?16000:32000):int16_t(channel?200:100)):signal(channel,first+i);
 return true;
}

bool VoiceStreamManager::readInstrumentFramesBlocking(int slot,int channel,uint32_t first,uint32_t count,int16_t*out){return previewRead(unsigned(slot),unsigned(channel),first,count,out);}
#include "preview-native.inc"
static void prepare(unsigned n,bool stereo,bool stream=true,unsigned pattern=3){samplepreview::release();reset(n,stream,int(pattern),true);if(!stereo){auto&m=mtProject.sampleMemory[0];m.isStereo=false;m.address[1]=m.address[0];mtProject.used_memory=m.ramFrames*2;files[canonical]->size=sizeof(strInstrumentFile)+n*2;}reads=maxRead=failRead=maxFrame=0;distantPeak=false;}
static void finishPreview(){unsigned limit=0;while(samplepreview::busy()){samplepreview::tick();CHECK(++limit<1000000);}CHECK(samplepreview::ready());CHECK(maxRead<=1024);}
template<class T>static void compare(unsigned id,unsigned n,bool stereo,uint16_t a,uint16_t b,std::initializer_list<float>params,bool stream=true,unsigned pattern=3){
 prepare(n,stereo,stream,pattern);T original{};unsigned index=0;for(auto x:params){float p=x;original.setParameter(p,int(index++));}
 int16_t priorIn[2][17]{},priorOut[2][17]{};int16_t*priorInputs[2]{priorIn[0],priorIn[1]},*priorOutputs[2]{priorOut[0],priorOut[1]};for(unsigned ch=0;ch<2;++ch)for(unsigned i=0;i<17;++i)priorIn[ch][i]=signal(ch,i);original.bindPreviewBuffers(priorInputs,priorOutputs,17);CHECK(original.startProcess());while(original.getProcessState())original.processBlock();int16_t priorBefore[2][17];std::memcpy(priorBefore,priorOut,sizeof(priorOut));
 std::array<uint8_t,sizeof(T)>before;std::memcpy(before.data(),&original,sizeof(original));const auto instrument=mtProject.instrument[0];const auto mem=mtProject.sampleMemory[0];const auto filesBefore=operations;
const auto pcmFrames=mem.ramFrames*(mem.isStereo?2:1);const std::vector<int16_t>pcmBefore(mem.address[0],mem.address[0]+pcmFrames);
 const auto lo=point(a,n),hi=point(b,n),length=std::min(hi-lo,samplepreview::kMaximumFrames);
 CHECK(samplepreview::begin(0,id,original,a,b));CHECK(!sdmono::begin(0)); // Exclusive workspace ownership.
 finishPreview();CHECK(samplepreview::frames()==length&&maxFrame<=hi);CHECK(operations==filesBefore);CHECK(!std::memcmp(pcmBefore.data(),mem.address[0],pcmBefore.size()*2));
 CHECK(!std::memcmp(before.data(),&original,sizeof(original)));CHECK(!std::memcmp(priorBefore,priorOut,sizeof(priorOut)));CHECK(!std::memcmp(&instrument,&mtProject.instrument[0],sizeof(instrument)));CHECK(!std::memcmp(&mem,&mtProject.sampleMemory[0],sizeof(mem)));
 std::vector<int16_t>in[2],out[2];int16_t*inputs[2],*outputs[2];
 for(unsigned ch=0;ch<2;++ch){in[ch].resize(length);out[ch].resize(length);for(unsigned i=0;i<length;++i)in[ch][i]=signal(stereo?ch:0,lo+i);inputs[ch]=in[ch].data();outputs[ch]=out[ch].data();}
 auto reference=original;reference.bindPreviewBuffers(inputs,outputs,length);
 if constexpr(std::is_same_v<T,mtEffectNormalizer>){int peaks[2]{};for(unsigned ch=0;ch<2;++ch)for(unsigned at=lo;at<hi;++at)peaks[ch]=std::max(peaks[ch],ce_pcm_peak(signal(stereo?ch:0,at)));CHECK(reference.startPreview(peaks));}
 else if constexpr(std::is_same_v<T,mtEffectStereoMono>){reference.setInputStereo(stereo);CHECK(reference.startProcess());}
 else CHECK(reference.startProcess());
 while(reference.getProcessState())reference.processBlock();
 for(unsigned ch=0;ch<2;++ch)CHECK(!std::memcmp(out[ch].data(),samplepreview::data(ch),length*2));
 const auto samples=std::vector<int16_t>(samplepreview::data(0),samplepreview::data(0)+length);samplepreview::release();CHECK(!samplepreview::active());CHECK(sdmono::begin(0));sdmono::cancel();sdmono::result();
 CHECK(samplepreview::begin(0,id,original,a,b));finishPreview();CHECK(std::equal(samples.begin(),samples.end(),samplepreview::data(0)));samplepreview::release();
}
static void matrix(){
 for(bool stereo:{false,true})for(unsigned n:{1u,8192u,264599u,264600u,264601u,440999u,441000u,441001u,1000003u})for(bool partial:{false,true}){
  const uint16_t a=partial?12345:0,b=partial?54321:65535;if(point(a,n)==point(b,n))continue;
  compare<mtEffectNormalizer>(editorEffectNormalizer,n,stereo,a,b,{75});compare<mtEffectAmplifier>(editorEffectAmplifier,n,stereo,a,b,{2.5f});
  compare<mtEffectOverdrive>(editorEffectOverdrive,n,stereo,a,b,{5,1});compare<mtEffectBitcrusher>(editorEffectBitcrusher,n,stereo,a,b,{8,6300});
  compare<mtEffectStereoMono>(editorEffectStereoMonoSD,n,stereo,a,b,{0,0});compare<mtEffectEQ>(editorEffectEQ,n,stereo,a,b,{3,100,1,-3,2000,1,6,12000,1});
  compare<mtEffectLimiter>(editorEffectLimiter,n,stereo,a,b,{-6,1,100});compare<mtEffectCompresor>(editorEffectCompresor,n,stereo,a,b,{-12,4,10,100});
 }
 for(unsigned pattern:{1u,2u,4u}){compare<mtEffectAmplifier>(editorEffectAmplifier,1000003,true,12345,54321,{2.5f},false,pattern);compare<mtEffectStereoMono>(editorEffectStereoMonoSD,1000003,true,12345,54321,{3,6},true,pattern);compare<mtEffectNormalizer>(editorEffectNormalizer,1000003,true,12345,54321,{100},true,pattern);}
 compare<mtEffectBitcrusher>(editorEffectBitcrusher,1000003,false,12345,54321,{1,6300},false);
 std::puts("PASS Preview PCM: 8 native effects, mono/stereo, 10-second boundaries, partial ranges, repeated preview and original state unchanged");
}
static void edgeCases(){
 prepare(1000003,true);mtEffectAmplifier applied{};float unity=1;applied.setParameter(unity,0);CHECK(samplepreview::begin(0,editorEffectAmplifier,applied,12345,54321));finishPreview();samplepreview::release();regionReference(sdmono::Edit::Amplifier,1000003,12345,54321);CHECK(sdmono::beginEdit(0,sdmono::Edit::Amplifier,12345,54321,regionParameter,regionSecondary));CHECK(drive().find("Edited")==0);editedFile(sdmono::Edit::Amplifier,1000003,12345,54321,2);

 prepare(1000003,true);mtEffectAmplifier gainEffect{};float gain=1;gainEffect.setParameter(gain,0);CHECK(samplepreview::begin(0,editorEffectAmplifier,gainEffect,0,65535));finishPreview();const auto firstResult=samplepreview::data(0)[0];samplepreview::release();gain=.5f;gainEffect.setParameter(gain,0);CHECK(samplepreview::begin(0,editorEffectAmplifier,gainEffect,0,65535));finishPreview();CHECK(samplepreview::data(0)[0]==firstResult/2);samplepreview::release();

 prepare(1000003,true);distantPeak=true;mtEffectNormalizer n{};float level=100;n.setParameter(level,0);CHECK(samplepreview::begin(0,editorEffectNormalizer,n,0,65535));finishPreview();CHECK(maxFrame==1000003&&samplepreview::frames()==441000);CHECK(samplepreview::data(0)[0]==int16_t(100*(32767.0f/32000)));CHECK(samplepreview::data(1)[0]==int16_t(200*(32767.0f/16000)));samplepreview::release();
 for(unsigned fail:{1u,2u,50u,520u,1000u,2000u,2300u,2700u}){prepare(1000003,true);failRead=fail;CHECK(samplepreview::begin(0,editorEffectNormalizer,n,0,65535));while(samplepreview::busy())samplepreview::tick();CHECK(!samplepreview::ready());samplepreview::release();}
 for(unsigned ticks:{0u,1u,100u,530u,1000u,2000u,2480u,2820u}){prepare(1000003,true);CHECK(samplepreview::begin(0,editorEffectNormalizer,n,0,65535));for(unsigned i=0;i<ticks&&samplepreview::busy();++i)samplepreview::tick();samplepreview::release();CHECK(!samplepreview::active());}
 prepare(1000003,true);CHECK(samplepreview::begin(0,editorEffectNormalizer,n,0,65535));mtProject.instrument[0].sliceNumber++;samplepreview::tick();CHECK(!samplepreview::ready()&&!samplepreview::busy());samplepreview::release();
 prepare(1000003,true);CHECK(!samplepreview::begin(0,editorEffectCut,n,0,65535));CHECK(!samplepreview::begin(48,editorEffectNormalizer,n,0,65535));CHECK(!samplepreview::begin(0,editorEffectNormalizer,n,123,123));
 prepare(1000003,true);CHECK(sdmono::begin(0));CHECK(!samplepreview::begin(0,editorEffectNormalizer,n,0,65535));sdmono::cancel();sdmono::result();
 prepare(1000003,true);n.undoParams.isEnable=true;CHECK(!samplepreview::begin(0,editorEffectNormalizer,n,0,65535));n.undoParams.isEnable=false;
 n.processing.isLoadedData=true;CHECK(!samplepreview::begin(0,editorEffectNormalizer,n,0,65535));n.processing.isLoadedData=false;
 n.processing.processParams.state=true;CHECK(!samplepreview::begin(0,editorEffectNormalizer,n,0,65535));n.processing.processParams.state=false;
 n.loadingParams.processingParams.state=true;CHECK(!samplepreview::begin(0,editorEffectNormalizer,n,0,65535));n.loadingParams.processingParams.state=false;
 prepare(1000003,true);mtProject.sampleMemory[0].address[1]=nullptr;CHECK(!samplepreview::begin(0,editorEffectNormalizer,n,0,65535));
 prepare(1000003,true);mtProject.sampleMemory[0].preCacheFrames=999999;CHECK(!samplepreview::begin(0,editorEffectNormalizer,n,0,65535));
 for(bool stereo:{false,true}){prepare(1000003,stereo);const auto live=mtProject.sampleMemory[0].ramFrames*(stereo?2u:1u);auto&inst=mtProject.instrument[1];inst=mtProject.instrument[0];auto&mem=mtProject.sampleMemory[1];mem=mtProject.sampleMemory[0];mem.isStereo=false;mem.isStreamed=false;mem.address[0]=externalRamSampleMemory+live;mem.address[1]=mem.address[0];mem.ramFrames=(SAMPLE_MEMORY_SIZE-1400000u-live*2)/2;inst.sample.length=mem.ramFrames;const auto ioBefore=operations;CHECK(!samplepreview::begin(0,editorEffectNormalizer,n,0,65535)&&!samplepreview::active()&&operations==ioBefore);}
 prepare(1000003,true);auto&other=mtProject.instrument[1];other=mtProject.instrument[0];other.sample.length=(SAMPLE_MEMORY_SIZE-mtProject.sampleMemory[0].ramFrames*4)/2;auto&m=mtProject.sampleMemory[1];m=mtProject.sampleMemory[0];m.isStereo=false;m.isStreamed=false;m.address[0]=mtProject.sampleMemory[0].address[0]+mtProject.sampleMemory[0].ramFrames*2;m.address[1]=m.address[0];m.ramFrames=other.sample.length;CHECK(!samplepreview::begin(0,editorEffectNormalizer,n,0,65535)&&!samplepreview::active());
 prepare(1000003,true);const auto start=mtProject.sampleMemory[0].ramFrames*2;const unsigned each=(SAMPLE_MEMORY_SIZE/2-start)/47;for(unsigned slot=1;slot<48;++slot){mtProject.instrument[slot]=mtProject.instrument[0];mtProject.instrument[slot].sample.length=each;auto&mem=mtProject.sampleMemory[slot];mem=mtProject.sampleMemory[0];mem.isStereo=false;mem.isStreamed=false;mem.ramFrames=each;mem.address[0]=externalRamSampleMemory+start+(slot-1)*each;mem.address[1]=mem.address[0];}const auto firstSlot=mtProject.instrument[0];CHECK(!samplepreview::begin(0,editorEffectNormalizer,n,0,65535)&&!samplepreview::active());CHECK(!std::memcmp(&firstSlot,&mtProject.instrument[0],sizeof(firstSlot)));
 std::puts("PASS Preview lifecycle: distant normalization peak, read failures, cancellation, context invalidation, native availability and Apply exclusion");
}
#include "dynamics.inc"
int main(){std::setvbuf(stdout,nullptr,_IONBF,0);dynamicsTests();matrix();compare<mtEffectNormalizer>(editorEffectNormalizer,50000003,true,12345,54321,{100});compare<mtEffectEQ>(editorEffectEQ,50000003,true,12345,54321,{3,100,1,-3,2000,1,6,12000,1});edgeCases();std::printf("PASS SD Preview: %u checks; native DSP copies and production bounded backend; no hardware validation\n",checks);}
