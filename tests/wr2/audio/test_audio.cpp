#include "../../../modules/wr2/AudioAdapter.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <thread>
#include <vector>
using namespace wr2;using namespace wr2::audio;
static std::atomic<unsigned long long> checks{0};
#define CHECK(x) do { checks.fetch_add(1,std::memory_order_relaxed); if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::abort();} } while(0)
static bool same(const InstrumentState& a,const InstrumentState& b){return std::memcmp(&a,&b,sizeof(a))==0;}
static bool same(const Selection& a,const Selection& b){return a.topology==b.topology&&a.instrument==b.instrument&&a.slice==b.slice&&a.note==b.note&&a.kind==b.kind&&a.direct==b.direct&&a.randomized==b.randomized&&a.sliceCount==b.sliceCount;}
static InstrumentState makeState(bool second=false){InstrumentState s;s.reset(3);create(s,0);add(s,0,1);add(s,0,2);setWeight(s,0,0,second?0:100);return s;}
static Request request(){Request r;r.enabled=true;r.sliceCount=3;r.topology=1;return r;}
static void(*hook)(bool,unsigned)=nullptr;
namespace wr2 {namespace audio {
void publicationTestStep(bool writing,unsigned step){if(hook)hook(writing,step);}
struct PublicationTestAccess {
    static void sequence(PublishedInstrument& p,std::uint32_t n){p.sequence_.store(n);}
    static void count(PublishedInstrument& p,std::uint32_t n){p.words_[24].store(n);}
};
}}
static PublishedInstrument* target=nullptr;
static unsigned injection=0;
static InstrumentState replacement;
static void duringRead(bool writing,unsigned step){if(writing||step!=injection)return;auto saved=hook;hook=nullptr;CHECK(target->publish(replacement,2)==PublishResult::Published);hook=saved;}
static void duringWrite(bool writing,unsigned step){if(!writing)return;auto saved=hook;hook=nullptr;Snapshot s;s.topology=999;bool ok=target->read(s);CHECK(ok==(step==27));if(ok){CHECK(s.topology==2);CHECK(same(s.state,replacement));}else CHECK(s.topology==999);hook=saved;}
static void publication(){
    PublishedInstrument p;Snapshot out;out.topology=999;CHECK(!p.read(out));CHECK(out.topology==999);
    auto a=makeState();replacement=makeState(true);target=&p;
    CHECK(p.publish(a,1)==PublishResult::Published);CHECK(p.read(out));CHECK(same(out.state,a));
    auto invalid=a;invalid.weight[0]=101;CHECK(p.publish(invalid,1)==PublishResult::Invalid);CHECK(p.publish(a,0)==PublishResult::Invalid);CHECK(p.read(out));CHECK(same(out.state,a));
    for(injection=0;injection<=26;++injection){CHECK(p.publish(a,1)==PublishResult::Published);out.state=a;out.topology=999;hook=duringRead;CHECK(!p.read(out));hook=nullptr;CHECK(out.topology==999);CHECK(same(out.state,a));CHECK(p.read(out));CHECK(same(out.state,replacement));CHECK(out.topology==2);}
    CHECK(p.publish(a,1)==PublishResult::Published);hook=duringWrite;CHECK(p.publish(replacement,2)==PublishResult::Published);hook=nullptr;
    PublicationTestAccess::count(p,255);CHECK(!p.read(out));
    CHECK(p.publish(a,1)==PublishResult::Published);
    PublicationTestAccess::sequence(p,0xfffffffeu);CHECK(p.publish(a,1)==PublishResult::Exhausted);CHECK(p.read(out));CHECK(same(out.state,a));
    std::puts("PASS publication: odd windows, 27 read interleavings, invalid data, unchanged output, no sequence wrap");
}
static void concurrentPublication(){
    PublishedInstrument p;auto a=makeState(),b=makeState(true);CHECK(p.publish(a,1)==PublishResult::Published);
    std::atomic<bool> go{false},done{false};std::atomic<unsigned> accepted{0},fallback{0};
    std::array<std::thread,3> readers;
    for(auto& reader:readers)reader=std::thread([&]{while(!go.load())std::this_thread::yield();for(unsigned i=0;i<100000;++i){Snapshot s;if(p.read(s)){CHECK(valid(s.state));CHECK((s.topology==1&&same(s.state,a))||(s.topology==2&&same(s.state,b)));accepted.fetch_add(1);}else fallback.fetch_add(1);}});
    std::thread writer([&]{while(!go.load())std::this_thread::yield();for(unsigned i=0;i<50000;++i)CHECK(p.publish((i&1)?a:b,(i&1)?1u:2u)==PublishResult::Published);done.store(true);});
    go.store(true);writer.join();for(auto& reader:readers)reader.join();CHECK(done.load());CHECK(accepted.load()+fallback.load()==300000);
    std::printf("PASS publication stress: 50000 writes, 300000 attempts, accepted=%u fallback=%u (schedule-dependent)\n",accepted.load(),fallback.load());
}
static void trigger(){
    PublishedBank bank;Random rng(0x12345678u);auto r=request();auto s=makeState(true);
    CHECK(bank.instrument[0].publish(s,1)==PublishResult::Published);
    auto before=rng.state();r.enabled=false;CHECK(resolve(bank,r,rng).kind==Kind::Native);CHECK(rng.state()==before);r.enabled=true;
    for(auto product:{Product::Original,Product::Mini}){r.product=product;CHECK(resolve(bank,r,rng).kind==Kind::Native);CHECK(rng.state()==before);}r.product=Product::Plus;
    r.directSlice=true;r.forcedSlice=2;auto direct=resolve(bank,r,rng);CHECK(direct.kind==Kind::Slice&&direct.slice==2&&direct.direct&&!direct.randomized);CHECK(rng.state()==before);
    r.forcedSlice=255;CHECK(resolve(bank,r,rng).slice==2);CHECK(rng.state()==before);r.directSlice=false;
    r.beatSlice=false;CHECK(resolve(bank,r,rng).kind==Kind::Native);CHECK(rng.state()==before);r.beatSlice=true;
    r.topology=2;auto missing=resolve(bank,r,rng);CHECK(missing.slice==0&&!missing.randomized);CHECK(rng.state()==before);r.topology=1;
    r.note=23;CHECK(resolve(bank,r,rng).slice==0);CHECK(rng.state()==before);
    r.note=127;CHECK(resolve(bank,r,rng).slice==2);CHECK(rng.state()==before);
    r.note=25;CHECK(resolve(bank,r,rng).slice==1);CHECK(rng.state()==before);r.note=24;
    Random expected(0x12345678u);expected.percent();const auto selected=resolve(bank,r,rng);
    CHECK(selected.randomized&&selected.slice>0);CHECK(rng.state()==expected.state());
    std::array<DeferredSelection,2> channels;
    for(auto& c:channels){auto d=c.submit(selected,EnvelopePhase::Idle);CHECK(d.startNow&&same(d.selection,selected));CHECK(canStart(d.selection,r));}
    CHECK(rng.state()==expected.state());
    // Delay, publication of new weights and altered FX state cannot re-resolve it.
    for(auto& c:channels)CHECK(!c.submit(selected,EnvelopePhase::Active).startNow);
    CHECK(bank.instrument[0].publish(makeState(),1)==PublishResult::Published);
    r.directSlice=true;r.forcedSlice=0;
    for(auto& c:channels){auto d=c.resume(r);CHECK(d.startNow&&same(d.selection,selected));CHECK(!c.resume(r).startNow);}
    CHECK(rng.state()==expected.state());r.directSlice=false;
    // Forced phase retrigger discards the previously queued descriptor.
    for(auto& c:channels)c.submit(selected,EnvelopePhase::Active);
    auto newer=resolve(bank,r,rng);expected.percent();CHECK(rng.state()==expected.state());
    for(auto& c:channels){auto d=c.submit(newer,EnvelopePhase::Forced);CHECK(d.startNow&&same(d.selection,newer));CHECK(!c.pending());CHECK(!c.resume(r).startNow);}
    // Direct S2 is frozen too, even if live S2 is subsequently cleared/changed.
    for(auto& c:channels)c.submit(direct,EnvelopePhase::Active);
    for(auto& c:channels){auto d=c.resume(r);CHECK(d.startNow&&d.selection.direct&&d.selection.slice==2);}
    // Different channel phases still share the same logical choice.
    CHECK(channels[0].submit(newer,EnvelopePhase::Forced).startNow);
    CHECK(!channels[1].submit(newer,EnvelopePhase::Active).startNow);
    auto late=channels[1].resume(r);CHECK(late.startNow&&same(late.selection,newer));
    // Repeated identical notes are new triggers, with one draw each, even at 100%.
    for(unsigned repeat=0;repeat<100;++repeat){auto t=resolve(bank,r,rng);expected.percent();CHECK(t.randomized);CHECK(rng.state()==expected.state());}
    // Cut positions are deliberately outside group identity: move them freely.
    Snapshot weightsBefore;CHECK(bank.instrument[0].read(weightsBefore));
    std::array<unsigned,3> cuts{0,1000,2000};for(auto& c:channels)c.submit(newer,EnvelopePhase::Active);
    cuts[1]=1500;cuts[2]=2400;
    for(auto& c:channels){auto d=c.resume(r);CHECK(d.startNow&&same(d.selection,newer));CHECK(cuts[d.selection.slice]<=2400);}
    Snapshot weightsAfter;CHECK(bank.instrument[0].read(weightsAfter));CHECK(same(weightsBefore.state,weightsAfter.state));
    // A genuine insertion/removal/permutation invalidates old pending identities.
    for(unsigned topology=2;topology<5;++topology){for(auto& c:channels)c.submit(newer,EnvelopePhase::Active);r.topology=topology;for(auto& c:channels)CHECK(!c.resume(r).startNow);}r.topology=1;
    for(auto& c:channels)c.submit(newer,EnvelopePhase::Active);r.sliceCount=2;for(auto& c:channels)CHECK(!c.resume(r).startNow);r.sliceCount=3;
    auto& channel=channels[0];channel.submit(newer,EnvelopePhase::Active);channel.cancel();CHECK(!channel.resume(r).startNow);
    before=rng.state();
    for(auto index:{48u,255u,std::numeric_limits<unsigned>::max()}){r.instrument=index;CHECK(resolve(bank,r,rng).kind==Kind::Reject);}r.instrument=0;
    for(auto count:{49u,255u,std::numeric_limits<unsigned>::max()}){r.sliceCount=count;CHECK(resolve(bank,r,rng).kind==Kind::Reject);}r.sliceCount=3;
    for(int note:{-129,128,std::numeric_limits<int>::min(),std::numeric_limits<int>::max()}){r.note=note;CHECK(resolve(bank,r,rng).kind==Kind::Reject);}r.note=24;
    r.topology=0;CHECK(resolve(bank,r,rng).kind==Kind::Reject);r.topology=1;
    r.sliceCount=0;auto empty=resolve(bank,r,rng);CHECK(empty.kind==Kind::WholeSample&&canStart(empty,r));r.directSlice=true;CHECK(resolve(bank,r,rng).kind==Kind::WholeSample);CHECK(rng.state()==before);
    // Sweep the native uint8 instrument/count domain and int8 note boundaries.
    r.directSlice=false;
    for(unsigned slot=0;slot<256;++slot)for(unsigned count=0;count<256;++count)for(int note:{-128,23,24,71,127}){
        r.instrument=slot;r.sliceCount=count;r.note=note;auto t=resolve(bank,r,rng);
        if(slot>=48||count>48)CHECK(t.kind==Kind::Reject);
        else if(!count)CHECK(t.kind==Kind::WholeSample);
        else {CHECK(t.kind==Kind::Slice);CHECK(t.slice<count&&t.slice<48);}
    }
    std::puts("PASS trigger: one draw/stereo, forced release, retrigger, S2 freeze, geometry retention, stale-event drop and bounds sweep");
}
static void budget(){
    struct DoubleBank {ProjectState edit,audio;};
    struct DoubleBankWithVersions {DoubleBank states;std::atomic<std::uint32_t> generation[48];};
    std::printf("BUDGET single-core-bank=%zu double-bank=%zu double-with-versions=%zu atomic-row=%zu atomic-bank=%zu snapshot=%zu selection=%zu deferred/channel=%zu deferred/8-stereo-voices=%zu\n",sizeof(ProjectState),sizeof(DoubleBank),sizeof(DoubleBankWithVersions),sizeof(PublishedInstrument),sizeof(PublishedBank),sizeof(Snapshot),sizeof(Selection),sizeof(DeferredSelection),sizeof(DeferredSelection)*16);
    CHECK(!kFirmwareIntegrationEnabled);CHECK(sizeof(PublishedBank)==5184);
}
int main(){publication();concurrentPublication();trigger();budget();std::printf("PASS: %llu phase-2 checks\n",checks.load());}
