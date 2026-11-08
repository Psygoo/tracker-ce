static_assert(editorEffectEQ==9 && editorEffectLimiter==10 && editorEffectCompresor==11,"UI fixture native ordinal mapping");
#pragma warning(push)
#pragma warning(disable:4244 4100)
namespace originalDynamics {
#include "original-dynamics.inc"
}
#pragma warning(pop)
// Differential oracle uses unmodified CE bodies, not the streaming adapter.
template<class T>static void params(T& effect,std::initializer_list<float> values){int i=0;for(float value:values)effect.setParameter(value,i++);}
template<class T>static std::vector<int16_t> nativeResult(T& effect,unsigned n,bool stereo,unsigned first,unsigned last){
 std::vector<int16_t> input[2],output[2];int16_t* in[2],*out[2];for(unsigned ch=0;ch<2;++ch){input[ch].resize(last-first);output[ch].resize(last-first);for(unsigned i=first;i<last;++i)input[ch][i-first]=signal(stereo?ch:0,i);in[ch]=input[ch].data();out[ch]=output[ch].data();}
 effect.bindPreviewBuffers(in,out,last-first);CHECK(effect.startProcess());while(effect.getProcessState())effect.processBlock();
 std::vector<int16_t> expected(n*(stereo?2u:1u));for(unsigned ch=0;ch<(stereo?2u:1u);++ch)for(unsigned i=0;i<n;++i)expected[ch*n+i]=(i>=first&&i<last)?output[ch][i-first]:signal(ch,i);return expected;
}
static void checkNativeFile(const std::vector<int16_t>& expected,unsigned n,unsigned channels){
 FIL f{};CHECK(f_open(&f,canonical,FA_READ)==FR_OK);CHECK(f_size(&f)==sizeof(strInstrumentFile)+expected.size()*2);CHECK(f_lseek(&f,sizeof(strInstrumentFile))==FR_OK);
 int16_t block[1024];UINT got=0;for(unsigned at=0;at<expected.size();at+=1024){const auto count=std::min(1024u,unsigned(expected.size())-at);CHECK(f_read(&f,block,count*2,&got)==FR_OK&&got==count*2);CHECK(!std::memcmp(block,expected.data()+at,count*2));}
 CHECK(f_close(&f)==FR_OK);const auto&m=mtProject.sampleMemory[0];CHECK(m.isStereo==(channels==2));for(unsigned ch=0;ch<channels;++ch)CHECK(!std::memcmp(m.address[ch],expected.data()+ch*n,(m.isStreamed?m.preCacheFrames:m.ramFrames)*2));
}
template<class Current,class Original>static void applyCompare(sdmono::Edit op,unsigned id,unsigned n,bool stereo,bool stream,uint16_t a,uint16_t b,std::initializer_list<float> values,unsigned pattern=3){
 prepare(n,stereo,stream,pattern);Current current{};Original original{};params(current,values);params(original,values);
 // Establish nonzero prior Limiter history as native CE does between operations.
 CHECK(nativeResult(current,17,true,0,17)==nativeResult(original,17,true,0,17));
 const auto first=point(a,n),last=point(b,n);if(first==last)return;
 const auto metadata=mtProject.instrument[0];auto expected=nativeResult(original,n,stereo,first,last);
 const auto oldFiles=files;CHECK(sdmono::beginEdit(0,op,a,b,1,0,samplepreview::effectCopy(id),&current));CHECK(drive().find("Edited")==0);
 checkNativeFile(expected,n,stereo?2:1);CHECK(mtProject.instrument[0].sliceNumber==metadata.sliceNumber&&!std::memcmp(mtProject.instrument[0].slices,metadata.slices,sizeof(metadata.slices)));CHECK(mtProject.instrument[0].loopPoint1==metadata.loopPoint1&&mtProject.instrument[0].loopPoint2==metadata.loopPoint2);CHECK(oldFiles.count("OtherProject/instruments/source.pti")==1&&files["OtherProject/instruments/source.pti"]==oldFiles.at("OtherProject/instruments/source.pti"));
 // Next native operation must see the same retained history, not the file pass reset.
 if(nativeResult(current,8193,true,0,8193)!=nativeResult(original,8193,true,0,8193)){std::printf("history op=%u n=%u stereo=%u stream=%u\n",unsigned(op),n,unsigned(stereo),unsigned(stream));CHECK(false);}
}
template<class T>static void transactionFailures(sdmono::Edit op,unsigned id,std::initializer_list<float> values){
 prepare(65,true);T source{};params(source,values);CHECK(sdmono::beginEdit(0,op,9000,60000,1,0,samplepreview::effectCopy(id),&source));CHECK(drive().find("Edited")==0);const auto total=lastNativeOps;
 for(unsigned at=1;at<=total;++at){prepare(65,true);T effect{};params(effect,values);std::array<unsigned char,sizeof(T)> before;std::memcpy(before.data(),&effect,sizeof(T));const auto instrument=mtProject.instrument[0];const auto memory=mtProject.sampleMemory[0];std::vector<int16_t> pcm(memory.address[0],memory.address[0]+memory.ramFrames*2);auto original=files[canonical];fault=int(at);const bool started=sdmono::beginEdit(0,op,9000,60000,1,0,samplepreview::effectCopy(id),&effect);const auto result=started?drive():std::string(sdmono::result());fault=-1;if(result.find("Edited")!=0){CHECK(files[canonical]==original);CHECK(!std::memcmp(&instrument,&mtProject.instrument[0],sizeof(instrument)));CHECK(!std::memcmp(&memory,&mtProject.sampleMemory[0],sizeof(memory)));CHECK(!std::memcmp(pcm.data(),memory.address[0],pcm.size()*2));CHECK(!std::memcmp(before.data(),&effect,sizeof(T)));}CHECK(sdmono::recover());}
 // Before/after every filesystem boundary: recovery yields original or fully verified file.
 for(unsigned at=1;at<=total;++at)for(bool after:{false,true}){prepare(65,true);T effect{};params(effect,values);T reference=effect;auto expected=nativeResult(reference,65,true,point(9000,65),point(60000,65));cut=int(at);afterCut=after;try{if(sdmono::beginEdit(0,op,9000,60000,1,0,samplepreview::effectCopy(id),&effect))while(sdmono::active()&&!sdmono::done())sdmono::tick();}catch(const Crash&){}cut=-1;if(sdmono::job){sdmono::job->~Job();sdmono::job=nullptr;}CHECK(sdmono::recover());CHECK(files.count(canonical));if(!files[canonical]->generated){FIL f{};UINT got=0;int16_t pcm[130];CHECK(f_open(&f,canonical,FA_READ)==FR_OK&&f_lseek(&f,sizeof(strInstrumentFile))==FR_OK&&f_read(&f,pcm,sizeof(pcm),&got)==FR_OK&&got==sizeof(pcm));CHECK(!std::memcmp(pcm,expected.data(),sizeof(pcm)));CHECK(f_close(&f)==FR_OK);}}
 for(unsigned ticks:{0u,1u,15u,100u}){prepare(33003,true);T effect{};params(effect,values);auto original=files[canonical];CHECK(sdmono::beginEdit(0,op,0,65535,1,0,samplepreview::effectCopy(id),&effect));for(unsigned i=0;i<ticks&&!sdmono::done();++i)sdmono::tick();sdmono::cancel();CHECK(std::string(sdmono::result()).find("Cancelled")==0&&files[canonical]==original);}
 prepare(65,true);T effect{};params(effect,values);freeClusters=0;CHECK(!sdmono::beginEdit(0,op,0,65535,1,0,samplepreview::effectCopy(id),&effect)&&!sdmono::active());
 std::printf("PASS SD native dynamics edit=%u: IO failure and before/after recovery at %u boundaries, cancellation and full card\n",unsigned(op),total);
}
static void dynamicsUi(){
 using namespace modeUi;
 mtEffectEQ eq{};mtEffectLimiter limiter{};mtEffectCompresor compressor{};
 params(eq,{3,100,1,-3,2000,1,6,12000,1});params(limiter,{-6,1,100});params(compressor,{-12,4,10,100});
 for(unsigned index:{10u,11u,12u}){prepare(1000003,true);sampleEditorEffect[index]=index==10?static_cast<mtEffect*>(&eq):index==11?static_cast<mtEffect*>(&limiter):static_cast<mtEffect*>(&compressor);SE->currentEffectIdx=index;SE->selection.startPoint=12345;SE->selection.endPoint=54321;SE->confirmedDataIsChanged=false;CHECK(isSdRegionEffect(index,1000003));monoBegin();CHECK(sdmono::active());CHECK(sdmono::job->edit==sdmono::Edit(unsigned(sdmono::Edit::Equalizer)+index-10));CHECK(sdmono::job->first==point(12345,1000003)&&sdmono::job->last==point(54321,1000003));CHECK(drive().find("Edited")==0);sampleEditorEffect[index]=&dummyEffect;}
 std::puts("PASS native UI dispatch: EQ/Limiter/Compressor Apply reaches full-selection file transaction with selected native object");
}
static void dynamicsTests(){
 dynamicsUi();
 for(bool stereo:{false,true})for(bool stream:{false,true})for(unsigned n:{1u,1023u,1024u,1025u,8192u,8193u,33003u,1000003u})for(bool partial:{false,true}){
 const uint16_t a=partial?12345:0,b=partial?54321:65535;
 applyCompare<mtEffectEQ,originalDynamics::mtEffectEQ>(sdmono::Edit::Equalizer,editorEffectEQ,n,stereo,stream,a,b,{3,100,1,-3,2000,1,6,12000,1});
 applyCompare<mtEffectLimiter,originalDynamics::mtEffectLimiter>(sdmono::Edit::Limiter,editorEffectLimiter,n,stereo,stream,a,b,{-6,1,100});
 applyCompare<mtEffectCompresor,originalDynamics::mtEffectCompresor>(sdmono::Edit::Compressor,editorEffectCompresor,n,stereo,stream,a,b,{-12,4,10,100});}
 for(unsigned pattern:{1u,2u,4u}){applyCompare<mtEffectEQ,originalDynamics::mtEffectEQ>(sdmono::Edit::Equalizer,editorEffectEQ,33003,true,true,12345,54321,{-12,10,0,12,20000,3,-12,12000,1},pattern);applyCompare<mtEffectLimiter,originalDynamics::mtEffectLimiter>(sdmono::Edit::Limiter,editorEffectLimiter,33003,true,true,12345,54321,{-48,.1f,2000},pattern);applyCompare<mtEffectCompresor,originalDynamics::mtEffectCompresor>(sdmono::Edit::Compressor,editorEffectCompresor,33003,true,true,12345,54321,{-48,20,1,2000},pattern);}
 applyCompare<mtEffectEQ,originalDynamics::mtEffectEQ>(sdmono::Edit::Equalizer,editorEffectEQ,5000003,true,true,12345,54321,{3,100,1,-3,2000,1,6,12000,1});
 applyCompare<mtEffectLimiter,originalDynamics::mtEffectLimiter>(sdmono::Edit::Limiter,editorEffectLimiter,5000003,true,true,12345,54321,{-6,1,100});
 applyCompare<mtEffectCompresor,originalDynamics::mtEffectCompresor>(sdmono::Edit::Compressor,editorEffectCompresor,5000003,true,true,12345,54321,{-12,4,10,100});
 transactionFailures<mtEffectEQ>(sdmono::Edit::Equalizer,editorEffectEQ,{3,100,1,-3,2000,1,6,12000,1});transactionFailures<mtEffectLimiter>(sdmono::Edit::Limiter,editorEffectLimiter,{-6,1,100});transactionFailures<mtEffectCompresor>(sdmono::Edit::Compressor,editorEffectCompresor,{-12,4,10,100});
 prepare(65,true);mtEffectLimiter test{};params(test,{-6,1,100});auto original=files[canonical];CHECK(!sdmono::beginEdit(0,sdmono::Edit::Limiter,0,65535));CHECK(!sdmono::beginEdit(0,sdmono::Edit::Limiter,0,65535,1,0,nullptr,&test));CHECK(files[canonical]==original);
 auto& other=mtProject.instrument[1];other=mtProject.instrument[0];auto& memory=mtProject.sampleMemory[1];memory=mtProject.sampleMemory[0];memory.isStereo=false;memory.isStreamed=false;memory.address[0]=externalRamSampleMemory+130;memory.address[1]=memory.address[0];memory.ramFrames=(SAMPLE_MEMORY_SIZE-260)/2;other.sample.length=memory.ramFrames;CHECK(!sdmono::beginEdit(0,sdmono::Edit::Limiter,0,65535,1,0,samplepreview::effectCopy(editorEffectLimiter),&test)&&!sdmono::active()&&files[canonical]==original);
 std::puts("PASS SD Apply EQ/Limiter/Compressor: bit-exact original CE, continuous channels, full selection beyond Preview, RAM install and streaming, native Limiter history, metadata retained");
}
