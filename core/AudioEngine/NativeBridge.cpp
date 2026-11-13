#if TRACKER_CE_ENABLE_WR2
#ifndef WR2_BRIDGE_HOST
#include "mtAudioEngine.h"
#include "mtSequencer.h"
#include "MIMXRT1051.h"
#endif
#include "NativeBridge.h"
#ifndef WR2_BRIDGE_HOST
static_assert(__NVIC_PRIO_BITS==4,"Recheck WR2 priority threshold");
#endif
namespace wr2native {
using namespace wr2;
using namespace wr2::audio;
namespace {
PublishedBank bank;
Random rng(0x57523231u);
unsigned basepri(){
#ifdef WR2_BRIDGE_HOST
    return Registers::basepri();
#else
    return __get_BASEPRI();
#endif
}
void setMask(unsigned n,bool restore){
#ifdef WR2_BRIDGE_HOST
    if(restore)Registers::restore(n);else Registers::raise(n);
#else
    const auto p=__get_PRIMASK();__disable_irq();
    if(restore)__set_BASEPRI(n);else __set_BASEPRI_MAX(n);
    __DSB();__ISB();__set_PRIMASK(p);
#endif
}
bool outputEnabled(){
#ifdef WR2_BRIDGE_HOST
    return Registers::audioEnabled();
#else
    return NVIC_GetEnableIRQ(DMA0_DMA16_IRQn)!=0;
#endif
}
void outputEnable(bool on){
#ifdef WR2_BRIDGE_HOST
    Registers::audioEnable(on);
#else
    if(on)NVIC_EnableIRQ(DMA0_DMA16_IRQn);else NVIC_DisableIRQ(DMA0_DMA16_IRQn);
#endif
}
struct Guard {unsigned saved=basepri();Guard(){setMask(0x30,false);}~Guard(){setMask(saved,true);}};
Selection nativeSelection(unsigned i,int note){
    Selection s;s.instrument=static_cast<uint8_t>(i);s.note=static_cast<int8_t>(note);
    s.sliceCount=mtProject.instrument[i].sliceNumber;
    s.kind=s.sliceCount?Kind::Slice:Kind::WholeSample;
    const int n=note-FIRST_BEAT_SLICE_NOTE;
    s.slice=static_cast<uint8_t>(!s.sliceCount||n<0?0:n>=s.sliceCount?s.sliceCount-1:n);
    return s;
}
}
PublishedBank&publication(){return bank;}
uint32_t randomState(){Guard g;return rng.state();}
void Frame::sampleChanged(playerEngine&e,uint8_t instrument){
    Guard guard;
    for(unsigned c=0;c<2&&c<e.playMemPtr.size();++c){auto*voice=e.playMemPtr[c];
        if(voice&&voice->wr2State.activeValid&&voice->wr2State.active.instrument==instrument)voice->wr2State.key.address=nullptr;
    }
}
#ifndef WR2_BRIDGE_HOST
void sampleChanged(uint8_t instrument){
    if(instrument>=kMaxInstruments)return;
    // Sixteen voice-channel keys at most. No PCM, IO, wait, note dispatch or RNG.
    Guard guard;
    for(auto&voice:instrumentPlayer)Frame::sampleChanged(voice,instrument);
}
#endif
bool Frame::same(const Geometry&a,const Geometry&b){return a.address==b.address&&a.length==b.length&&a.start==b.start&&a.end==b.end&&a.instrument==b.instrument&&a.count==b.count&&a.slice==b.slice&&a.snap==b.snap;}
bool Frame::sameSelection(const Selection&a,const Selection&b){return a.instrument==b.instrument&&a.slice==b.slice&&a.sliceCount==b.sliceCount&&a.note==b.note&&a.kind==b.kind&&a.direct==b.direct&&a.randomized==b.randomized&&a.topology==b.topology;}
bool Frame::live(const Selection&s){return s.instrument<kMaxInstruments&&(!s.topology||bank.instrument[s.instrument].context()==s.topology)&&mtProject.instrument[s.instrument].playMode==playModeBeatSlice&&s.sliceCount==mtProject.instrument[s.instrument].sliceNumber&&s.sliceCount<=kMaxSlices&&((s.kind==Kind::WholeSample&&!s.sliceCount)||(s.kind==Kind::Slice&&s.sliceCount&&s.slice<s.sliceCount));}
Geometry Frame::geometry(const Selection&s){
    Geometry k;k.instrument=s.instrument;k.count=mtProject.instrument[s.instrument].sliceNumber;k.slice=s.slice;
    k.address=mtProject.sampleMemory[s.instrument].address[0];k.length=mtProject.instrument[s.instrument].sample.length;k.snap=AudioPlayMemory::isSnapToZeroEnabled;
    if(k.count){k.start=mtProject.instrument[s.instrument].slices[k.slice];k.end=(!mtProject.instrument[s.instrument].slicePlayThru&&k.slice+1<k.count)?mtProject.instrument[s.instrument].slices[k.slice+1]:65535;}
    return k;
}
Points Frame::prepare(const Selection&s,AudioPlayMemory*source){
    Points p;if(!live(s))return p;p.key=geometry(s);
    if(source){Guard g;if(source->currentPlayMode==playModeBeatSlice&&source->wr2State.activeValid&&sameSelection(source->wr2State.active,s)&&same(source->wr2State.key,p.key)){p.start=source->pointsInSamples.start;p.end=source->pointsInSamples.end;p.valid=true;return p;}}
#ifdef WR2_BRIDGE_HOST
    p.start=AudioPlayMemory::snapNativeBody(p.key.start,p.key.length,const_cast<int16_t*>(p.key.address));
    p.end=AudioPlayMemory::snapNativeBody(p.key.end,p.key.length,const_cast<int16_t*>(p.key.address));p.valid=true;return p;
#else
    p.start=AudioPlayMemory::calculatePointPositionAndSnapToZero(p.key.start,p.key.length,const_cast<int16_t*>(p.key.address));
    p.end=AudioPlayMemory::calculatePointPositionAndSnapToZero(p.key.end,p.key.length,const_cast<int16_t*>(p.key.address));p.valid=true;return p;
#endif
}
void Frame::lock(){saved_=basepri();setMask(0x30,false);audioEnabled_=outputEnabled();locked_=true;}
void Frame::unlock(){if(locked_){outputEnable(audioEnabled_);locked_=false;setMask(saved_,true);}}
void Frame::install(){for(unsigned c=0;c<2;++c)if(voices_[c]){previous_[c]=voices_[c]->wr2State.frame;voices_[c]->wr2State.frame=this;}}
bool Frame::needMain(playerEngine&e,bool fx,uint8_t f2,int8_t velocity)const{
    if(needTechnical(e,fx,f2,velocity))return true;
    const auto L=Sequencer::strFxConsts::FX_TYPE_VOLUME_LFO;
    const auto&amp=mtProject.instrument[main_.instrument].envelope[envAmp];
    // clearFxAmpRateLFO restores Sustain for a non-idle voice when the last
    // sequencer Amp LFO is ended (unless native/performance LFO remains).
    // That accepted NoteOn will queue, so preparing its PCM here is unnecessary.
    const bool clearsToSustain=fx&&(e.lastSeqFx[0]==L||e.lastSeqFx[1]==L)&&
        !e.trackControlParameter[static_cast<int>(playerEngine::controlType::performanceMode)][static_cast<int>(playerEngine::parameterList::lfoAmp)]&&
        (!amp.enable||(amp.loop!=1&&amp.loop!=3));
    for(auto*v:voices_)if(v){
        if(v->envelope.state==AudioPlayMemory::envelopePhaseIdle)return true;
        if(!clearsToSustain&&(v->envelope.state==AudioPlayMemory::envelopePhaseDelay||v->envelope.state==AudioPlayMemory::envelopePhaseForced))return true;
    }
    return false;
}
bool Frame::needTechnical(playerEngine&e,bool fx,uint8_t f2,int8_t velocity)const{
    if(velocity>=0&&e.isFxVelocity(f2))f2=Sequencer::strFxConsts::FX_TYPE_VOLUME;
    return fx&&e.lastSeqFx[0]==Sequencer::strFxConsts::FX_TYPE_VOLUME_LFO&&f2==Sequencer::strFxConsts::FX_TYPE_VOLUME_LFO;
}
void Frame::prepareTechnical(uint8_t target){
    Selection s;AudioPlayMemory* source=nullptr;
    {Guard g;for(auto*v:voices_)if(v&&v->currentPlayMode==playModeBeatSlice&&v->wr2State.activeValid&&v->wr2State.active.instrument==target&&live(v->wr2State.active)){s=v->wr2State.active;source=v;observedStart_=v->pointsInSamples.start;observedEnd_=v->pointsInSamples.end;break;}}
    if(!source)return;
    // Native technical restarts recalculate from current PCM. Do not cache old
    // zero crossings across destructive edits that can keep address/length/cuts.
    const auto p=prepare(s);
    for(unsigned c=0;c<2;++c)if(voices_[c]){technical_[c]=s;technicalPoints_[c]=p;hasTechnical_[c]=true;}
}
Frame::Frame(playerEngine&e,uint8_t i,int8_t n,bool fx,uint8_t f1,uint8_t v1,uint8_t f2,uint8_t v2,int8_t velocity){
    if(i>=kMaxInstruments||mtProject.instrument[i].playMode!=playModeBeatSlice||mtProject.instrument[i].sliceNumber>kMaxSlices)return;
    Snapshot snapshot;bool published=bank.instrument[i].read(snapshot);
    // Normal firmware starts with no groups: preserve the native
    // path until a valid developer publication exists. No RNG for bypasses.
    bool carries=false;for(unsigned c=0;c<2&&c<e.playMemPtr.size();++c){Guard g;auto*v=e.playMemPtr[c];auto&st=v->wr2State;carries|=(st.activeValid&&v->currentPlayMode==playModeBeatSlice)||st.pendingValid;}
    if(!published&&!carries)return;
    for(unsigned c=0;c<2&&c<e.playMemPtr.size();++c)voices_[c]=e.playMemPtr[c];
    main_=nativeSelection(i,n);
    const auto S=Sequencer::strFxConsts::FX_TYPE_SAMPLE_SLICE;
    if(fx&&(f1==S||f2==S)){main_.direct=true;main_.slice=main_.sliceCount?static_cast<uint8_t>((f1==S?v1:v2)%main_.sliceCount):0;}
    else if(published&&snapshot.state.sliceCount==main_.sliceCount&&n>=24&&n-24<main_.sliceCount&&snapshot.state.owner[n-24]==n-24){
        unsigned ticket;{Guard g;ticket=rng.percent();}
        main_.slice=static_cast<uint8_t>(selectTicket(snapshot.state,n-24,ticket));main_.randomized=true;
    }
    if(published)main_.topology=snapshot.topology;
    bool need,restarting;{Guard g;need=needMain(e,fx,f2,velocity);restarting=needTechnical(e,fx,f2,velocity);}
    if(need)mainPoints_=prepare(main_);
    if(restarting)prepareTechnical(i);
    lock();
    // An IRQ may have changed the envelope phase during preparation. Preparing
    // a previously unnecessary MAIN happens once, with BASEPRI restored first.
    if(!mainPoints_.valid&&needMain(e,fx,f2,velocity)){unlock();mainPoints_=prepare(main_);lock();}
    if(needTechnical(e,fx,f2,velocity)&&!hasTechnical_[0]){
        auto*v=voices_[0];
        if(v&&v->wr2State.activeValid&&v->wr2State.active.instrument==i&&live(v->wr2State.active)&&!same(v->wr2State.key,geometry(v->wr2State.active))){unlock();prepareTechnical(i);lock();}
    }
    // Refresh only matched native Beat Slice identity/points, never native modes.
    if(fx){bool found=false;for(auto*v:voices_)if(v){auto&st=v->wr2State;
        if(v->currentPlayMode==playModeBeatSlice&&st.activeValid&&st.active.instrument==i&&live(st.active)&&same(st.key,geometry(st.active))&&(!hasTechnical_[0]||!sameSelection(st.active,technical_[0])||v->pointsInSamples.start!=observedStart_||v->pointsInSamples.end!=observedEnd_)){
            for(unsigned c=0;c<2;++c)if(voices_[c]){technical_[c]=st.active;technicalPoints_[c].key=st.key;technicalPoints_[c].start=v->pointsInSamples.start;technicalPoints_[c].end=v->pointsInSamples.end;technicalPoints_[c].valid=true;hasTechnical_[c]=true;}
            found=true;break;
        }
    }if(!found&&(!hasTechnical_[0]||!live(technical_[0])||!same(technicalPoints_[0].key,geometry(technical_[0]))))for(unsigned c=0;c<2;++c)hasTechnical_[c]=false;}
    install();
}
Frame::Frame(playerEngine&e){
    if(e.playMemPtr.empty())return;
    if(auto*f=e.playMemPtr[0]->wr2State.frame){parent_=f;parentMode_=f->technicalMode_;f->technicalMode_=true;return;}
    Selection chosen;bool found=false,pendingChosen=false;uint8_t target;
    {Guard g;target=e.currentInstrument_idx;if(target>=kMaxInstruments||mtProject.instrument[target].playMode!=playModeBeatSlice)return;
        for(unsigned c=0;c<2&&c<e.playMemPtr.size();++c){voices_[c]=e.playMemPtr[c];auto&st=voices_[c]->wr2State;if(st.pendingValid&&st.pending.instrument==target&&live(st.pending)){chosen=st.pending;found=pendingChosen=true;}}
        if(!found)for(auto*v:voices_)if(v&&v->currentPlayMode==playModeBeatSlice&&v->wr2State.activeValid&&v->wr2State.active.instrument==target&&live(v->wr2State.active)){chosen=v->wr2State.active;found=true;break;}
        observedStart_=voices_[0]->pointsInSamples.start;observedEnd_=voices_[0]->pointsInSamples.end;
    }
    if(!found)return;
    main_=chosen;mainPoints_=prepare(main_);technicalMode_=true;
    lock();
    // If a musical trigger preempted preparation, use the now-committed native
    // pair. Do not reuse the old instrument or allocate/re-draw/reject a note.
    auto&st=voices_[0]->wr2State;
    bool stillPending=false;for(auto*v:voices_)if(v&&v->wr2State.pendingValid&&sameSelection(v->wr2State.pending,main_))stillPending=true;
    if(!(pendingChosen&&stillPending))for(auto*v:voices_)if(v&&v->wr2State.pendingValid&&v->wr2State.pending.instrument==e.currentInstrument_idx&&live(v->wr2State.pending)&&!sameSelection(v->wr2State.pending,main_)){
        main_=v->wr2State.pending;unlock();mainPoints_=prepare(main_);lock();pendingChosen=true;stillPending=true;break;
    }
    if(!(pendingChosen&&stillPending)&&voices_[0]->currentPlayMode==playModeBeatSlice&&st.activeValid&&st.active.instrument==e.currentInstrument_idx&&live(st.active)&&same(st.key,geometry(st.active))&&(!sameSelection(st.active,chosen)||voices_[0]->pointsInSamples.start!=observedStart_||voices_[0]->pointsInSamples.end!=observedEnd_)){
        main_=st.active;mainPoints_.key=st.key;mainPoints_.start=voices_[0]->pointsInSamples.start;mainPoints_.end=voices_[0]->pointsInSamples.end;mainPoints_.valid=true;
    }
    if(main_.instrument!=e.currentInstrument_idx){unlock();return;}
    install();
}
Frame::Frame(AudioPlayMemory&v,uint8_t i,int8_t){
    if(v.wr2State.frame||i>=kMaxInstruments||mtProject.instrument[i].playMode!=playModeBeatSlice)return;
    {Guard g;if(!v.wr2State.pendingValid)return;main_=v.wr2State.pending;v.wr2State.pendingValid=false;}
    if(main_.instrument!=i||!live(main_))return;
    mainPoints_=prepare(main_,v.samplePointsSource);voices_[0]=&v;lock();install();
}
Frame::Frame(playerEngine&e,Cancel){
    lock();
    bool needed=false;for(auto*v:e.playMemPtr)needed|=v->wr2State.activeValid||v->wr2State.pendingValid;
    if(!needed)unlock();
    // No frame pointers are installed: the native NoteOff cancels each sidecar
    // in its original envelope path while this short pair fence prevents IRQ
    // NoteOn from being cancelled on just the remaining channel.
}
Frame::~Frame(){if(parent_){parent_->technicalMode_=parentMode_;return;}if(locked_){for(unsigned c=0;c<2;++c)if(voices_[c])voices_[c]->wr2State.frame=previous_[c];unlock();}}
Selection Frame::selection(AudioPlayMemory&v,uint8_t i)const{
    if(technicalMode_)for(unsigned c=0;c<2;++c)if(voices_[c]==&v&&hasTechnical_[c]&&technical_[c].instrument==i)return technical_[c];
    return main_;
}
const Points*Frame::points(AudioPlayMemory&v,uint8_t i)const{
    if(technicalMode_)for(unsigned c=0;c<2;++c)if(voices_[c]==&v&&hasTechnical_[c]&&technical_[c].instrument==i)return &technicalPoints_[c];
    return main_.instrument==i?&mainPoints_:nullptr;
}
}
void AudioPlayMemory::wr2Queue(uint8_t i,int8_t){
    wr2native::Guard g;wr2State.pendingValid=false;
    if(wr2State.frame&&envelope.state!=envelopePhaseIdle&&envelope.state!=envelopePhaseDelay&&envelope.state!=envelopePhaseForced){auto s=wr2State.frame->selection(*this,i);if(s.instrument==i&&wr2native::Frame::live(s)){wr2State.pending=s;wr2State.pendingValid=true;}}
}
void AudioPlayMemory::wr2Cancel(){wr2native::Guard g;wr2State.pendingValid=false;}
void AudioPlayMemory::wr2ClearActive(){wr2native::Guard g;wr2State.activeValid=false;}
uint8_t AudioPlayMemory::wr2Slice(uint8_t i,uint8_t native)const{
    if(wr2State.frame){const auto s=wr2State.frame->selection(const_cast<AudioPlayMemory&>(*this),i);if(s.instrument==i&&wr2native::Frame::live(s))return s.slice;}
    return native;
}
bool AudioPlayMemory::wr2Prepared(uint8_t i,uint16_t a,uint16_t b){
    auto*f=wr2State.frame;if(!f)return false;const auto*p=f->points(*this,i);const auto s=f->selection(*this,i);
    if(!p||!p->valid||!wr2native::Frame::live(s)||!wr2native::Frame::same(p->key,wr2native::Frame::geometry(s))||p->key.start!=a||p->key.end!=b)return false;
    pointsInSamples.start=p->start;pointsInSamples.end=p->end;return true;
}
void AudioPlayMemory::wr2Commit(uint8_t i){
    wr2native::Guard g;wr2State.activeValid=false;
    if(auto*f=wr2State.frame){const auto s=f->selection(*this,i);if(s.instrument==i&&wr2native::Frame::live(s)){wr2State.active=s;wr2State.key=wr2native::Frame::geometry(s);wr2State.activeValid=true;}}
}
#endif
