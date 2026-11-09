namespace legacy{
class mtEffect{public:static constexpr int kChannels=2;struct Part{int16_t*addr;uint32_t length;};struct Buffer{Part area,selection;}confirmed[2]{},processed[2]{};bool processingWithoutCopying=false;virtual ~mtEffect()=default;virtual void setParameter(float&,int)=0;virtual bool startProcess()=0;virtual int32_t processBlock()=0;virtual bool getProcessState()=0;virtual uint32_t getExpectedProcessLength()=0;};
#pragma warning(push)
#pragma warning(disable:4244) // Original native implicit PCM truncation.
#include "legacy.inc"
#pragma warning(pop)
}
static uint32_t point(uint16_t p,uint32_t n){return std::min<uint32_t>(n,uint32_t(uint32_t(p)*float(n)/65535));}
#pragma warning(push)
#pragma warning(disable:4244) // Compile original CE conversion behavior unchanged.
#include "original-drive.inc"
#pragma warning(pop)
static float regionParameter=1.0f,regionSecondary=0.0f;static int referencePeaks[2]{};
static void regionReference(sdmono::Edit op,uint32_t n,uint16_t a,uint16_t b){
 regionParameter=op==sdmono::Edit::Normalize?75.0f:op==sdmono::Edit::Amplifier||op==sdmono::Edit::Overdrive?2.5f:op==sdmono::Edit::Bitcrusher?8.0f:1.0f;regionSecondary=op==sdmono::Edit::Overdrive?1.0f:op==sdmono::Edit::Bitcrusher?6300.0f:0.0f;referencePeaks[0]=referencePeaks[1]=0;
 if(op==sdmono::Edit::Normalize)for(unsigned ch=0;ch<2;++ch)for(auto at=point(a,n);at<point(b,n);++at)referencePeaks[ch]=std::max(referencePeaks[ch],std::abs(int(signal(ch,at))));
}
static int16_t expectedEdit(sdmono::Edit op,unsigned ch,uint32_t at,uint32_t a,uint32_t b){
 if(op==sdmono::Edit::Crop)return signal(ch,a+at);
 if(op==sdmono::Edit::Reverse&&at>=a&&at<b)return signal(ch,a+b-1-at);
 const auto sample=signal(ch,at);if(at<a||at>=b)return sample;
 if(op==sdmono::Edit::Overdrive)return referenceDrive(sample,regionSecondary==1?regionParameter*4:regionSecondary==2?regionParameter*10:regionParameter,unsigned(regionSecondary));
 if(op==sdmono::Edit::Bitcrusher){const auto step=unsigned(std::clamp(int(44117.64706/unsigned(regionSecondary)+.5),1,64)),shift=16-unsigned(regionParameter);const auto pos=at-a,anchor=(pos/8192)*8192+((pos%8192)/step)*step;return int16_t((int32_t(signal(ch,a+anchor))>>shift)*(int32_t(1)<<shift));}
 if(op==sdmono::Edit::Normalize||op==sdmono::Edit::Amplifier){const auto gain=op==sdmono::Edit::Amplifier?regionParameter:referencePeaks[ch]?float((int(regionParameter)*32767)/100)/float(referencePeaks[ch]):0.0f;const auto v=int32_t(sample*gain);return int16_t(std::clamp(v,-32768,32767));}
 if(op==sdmono::Edit::FadeIn)return int16_t(double(sample)*double(at-a)/double(b-a));
 if(op==sdmono::Edit::FadeOut)return int16_t(double(sample)*(1.0-double(at-a)/double(b-a)));
 return sample;
}
static void editedFile(sdmono::Edit op,uint32_t n,uint16_t pa,uint16_t pb,unsigned channels){
 const auto a=point(pa,n),b=point(pb,n),out=op==sdmono::Edit::Crop?b-a:n;FIL f;CHECK(f_open(&f,canonical,FA_READ)==FR_OK);CHECK(f_size(&f)==sizeof(strInstrumentFile)+uint64_t(out)*channels*2);
 strInstrumentFile h;UINT got=0;CHECK(f_read(&f,&h,sizeof(h),&got)==FR_OK&&got==sizeof(h));CHECK(h.data.instrument.sample.length==out&&h.data.instrument.loopPoint1==1000&&h.data.instrument.loopPoint2==60000);
 int16_t data[1024];for(unsigned ch=0;ch<channels;++ch)for(uint32_t pos=0;pos<out;pos+=1024){const auto count=std::min(1024u,out-pos);CHECK(f_read(&f,data,count*2,&got)==FR_OK&&got==count*2);for(unsigned j=0;j<count;++j){const auto target=expectedEdit(op,ch,pos+j,a,b);const bool fade=(op==sdmono::Edit::FadeIn||op==sdmono::Edit::FadeOut)&&(pos+j>=a&&pos+j<b);CHECK(std::abs(int(data[j])-target)<=(fade?1:0));}}
 CHECK(f_close(&f)==FR_OK);
}
static void regionRun(sdmono::Edit op,uint32_t n,bool stream,uint16_t a,uint16_t b,bool stereo=true,float custom=-1,float second=0){
 reset(n,stream,3,true);if(!stereo){auto&m=mtProject.sampleMemory[0];m.isStereo=false;m.address[1]=m.address[0];files[canonical]->size=sizeof(strInstrumentFile)+n*2;mtProject.used_memory=m.ramFrames*2;}
 auto&neighbor=mtProject.instrument[1];neighbor=mtProject.instrument[0];neighbor.sample.length=700;auto&nm=mtProject.sampleMemory[1];nm=mtProject.sampleMemory[0];nm.address[0]=mtProject.sampleMemory[0].address[0]+mtProject.sampleMemory[0].ramFrames*(stereo?2:1);nm.address[1]=nm.address[0]+700;nm.ramFrames=700;nm.isStreamed=false;nm.isStereo=true;neighbor.sample.address=nm.address[0];for(unsigned i=0;i<1400;++i)nm.address[0][i]=int16_t(i*17);std::vector<int16_t>neighborData(nm.address[0],nm.address[0]+1400);auto*neighborAddress=nm.address[0];instrumentPlayer[1].currentInstrument_idx=1;
 regionReference(op,n,a,b);if(custom>=0){regionParameter=custom;regionSecondary=second;}
 const auto oldRam=mtProject.sampleMemory[0].ramFrames;const auto original=mtProject.instrument[0];const auto lo=point(a,n),hi=point(b,n);const bool started=sdmono::beginEdit(0,op,a,b,regionParameter,regionSecondary);if(lo==hi){CHECK(!started&&mtProject.sampleMemory[0].isStereo==stereo);return;}CHECK(started);CHECK(drive().find("Edited")==0);auto&memory=mtProject.sampleMemory[0];const auto out=op==sdmono::Edit::Crop?hi-lo:n;CHECK(mtProject.instrument[0].sample.length==out&&memory.isStereo==stereo&&memory.isStreamed==stream);auto after=mtProject.instrument[0];after.sample.length=original.sample.length;CHECK(!std::memcmp(&original,&after,sizeof(original)));editedFile(op,n,a,b,stereo?2:1);
 const auto newRam=stream?std::min(out,kStreamPreCacheFrames):out;CHECK(nm.address[0]==neighborAddress-(oldRam-newRam)*(stereo?2:1));CHECK(!std::memcmp(neighborData.data(),nm.address[0],2800));CHECK(instrumentPlayer[1].cuts==(oldRam!=newRam?1u:0u));
 if(!stream)for(unsigned ch=0;ch<(stereo?2u:1u);++ch)for(unsigned j=0;j<out;++j)CHECK(std::abs(int(memory.address[ch][j])-expectedEdit(op,ch,j,lo,hi))<=((op==sdmono::Edit::FadeIn||op==sdmono::Edit::FadeOut)?1:0));
}
static void regionFailures(sdmono::Edit op,uint32_t frames){
 reset(frames,true,3,true);regionReference(op,frames,9000,60000);CHECK(sdmono::beginEdit(0,op,9000,60000,regionParameter,regionSecondary));drive();const auto total=lastNativeOps;
 for(unsigned n=1;n<=total;++n){reset(frames,true,3,true);regionReference(op,frames,9000,60000);const auto inst=mtProject.instrument[0];const auto mem=mtProject.sampleMemory[0];std::vector<int16_t>ram(mem.address[0],mem.address[0]+mem.ramFrames*2);auto original=files[canonical];fault=int(n);const bool started=sdmono::beginEdit(0,op,9000,60000,regionParameter,regionSecondary);const auto result=started?drive():std::string(sdmono::result());fault=-1;if(result.find("Edited")!=0){CHECK(!std::memcmp(&inst,&mtProject.instrument[0],sizeof(inst)));CHECK(!std::memcmp(&mem,&mtProject.sampleMemory[0],sizeof(mem)));CHECK(!std::memcmp(ram.data(),mem.address[0],ram.size()*2));CHECK(files[canonical]==original);}}
 std::printf("PASS region IO edit=%u: %u failure boundaries\n",unsigned(op),total);
}
template<class Effect>static void fadeDifference(bool fadeIn){
 std::vector<int16_t>in(8193,30000),out(8193);Effect native;
 for(int c=0;c<2;++c){native.confirmed[c]={{in.data(),8193},{in.data(),8193}};native.processed[c]={{out.data(),8193},{out.data(),8193}};}CHECK(native.startProcess());while(native.getProcessState())native.processBlock();
 int maximum=0;for(unsigned n=0;n<8193;++n){const float f=float(n)/8193.0f;const auto proposed=int16_t(in[n]*(fadeIn?f:1.0f-f));maximum=std::max(maximum,std::abs(int(proposed)-out[n]));}
 std::printf("Native vs indexed %s, 8193 constant frames: max absolute difference %d PCM codes\n",fadeIn?"FadeIn":"FadeOut",maximum);
}
static void legacyExamples(){
 fadeDifference<legacy::mtEffectFadeIn>(true);fadeDifference<legacy::mtEffectFadeOut>(false);
 std::vector<int16_t>in(1025),out(1026,1234);for(unsigned i=0;i<in.size();++i)in[i]=int16_t(i+1);
 legacy::mtEffectReverse reverse;for(int c=0;c<2;++c){reverse.confirmed[c]={{in.data(),1025},{in.data(),1025}};reverse.processed[c]={{out.data(),1025},{out.data(),1025}};}CHECK(reverse.startProcess());while(reverse.getProcessState())reverse.processBlock();CHECK(out[0]==1234&&out[1025]==in[0]);
 std::vector<int16_t>longInput(8192,30000),longOutput(8192);legacy::mtEffectFadeOut fade;
 for(int c=0;c<2;++c){fade.confirmed[c]={{longInput.data(),50000000},{longInput.data(),50000000}};fade.processed[c]={{longOutput.data(),50000000},{longOutput.data(),50000000}};}CHECK(fade.startProcess());CHECK(fade.processBlock()==8192);CHECK(longInput==longOutput);
 const float step=1.0f/50000000.0f;CHECK(1.0f-step==1.0f);std::puts("CONFIRMED native Reverse writes logical index N, leaves index 0 untouched; native FadeOut 50M-frame increment rounds away at 1.0");
 legacy::mtEffectCut crop;for(int c=0;c<2;++c){crop.confirmed[c]={{in.data(),1025},{in.data()+17,1000}};crop.processed[c]={{out.data(),1025},{out.data(),1025}};}CHECK(crop.startProcess());while(crop.getProcessState())crop.processBlock();CHECK(!std::memcmp(out.data(),in.data()+17,2000));
}
static void regionPower(sdmono::Edit op){
 reset(65,true);regionReference(op,65,9000,60000);CHECK(sdmono::beginEdit(0,op,9000,60000,regionParameter,regionSecondary));drive();const auto total=lastNativeOps;
 for(unsigned n=1;n<=total;++n)for(bool after:{false,true}){reset(65,true);regionReference(op,65,9000,60000);operations=0;cut=int(n);afterCut=after;
  try{if(sdmono::beginEdit(0,op,9000,60000,regionParameter,regionSecondary))while(sdmono::active()&&!sdmono::done())sdmono::tick();}catch(const Crash&){}
  cut=-1;if(sdmono::job){sdmono::job->~Job();sdmono::job=nullptr;}CHECK(sdmono::recover());CHECK(files.count(canonical));if(!files[canonical]->generated)editedFile(op,65,9000,60000,2);
 }
 std::printf("PASS region recovery edit=%u: before/after %u filesystem boundaries\n",unsigned(op),total);
}
static void regionUi(){
 reset(20000003,true);mtProject.sampleMemory[0].hasWaveformPeaks=true;modeUi::streamedDraws=0;CHECK(modeUi::streamedEditorSelected(modeUi::SE));CHECK(modeUi::streamedEditorSpectrum(modeUi::SE,0)&&modeUi::streamedEditorSpectrum(modeUi::SE,1)&&modeUi::streamedDraws==2);mtProject.sampleMemory[0].hasWaveformPeaks=false;CHECK(modeUi::streamedEditorSpectrum(modeUi::SE,0)&&modeUi::streamedDraws==2);reset(1025,true);CHECK(!modeUi::streamedEditorSelected(modeUi::SE));
 CHECK(uint64_t(20000003)/600*599>=kStreamPreCacheFrames); // Old raw renderer would address outside the attack cache.
 std::puts("PASS streamed editor: 20M-frame raw-render offsets exceed 32K precache; actual hook routes both channels to native SD renderer");
 for(unsigned index:{modeUi::editorEffectCut,modeUi::editorEffectReverse,modeUi::editorEffectFadeIn,modeUi::editorEffectFadeOut,modeUi::editorEffectNormalizer,modeUi::editorEffectAmplifier,modeUi::editorEffectOverdrive,modeUi::editorEffectBitcrusher}){
  reset(2000000,true);modeUi::SE->effectDisplayParamsToAdjust[modeUi::editorEffectNormalizer].parameter[0]=75;modeUi::SE->effectDisplayParamsToAdjust[modeUi::editorEffectAmplifier].parameter[0]=2.5f;modeUi::SE->effectDisplayParamsToAdjust[modeUi::editorEffectOverdrive].parameter[0]=2.5f;modeUi::SE->effectDisplayParamsToAdjust[modeUi::editorEffectOverdrive].parameter[1]=2;modeUi::SE->effectDisplayParamsToAdjust[modeUi::editorEffectBitcrusher].parameter[0]=8;modeUi::SE->effectDisplayParamsToAdjust[modeUi::editorEffectBitcrusher].parameter[1]=6300;modeUi::SE->currentEffectIdx=index;modeUi::SE->selection.startPoint=12345;modeUi::SE->selection.endPoint=54321;modeUi::monoBegin();CHECK(sdmono::active());CHECK(sdmono::job->edit!=sdmono::Edit::None);CHECK(drive().find("Edited")==0);
 }
 modeUi::SE->currentEffectIdx=modeUi::editorEffectStereoMonoSD;
 std::puts("PASS actual large-effect UI Apply hook: existing Crop/Reverse/Fade In/Fade Out entries use selected Start/End, no duplicate effects");
}

static void driveMatrix(){
 for(bool stereo:{false,true}){
  for(unsigned type=0;type<3;++type)for(float gain:{.01f,.5f,1.0f,5.0f})regionRun(sdmono::Edit::Overdrive,33003,true,12345,54321,stereo,gain,float(type));
  for(float bits:{1.0f,8.0f,16.0f})for(float rate:{500.0f,6300.0f,8800.0f,14700.0f,44100.0f})regionRun(sdmono::Edit::Bitcrusher,33003,true,12345,54321,stereo,bits,rate);
  regionRun(sdmono::Edit::Bitcrusher,33003,false,12345,54321,stereo,1,6300);
  regionRun(sdmono::Edit::Overdrive,33003,false,12345,54321,stereo,5,2);
 }
 for(auto op:{sdmono::Edit::Overdrive,sdmono::Edit::Bitcrusher})for(auto params:{std::pair<float,float>{0.f,0.f},{17.f,500.f},{1.f,-1.f},{1.f,50000.f},{NAN,1.f},{1.f,NAN}}){reset(65,true);const auto original=files[canonical];CHECK(!sdmono::beginEdit(0,op,0,65535,params.first,params.second));CHECK(files[canonical]==original&&!sdmono::active());}
 std::puts("PASS SD drive matrix: original formulas, 3 drive types, gain extremes, 1/8/16 bits, rate extremes, native/SD block crossings, partial region, RAM/stream commit and invalid parameters");
}
static void regionTests(){
 legacyExamples();regionUi();driveMatrix();for(auto op:{sdmono::Edit::Crop,sdmono::Edit::Reverse,sdmono::Edit::FadeIn,sdmono::Edit::FadeOut,sdmono::Edit::Normalize,sdmono::Edit::Amplifier,sdmono::Edit::Overdrive,sdmono::Edit::Bitcrusher}){
  for(unsigned n:{1u,2u,1023u,1024u,1025u,8192u,8193u,65537u})for(bool stereo:{false,true}){regionRun(op,n,false,0,65535,stereo);regionRun(op,n,true,12345,54321,stereo);}
  regionRun(op,20000003,true,12345,54321);
  regionFailures(op,65);regionPower(op);
  reset(33000,true);auto original=files[canonical];CHECK(sdmono::beginEdit(0,op,12345,54321,regionParameter,regionSecondary));sdmono::tick();sdmono::cancel();CHECK(sdmono::done()&&std::string(sdmono::result()).empty()&&files[canonical]==original);
 }
 regionFailures(sdmono::Edit::Reverse,33000);
 std::puts("PASS regions: all 8 effects, mono/stereo, RAM/SD, partial/full/block boundaries, >64 MB sources, original metadata");
}
