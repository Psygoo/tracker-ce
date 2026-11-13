#include "LocalSelection.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <algorithm>
#include <cassert>
#include <limits>
#include <tuple>
#include <vector>
using namespace wr2minimal;
static unsigned long long checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);std::abort();}}while(0)
struct Registers {
    static unsigned mask,primask,context;static bool nvic,pending;static std::function<void()>isr;
    static unsigned basepri(){return mask;}
    static void raise(unsigned v){auto previous=primask;primask=1;if(!mask||v<mask)mask=v;primask=previous;}
    static bool permitted(unsigned priority){return !primask&&(!mask||priority*16<mask)&&priority<context;}
    static void service(){if(nvic&&pending&&permitted(3)){pending=false;auto saved=context;context=3;isr();context=saved;}}
    static void restore(unsigned v){auto previous=primask;primask=1;mask=v;primask=previous;service();}
    static void barrier(){}
    static bool audioEnabled(){return nvic;}
    static void audioEnable(bool on){nvic=on;service();}
    static void reset(){mask=0;primask=0;context=16;nvic=true;pending=false;isr=[]{};}
};
unsigned Registers::mask=0,Registers::primask=0,Registers::context=16;
bool Registers::nvic=true,Registers::pending=false;std::function<void()>Registers::isr=[]{};
static bool same(const Selection&a,const Selection&b){return a.kind==b.kind&&a.instrument==b.instrument&&a.note==b.note&&a.slice==b.slice&&a.topology==b.topology&&a.direct==b.direct&&a.randomized==b.randomized&&a.sliceCount==b.sliceCount;}
static Request request(){Request r;r.enabled=true;r.topology=1;r.sliceCount=3;return r;}
static InstrumentState state(){InstrumentState s;s.reset(3);create(s,0);add(s,0,1);add(s,0,2);setWeight(s,0,0,60);return s;}
static void guards(){
    for(unsigned mask=0;mask<256;mask+=16)for(unsigned p:{0u,1u})for(bool n:{false,true}){
        Registers::reset();Registers::mask=mask;Registers::primask=p;Registers::nvic=n;
        {Guard<Registers> outer;auto effective=mask&&mask<0x30?mask:0x30;CHECK(Registers::mask==effective);
            {Guard<Registers> inner;CHECK(Registers::mask==effective);CHECK(!Registers::permitted(3));}
            CHECK(Registers::mask==effective);CHECK(Registers::primask==p);CHECK(Registers::nvic==n);
        }
        CHECK(Registers::mask==mask);CHECK(Registers::primask==p);CHECK(Registers::nvic==n);
    }
    Registers::reset();{Guard<Registers> g;CHECK(Registers::permitted(0));CHECK(Registers::permitted(1));CHECK(!Registers::permitted(3));CHECK(!Registers::permitted(6));Registers::audioEnable(true);CHECK(!Registers::permitted(3));}
    Registers::reset();Registers::nvic=false;{StereoFence<Registers>f;Registers::audioEnable(true);CHECK(!Registers::permitted(3));}CHECK(!Registers::nvic);CHECK(Registers::mask==0);
    std::puts("PASS mask model: 64 initial states, nesting, stronger BASEPRI, PRIMASK/NVIC retained; priority 0/1 remain runnable");
}
static void differential(){
    Registers::reset();PublishedBank bank;auto s=state();CHECK(bank.instrument[0].publish(s,1)==PublishResult::Published);CHECK(bank.instrument[47].publish(s,1)==PublishResult::Published);
    unsigned cases=0;
    for(auto prod:{Product::Plus,Product::Original,Product::Mini})for(bool enabled:{false,true})for(bool beat:{false,true})for(unsigned inst:{0u,47u,48u,255u})for(unsigned count=0;count<=50;++count)for(int note:{-129,-128,23,24,25,26,71,127,128})for(bool direct:{false,true})for(unsigned topo:{0u,1u,2u}){
        auto r=request();r.product=prod;r.enabled=enabled;r.beatSlice=beat;r.instrument=inst;r.sliceCount=count;r.note=note;r.directSlice=direct;r.forcedSlice=255;r.topology=topo;
        Random a(1234u+cases),b(1234u+cases);auto p=prepare(bank,r);auto chosen=choose<Registers>(p,a),original=resolve(bank,r,b);
        CHECK(same(chosen,original));CHECK(a.state()==b.state());++cases;
    }
    auto r=request();auto p=prepare(bank,r);Random rng(33),expected(33);
    // Preempt after snapshot, before RNG guard: inner event draws first.
    auto inner=choose<Registers>(p,rng);expected.percent();CHECK(inner.randomized&&rng.state()==expected.state());
    auto outer=choose<Registers>(p,rng);expected.percent();CHECK(outer.randomized&&rng.state()==expected.state());
    // Deferred ISR inside short mask only runs after the caller's single draw.
    Registers::isr=[&]{choose<Registers>(p,rng);};
    {Guard<Registers>g;Registers::pending=true;Registers::service();CHECK(Registers::pending);rng.percent();expected.percent();CHECK(rng.state()==expected.state());}
    expected.percent();CHECK(rng.state()==expected.state());CHECK(!Registers::pending);
    // Freeze snapshot: later publication must not change this event or need retry.
    s=state();setWeight(s,0,0,100);CHECK(bank.instrument[0].publish(s,1)==PublishResult::Published);p=prepare(bank,r);
    setWeight(s,0,0,0);CHECK(bank.instrument[0].publish(s,1)==PublishResult::Published);CHECK(choose<Registers>(p,rng).slice==0);
    Registers::reset();std::printf("PASS differential: %u requests match phase-2 result AND RNG; interleaved draws, immutable snapshot\n",cases);
}
static void descriptors(){
    Registers::reset();PublishedBank bank;CHECK(bank.instrument[0].publish(state(),1)==PublishResult::Published);Random rng(45);auto r=request();auto p=prepare(bank,r);auto selected=choose<Registers>(p,rng);const auto after=rng.state();
    std::array<ChannelState,2> c;
    {Guard<Registers>g;CHECK(c[0].submit(selected,EnvelopePhase::Idle));CHECK(!c[1].submit(selected,EnvelopePhase::Active));CHECK(c[1].resume(r));CHECK(same(c[0].active,c[1].active));}
    {Guard<Registers>g;for(auto&ch:c){ch.submit(selected,EnvelopePhase::Active);CHECK(ch.technical(r));CHECK(!ch.pendingValid);CHECK(same(ch.active,selected));}}
    CHECK(rng.state()==after);
    auto next=choose<Registers>(p,rng);
    {Guard<Registers>g;for(auto&ch:c){ch.submit(selected,EnvelopePhase::Active);CHECK(ch.submit(next,EnvelopePhase::Forced));CHECK(!ch.pendingValid);CHECK(same(ch.active,next));ch.submit(next,EnvelopePhase::Active);ch.cancel();CHECK(!ch.resume(r));}}
    const auto before=rng.state();r.directSlice=true;r.forcedSlice=2;auto direct=choose<Registers>(prepare(bank,r),rng);CHECK(direct.direct&&direct.slice==2);CHECK(rng.state()==before);
    {Guard<Registers>g;for(auto&ch:c)ch.submit(direct,EnvelopePhase::Active);}
    r.directSlice=false;r.forcedSlice=0;
    {Guard<Registers>g;for(auto&ch:c){CHECK(ch.resume(r));CHECK(ch.active.slice==2&&ch.active.direct);}}
    // Upcoming current* can be another instrument: technical identity must be active.
    auto upcoming=r;upcoming.instrument=1;
    {Guard<Registers>g;for(auto&ch:c){CHECK(!ch.technical(upcoming));CHECK(ch.technical(r));}}
    CHECK(rng.state()==before);
    for(const char*origin:{"sequencer","pad","DIN","USB","instrument preview","offline render"}){(void)origin;auto ev=choose<Registers>(p,rng);CHECK(ev.randomized);}
    for(unsigned bypass=0;bypass<5;++bypass){auto b=r;if(bypass==0)b.beatSlice=false;if(bypass==1)b.enabled=false;if(bypass==2)b.note=25;if(bypass==3)b.directSlice=true;if(bypass==4)b.sliceCount=0;auto prior=rng.state();choose<Registers>(prepare(bank,b),rng);CHECK(rng.state()==prior);}
    Snapshot a,b;CHECK(bank.instrument[0].read(a));unsigned cuts[3]={0,100,200};cuts[1]=150;
    {Guard<Registers>g;for(auto&ch:c)CHECK(ch.technical(r));}
    CHECK(cuts[1]==150);CHECK(bank.instrument[0].read(b));for(unsigned i=0;i<48;++i){CHECK(a.state.owner[i]==b.state.owner[i]);CHECK(a.state.weight[i]==b.state.weight[i]);}
    r.topology=2;{Guard<Registers>g;for(auto&ch:c){ch.submit(selected,EnvelopePhase::Active);CHECK(!ch.resume(r));}}
    std::puts("PASS local descriptors: stereo/stacked/Forced/cancel/S2, no RNG on technical restart, incoming vs active identity, render and cut movement");
}
namespace nativefixture {
#define AUDIO_CORE_RAM
#define AUDIO_CORE_FLASH
enum class IRQn_Type{DMA0_DMA16_IRQn};
void NVIC_EnableIRQ(IRQn_Type){Registers::audioEnable(true);}void NVIC_DisableIRQ(IRQn_Type){Registers::audioEnable(false);}
#include "irq-native.inc"
#define AudioNoInterrupts() disableAudioInterrupt()
#define AudioInterrupts() enableAudioInterrupt()
enum Phase{envelopePhaseIdle,envelopePhaseDelay,envelopePhaseActive,envelopePhaseForced,envelopePhaseSustain};
static const Selection*event=nullptr;static bool inject=false;static unsigned root=0;
struct AudioPlayMemory {
    struct {int pressedFlag=0,wtPhase=0,wtReleaseStartPhase=0,endKillReleaseFlag=0;Phase state=envelopePhaseIdle;bool passFlag=false;}envelope;
    struct{bool enable=false;std::uint8_t instr_idx=0;std::int8_t note=0;bool isPrev=false;}stackedPlay;
    unsigned channel=0,observed=255;
    void envelopeSwitchPhase(Phase p){envelope.state=p;}
    void envelopeSetIsRandom(bool){}
    void play(std::uint8_t,std::int8_t){AudioNoInterrupts();CHECK(event);observed=event->slice;
        if(channel==0&&root==1&&inject){inject=false;Registers::pending=true;}AudioInterrupts();}
    void envelopeNoteOn(std::uint8_t instr_idx,std::int8_t note);
};
#include "envelope-native.inc"
struct playerEngine{std::vector<AudioPlayMemory*>playMemPtr;
    void pair(std::uint8_t instr_idx,std::int8_t note){bool isAmpRandom=false;
#include "stereo-native.inc"
    }
};
static void run(){
    for(bool fenced:{false,true}){
        Registers::reset();AudioPlayMemory l,r;l.channel=0;r.channel=1;playerEngine e;e.playMemPtr={&l,&r};
        Selection a,b;a.slice=1;b.slice=2;root=1;event=&a;inject=true;
        Registers::isr=[&]{auto old=event;auto previous=root;event=&b;root=2;e.pair(0,24);root=previous;event=old;};
        if(fenced){StereoFence<Registers>f;e.pair(0,24);CHECK(l.observed==1&&r.observed==1);CHECK(Registers::pending);}
        else e.pair(0,24);
        if(fenced){CHECK(l.observed==2&&r.observed==2);CHECK(!Registers::pending);}
        else {CHECK(l.observed==2&&r.observed==1);std::puts("REPRO actual envelope/loop/helpers: RNG-only guard permits final L=B/R=A after nested IRQ");}
        CHECK(Registers::mask==0);CHECK(Registers::nvic);
    }
    std::puts("PASS native-body model: wider stereo fence prevents the interleaving; PCM/DSP cost NOT measured");
}
}
namespace zones {
using std::clamp;constexpr unsigned MAX_16BIT=65535;
struct AudioPlayMemory{static std::tuple<int16_t*,int16_t*,int16_t*>calculatePositionZoneAddresses(uint16_t,uint32_t,int16_t*);};
#include "zone-native.inc"
static void run(){for(unsigned n:{1000000u,16000000u}){
    std::vector<int16_t> pcm(n);auto [low,initial,high]=AudioPlayMemory::calculatePositionZoneAddresses(32768,n,pcm.data());
    CHECK(low>=pcm.data());CHECK(high<pcm.data()+n);CHECK(initial>=low&&initial<=high);CHECK(high-low>128);
    std::printf("ACTUAL snap zone: %u frames -> %td intervals (not 128 PCM frames); no elapsed-time claim\n",n,high-low);
}}
}
int main(){guards();differential();descriptors();nativefixture::run();zones::run();std::printf("PASS %llu checks; selection proof and native counterexample, NOT firmware integration\n",checks);}
