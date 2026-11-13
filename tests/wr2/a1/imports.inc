// Native completion and playback bodies, synthetic PCM; no SD/file decoding.
constexpr unsigned SAMPLE_NAME_SIZE=64,mtSampleTypeWavetable=1;
void VoiceStreamManager::clearInstrumentMetadata(int){} // filesystem/cache boundary stub
class ImportCompletion{
public:void completeLoadedInstrumentStruct(int,bool,uint32_t);
};
#pragma warning(push)
#pragma warning(disable:4244)
#include "import-completion.inc"
#pragma warning(pop)
static int16_t replacementPcm[65536]{};
static void importPlaybackTests(){
    unsigned wavCases=0;Snapshot before,after;ImportCompletion native;
    for(unsigned frames:{1u,2u,1024u,32768u,65536u})for(bool reuse:{false,true})for(bool streamed:{false,true})for(unsigned wt:{0u,2u}){
        ioSetup();auto&inst=mtProject.instrument[47];auto&mem=mtProject.sampleMemory[47];
        wr2::ui::Editor edit(wr2native::publication());CHECK(edit.invalidate(47));edit.bind(47,3,true);
        CHECK(edit.trig(0)==Result::Applied&&edit.addMember(1)==Result::Applied&&edit.addMember(2)==Result::Applied);
        CHECK(wr2native::publication().instrument[47].read(before));const auto originalCuts=inst.slices;
        playerEngine e;CHECK(e.noteOn(47,24,127));
        for(auto*v:e.playMemPtr){v->envelope.state=envelopePhaseSustain;v->envelope.wtMode=static_cast<uint8_t>(wt);}
        CHECK(e.noteOn(47,24,127));CHECK(e.left.wr2State.pendingValid&&e.right.wr2State.pendingValid);
        const auto selected=e.left.wr2State.pending.slice;const auto savedRandom=wr2native::randomState();
        // Mirrors the successful active-slot WAV branch: no default/header load.
        mem.isStreamed=streamed;mem.ramFrames=0;mem.preCacheFrames=streamed?std::min(frames,512u):frames;
        mem.address[0]=mem.address[1]=reuse?AudioPlayMemory::samples:replacementPcm;
        native.completeLoadedInstrumentStruct(47,true,frames);inst.sampleLength=frames;wr2native::Frame::sampleChanged(e,47);
        CHECK(inst.slices==originalCuts&&inst.sliceNumber==3);
        CHECK(wr2native::publication().instrument[47].read(after)&&after.topology==before.topology&&std::memcmp(&before.state,&after.state,sizeof(before.state))==0);
        starts.clear();for(auto*v:e.playMemPtr){v->envelope.count=0;v->envelopeUpdate(&v->storageBlock);}
        CHECK(starts.size()==2&&starts[0].slice==selected&&starts[1].slice==selected);
        CHECK(starts[0].start==starts[1].start&&starts[0].end==starts[1].end&&starts[0].start<=frames&&starts[0].end<=frames);
        CHECK(e.left.currentSampleLength==frames&&e.right.currentSampleLength==frames);
        CHECK(e.left.currentStartAddress==mem.address[0]&&e.right.currentStartAddress==mem.address[1]);
        CHECK(wr2native::randomState()==savedRandom);
        e.syncFxAmpLFO();CHECK(wr2native::randomState()==savedRandom&&e.left.wr2State.active.slice==selected&&e.right.wr2State.active.slice==selected);
        const auto S=static_cast<uint8_t>(Sequencer::strFxConsts::FX_TYPE_SAMPLE_SLICE);
        for(auto*v:e.playMemPtr)v->envelope.state=envelopePhaseIdle;
        CHECK(e.noteOn(47,24,-1,S,2,0,0));CHECK(wr2native::randomState()==savedRandom&&e.left.wr2State.active.slice==2&&e.right.wr2State.active.slice==2);
        ++wavCases;
    }
    // Destructive same-address/same-length replacement must recompute crossings,
    // while preserving group/selection identity and never re-drawing technically.
    ioSetup();auto&inst=mtProject.instrument[47];inst.slices[1]=1000;inst.slices[2]=2000;
    wr2::ui::Editor edit(wr2native::publication());CHECK(edit.invalidate(47));edit.bind(47,3,true);CHECK(edit.trig(0)==Result::Applied&&edit.addMember(1)==Result::Applied);
    std::fill(std::begin(AudioPlayMemory::samples),std::end(AudioPlayMemory::samples),int16_t{1000});AudioPlayMemory::samples[1050]=0;
    playerEngine e;CHECK(e.noteOn(47,25,127));CHECK(e.left.pointsInSamples.start==1050);auto savedRandom=wr2native::randomState();
    AudioPlayMemory::samples[1050]=1000;AudioPlayMemory::samples[1060]=0;native.completeLoadedInstrumentStruct(47,true,65536);
    e.syncFxAmpLFO();CHECK(e.left.pointsInSamples.start==1060&&e.right.pointsInSamples.start==1060&&wr2native::randomState()==savedRandom);
    // Partial stereo resume can leave an old L key available to a later R start.
    ioSetup();inst.slices[1]=1000;inst.slices[2]=2000;
    CHECK(edit.invalidate(47));edit.bind(47,3,true);CHECK(edit.trig(0)==Result::Applied&&edit.addMember(1)==Result::Applied);
    std::fill(std::begin(AudioPlayMemory::samples),std::end(AudioPlayMemory::samples),int16_t{1000});AudioPlayMemory::samples[1050]=0;
    playerEngine half;CHECK(half.noteOn(47,25,127));
    for(auto*v:half.playMemPtr)v->envelope.state=envelopePhaseSustain;
    CHECK(half.noteOn(47,25,127));half.left.envelope.count=0;half.left.envelopeUpdate(&half.left.storageBlock);
    CHECK(half.left.pointsInSamples.start==1050&&half.right.wr2State.pendingValid);
    savedRandom=wr2native::randomState();CHECK(wr2native::publication().instrument[47].read(before));
    AudioPlayMemory::samples[1050]=1000;AudioPlayMemory::samples[1060]=0;native.completeLoadedInstrumentStruct(47,true,65536);
    resetWork();wr2native::Frame::sampleChanged(half,47);CHECK(snapTotal==0&&pcmVisits==0);
    half.right.envelope.count=0;half.right.envelopeUpdate(&half.right.storageBlock);
    CHECK(half.right.pointsInSamples.start==1060);CHECK(snapBasepriMasked==0);CHECK(wr2native::randomState()==savedRandom);
    CHECK(wr2native::publication().instrument[47].read(after)&&after.topology==before.topology&&std::memcmp(&before.state,&after.state,sizeof(before.state))==0);
    auto key=half.right.wr2State.key.address;wr2native::Frame::sampleChanged(half,46);CHECK(half.right.wr2State.key.address==key);
    for(unsigned mask:{0u,0x20u,0x40u})for(bool enabled:{false,true}){Registers::mask=mask;Registers::nvic=enabled;wr2native::Frame::sampleChanged(half,47);CHECK(Registers::mask==mask&&Registers::nvic==enabled);}
    Registers::reset();
    std::printf("PASS LOAD WAV native completion/playback: %u cases + same-buffer PCM replacement; %llu checks; groups/cuts retained, fresh bounds, stereo, pending, S2 and Amp LFO; no real file IO\n",wavCases,checks);
}
