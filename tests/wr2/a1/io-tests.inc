// Extracted CE exporter/lifecycle bodies, RAM sink and synthetic storage backend.
#include "io-refresh.inc"
static int16_t ioRight[65536]{};
static unsigned ioTransfers=0,ioSchedules=0,ioRendered=0,ioTicks=0,ioFileUpdates=0,ioEngineUpdates=0;
static std::function<void()> ioTrigger;
static uint32_t ioFetch=0;
bool VoiceStreamManager::startAsyncSectorReadForJob(uint8_t channel){
    CHECK(!Registers::mask&&Registers::nvic);++ioTransfers;
    currentJob.channel=channel;currentJob.intraSectorByteOffset=0;
    auto*dst=reinterpret_cast<int16_t*>(s_dmaStagingBuffer);
    for(unsigned n=0;n<currentJob.fetchFrameCount;++n)dst[n]=static_cast<int16_t>(channel?2000:1000);
    return true;
}
bool VoiceStreamManager::scheduleNextVoiceJob(bool synchronous){
    CHECK(!Registers::mask&&Registers::nvic);++ioSchedules;
    if(!ioFetch)return false;
    ioFetch=0;auto&v=voices[0];currentJob=AsyncJob{};currentJob.inProgress=true;
    currentJob.voiceIdx=0;currentJob.instrIdx=v.instrIdx;currentJob.expectedGeneration=v.generation;
    currentJob.totalChannels=2;currentJob.fetchStartFrame=v.currentFrame;currentJob.fetchFrameCount=128;
    currentJob.targetType=AsyncTargetType::kRingForward;
    if(synchronous){while(currentJob.inProgress){CHECK(startAsyncSectorReadForJob(currentJob.channel));completeChannelTransfer();}}
    else {CHECK(startAsyncSectorReadForJob(0));currentJob.transferLaunched=true;}
    return true;
}
void VoiceStreamManager::fillJobSynchronously(const AsyncJob&){CHECK(false);}
void Sequencer::tick(float){++ioTicks;if(ioTrigger)ioTrigger();}
struct {void update(){++ioFileUpdates;CHECK(!Registers::mask&&Registers::nvic);}}newFileManager;
struct {void update(){++ioEngineUpdates;CHECK(!Registers::mask&&Registers::nvic);}}engine;
struct elapsedMillisAudio{inline static float virtualMicros=0;};
struct elapsedMicrosAudio{inline static float virtualMicros=0;};
constexpr float kAudioSampleRate=44100;
static void artificial_isr(){CHECK(!Registers::mask&&Registers::nvic);}
class mtAudioExport{
public:
    enum{stem_master=0,stem_totalCount=3};bool renderFinished=false,master=true;
    bool exportOnlyMaster(){return master;}
    void handleStem(unsigned){++ioRendered;CHECK(!Registers::mask&&Registers::nvic);}
    void updateRender();
};
#include "io-export-body.inc"
static void ioSetup(){
    reset();baselineNative=true;
    for(auto&m:mtProject.sampleMemory){m.isStreamed=false;m.isStereo=true;m.preCacheFrames=65536;m.address[0]=AudioPlayMemory::samples;m.address[1]=ioRight;}
    std::fill(std::begin(AudioPlayMemory::samples),std::end(AudioPlayMemory::samples),int16_t{0});
    auto&i=mtProject.instrument[47];i.sliceNumber=3;i.slices[0]=0;i.slices[1]=22000;i.slices[2]=44000;i.sample.length=i.sampleLength=65536;
    voiceStreamManager=VoiceStreamManager{};
    std::memset(s_instrumentSliceCache,0,sizeof(s_instrumentSliceCache));std::fill(std::begin(s_instrumentSliceCacheValid),std::end(s_instrumentSliceCacheValid),false);
    ioFetch=0;ioTrigger={};ioRendered=ioTicks=ioFileUpdates=ioEngineUpdates=0;
}
static void ioTests(){
    ioSetup();Snapshot snapshot;
#ifdef A1_FIRMWARE_BRIDGE
    for(unsigned i=0;i<48;++i)CHECK(!wr2native::publication().instrument[i].read(snapshot));
    wr2::ui::Editor ui(wr2native::publication());ui.bind(47,3,true);
    CHECK(ui.trig(0)==Result::Applied);CHECK(ui.addMember(1)==Result::Applied);CHECK(ui.addMember(2)==Result::Applied);
    CHECK(ui.weight(0,-100)==Result::Applied);
    CHECK(ui.weight(1,75-ui.weight(1))==Result::Applied);
    CHECK(ui.weight(0,60)==Result::Applied);ui.finish();
    for(unsigned i=0;i<48;++i)CHECK(wr2native::publication().instrument[i].read(snapshot)==(i==47));
    CHECK(snapshot.state.sliceCount==3&&snapshot.state.weight[0]==60&&snapshot.state.weight[1]==30&&snapshot.state.weight[2]==10);
#else
    (void)snapshot;
#endif
    // Real cache boundary/invalid-index behavior, including stereo base selection.
    const int16_t* cachePtr=nullptr;uint32_t cacheFrames=0;
    s_instrumentSliceCacheValid[47]=true;s_instrumentSliceCache[47][1]={22000,128,128};
    CHECK(voiceStreamManager.getSlicePreCache(47,1,1,22005,cachePtr,cacheFrames));
    CHECK(cachePtr==mtProject.sampleMemory[47].address[1]+133&&cacheFrames==123);
    for(int instr:{-1,48,47})CHECK(!voiceStreamManager.getSlicePreCache(instr,48,0,0,cachePtr,cacheFrames)&&!cachePtr&&!cacheFrames);
    for(unsigned start:{21999u,22128u})CHECK(!voiceStreamManager.getSlicePreCache(47,1,0,start,cachePtr,cacheFrames));
    Random expected(0x57523231u);unsigned cases=0;
    for(unsigned pre:{0u,2048u,65536u})for(bool reverse:{false,true})for(bool cached:{false,true})for(bool bypass:{false,true})for(bool delayed:{false,true})for(bool offline:{false,true}){
        ioSetup();Registers::context=offline?16:3;playerEngine e;e.left.voiceIndex=e.right.voiceIndex=0;
        auto&m=mtProject.sampleMemory[47];m.isStreamed=true;m.preCacheFrames=pre;
        for(unsigned c=0;c<2;++c){e.playMemPtr[c]->reverseDirectionFlag=reverse;e.playMemPtr[c]->envelope.state=static_cast<uint8_t>(delayed?envelopePhaseSustain:envelopePhaseIdle);}
        s_instrumentSliceCacheValid[47]=cached;
        for(unsigned s=0;s<3;++s){s_instrumentSliceCache[47][s]={static_cast<uint32_t>(mtProject.instrument[47].slices[s])*65536u/65535u,128,static_cast<uint16_t>(s*128)};}
        const unsigned ticket=bypass?0:expected.percent();const unsigned chosen=bypass?2:ticket<60?0:ticket<90?1:2;
        const auto S=static_cast<uint8_t>(Sequencer::strFxConsts::FX_TYPE_SAMPLE_SLICE);
#ifdef A1_FIRMWARE_BRIDGE
        CHECK(bypass?e.noteOn(47,24,-1,S,2,0,0):e.noteOn(47,24,127));
        CHECK(wr2native::randomState()==expected.state());
#else
        CHECK(bypass?e.noteOn(47,24,-1,S,2,0,0):e.noteOn(47,static_cast<int8_t>(24+chosen),127));
#endif
        if(delayed)for(auto*v:e.playMemPtr){v->envelope.count=0;v->envelopeUpdate(&v->storageBlock);}
        CHECK(starts.size()==2&&starts[0].slice==chosen&&starts[1].slice==chosen);
        CHECK(starts[0].start==starts[1].start&&starts[0].end==starts[1].end);
        auto&v=voiceStreamManager.voices[0];CHECK(v.active&&v.instrIdx==47&&v.generation==1);
        CHECK(v.startFrame==e.left.pointsInSamples.start&&v.endFrame==e.left.pointsInSamples.end);
        CHECK(v.currentFrame==(reverse?v.endFrame-1:v.startFrame));
        CHECK(e.left.currentSliceCacheFrames==(!reverse&&cached?128u:0u));
        if(!reverse&&cached){
            CHECK(e.left.currentSliceCacheAddress==m.address[0]+chosen*128);
            CHECK(e.right.currentSliceCacheAddress==m.address[1]+chosen*128);
        }
        CHECK(snapBasepriMasked==0);CHECK(!Registers::mask&&Registers::nvic);
        std::printf("IO case=%u pre=%u reverse=%u cache=%u S2=%u delayed=%u offline=%u slice=%u start=%u end=%u cachedFrames=%u snap=%u masked=%u\n",cases++,pre,reverse,cached,bypass,delayed,offline,chosen,v.startFrame,v.endFrame,e.left.currentSliceCacheFrames,snapTotal,snapOutputMasked);
        // Pump real update/updateSynchronous and real stereo completion with synthetic sectors.
        Registers::context=16;FileSystem fs;voiceStreamManager.fs=&fs;ioFetch=1;
        if(offline)voiceStreamManager.updateSynchronous();else {voiceStreamManager.update();voiceStreamManager.update();voiceStreamManager.update();}
        v.fadeInCounter=32;
        CHECK(voiceStreamManager.getSample(0,0,v.currentFrame,nullptr,0)==1000);
        CHECK(voiceStreamManager.getSample(0,1,v.currentFrame,nullptr,0)==2000);
        CHECK(voiceStreamManager.getSample(0,0,v.currentFrame+129,nullptr,0)==0);
        // Retigger invalidates old in-flight transfer; completion may not publish stale ring data.
        voiceStreamManager.currentJob.inProgress=true;voiceStreamManager.currentJob.voiceIdx=0;voiceStreamManager.currentJob.expectedGeneration=v.generation;
        voiceStreamManager.notifyNoteOff(0);CHECK(!v.active);voiceStreamManager.completeChannelTransfer();CHECK(!voiceStreamManager.currentJob.inProgress);
#ifdef A1_FIRMWARE_BRIDGE
        CHECK(wr2native::randomState()==expected.state());
#endif
    }
    // Actual exporter loop, complete body. Tick callback enters native NoteOn;
    // RAM sink records stems, no WAV/filesystem/DSP fidelity claim.
    for(bool master:{false,true}){
        ioSetup();FileSystem fs;voiceStreamManager.fs=&fs;playerEngine e;e.left.voiceIndex=e.right.voiceIndex=0;
        mtProject.sampleMemory[47].isStreamed=true;mtProject.sampleMemory[47].preCacheFrames=2048;
        mtAudioExport renderer;renderer.master=master;unsigned triggers=0;
        ioTrigger=[&]{starts.clear();for(auto*v:e.playMemPtr)v->envelope.state=envelopePhaseIdle;
            unsigned ticket=expected.percent(),chosen=ticket<60?0:ticket<90?1:2;
#ifdef A1_FIRMWARE_BRIDGE
            CHECK(e.noteOn(47,24,127));CHECK(wr2native::randomState()==expected.state());
#else
            CHECK(e.noteOn(47,static_cast<int8_t>(24+chosen),127));
#endif
            CHECK(starts.size()==2&&starts[0].slice==chosen&&starts[1].slice==chosen);ioFetch=1;
            if(++triggers==12)renderer.renderFinished=true;
        };
        renderer.updateRender();CHECK(ioTicks==12&&ioFileUpdates==12&&ioEngineUpdates==12);
        CHECK(ioRendered==12u*(master?1u:3u));CHECK(!Registers::mask&&Registers::nvic);
        std::printf("EXPORT master=%u ticks=%u rendered=%u virtualUs=%.0f\n",master,ioTicks,ioRendered,elapsedMicrosAudio::virtualMicros);
    }
#ifdef A1_FIRMWARE_BRIDGE
    ioSetup();playerEngine e;unsigned hist[3]{};
    for(unsigned n=0;n<20000;++n){starts.clear();for(auto*v:e.playMemPtr)v->envelope.state=envelopePhaseIdle;
        auto ticket=expected.percent();unsigned chosen=ticket<60?0:ticket<90?1:2;
        CHECK(e.noteOn(47,24,127));CHECK(starts.size()==2&&starts[0].slice==chosen&&starts[1].slice==chosen);
        CHECK(wr2native::randomState()==expected.state());++hist[chosen];
    }
    double chi=0;for(unsigned s=0;s<3;++s){double mean=20000.0*(s==0?.6:s==1?.3:.1);chi+=(hist[s]-mean)*(hist[s]-mean)/mean;}
    CHECK(chi<20);std::printf("NATIVE-DISTRIBUTION n=20000 master=%u member1=%u member2=%u chi2=%.4f\n",hist[0],hist[1],hist[2],chi);
    // Direct Members and unselected slot never consume WR2 RNG.
    for(unsigned instr:{46u,47u})for(int note:{25,26}){starts.clear();for(auto*v:e.playMemPtr)v->envelope.state=envelopePhaseIdle;auto savedRandom=wr2native::randomState();CHECK(e.noteOn(static_cast<uint8_t>(instr),static_cast<int8_t>(note),127));CHECK(wr2native::randomState()==savedRandom&&starts.size()==2&&starts[0].slice==static_cast<unsigned>(note-24));}
    // Different slice count/mode falls through, without changing the published weights.
    for(unsigned mode=0;mode<8;++mode){mtProject.instrument[47].playMode=static_cast<uint8_t>(mode);if(mode==5)mtProject.instrument[47].sliceNumber=4;for(auto*v:e.playMemPtr)v->envelope.state=envelopePhaseIdle;auto savedRandom=wr2native::randomState();CHECK(e.noteOn(47,24,127));CHECK(wr2native::randomState()==savedRandom);}
    CHECK(wr2native::publication().instrument[47].read(snapshot)&&snapshot.state.weight[0]==60&&snapshot.state.sliceCount==3);
#endif
    std::printf("PASS native IO/export: %u differential streaming cases; %llu checks; storage scheduler, HAL, DSP and file sink are seams; no hardware validation\n",cases,checks);
}
