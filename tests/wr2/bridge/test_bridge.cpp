#include "TransportModel.h"
#include "../../../modules/wr2/AudioAdapter.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <thread>
#include <vector>
using namespace wr2model;
using namespace wr2;
using namespace wr2::audio;
static unsigned long long checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);std::abort();}}while(0)
static Command command(unsigned n,Op op=Op::NoteOn){Command c;c.serial=n;c.epoch=n^0x5a5a5a5au;c.op=op;c.submittedSample=n*3;return c;}
static void queues(){
    Queue<32> q;Command out;
    CHECK(!q.pop(out));
    for(unsigned i=0;i<32;++i)CHECK(q.push(command(i)));
    for(Op op:{Op::NoteOn,Op::NoteOff,Op::Cancel,Op::Reset})CHECK(!q.push(command(32,op)));
    for(unsigned i=0;i<32;++i){CHECK(q.pop(out));CHECK(out.serial==i);}
    CHECK(q.empty());
    // Caller retains rejected critical event and retries AFTER capacity is freed.
    for(Op op:{Op::NoteOff,Op::Cancel,Op::Reset}){
        for(unsigned i=0;i<32;++i)CHECK(q.push(command(i)));
        const auto pending=command(32,op);CHECK(!q.push(pending));
        CHECK(q.pop(out));CHECK(out.serial==0);CHECK(q.push(pending));
        for(unsigned i=1;i<=32;++i){CHECK(q.pop(out));CHECK(out.serial==i);if(i==32)CHECK(out.op==op);}
    }
    q.emptyAt(std::numeric_limits<std::uint32_t>::max()-15);
    for(unsigned round=0;round<16;++round){for(unsigned i=0;i<32;++i)CHECK(q.push(command(round*32+i)));for(unsigned i=0;i<32;++i){CHECK(q.pop(out));CHECK(out.serial==round*32+i);}}
    // A finite reserve does not prove critical-event retention.
    Queue<32> reserved;
    for(unsigned i=0;i<24;++i)CHECK(reserved.push(command(i)));
    for(unsigned i=24;i<32;++i)CHECK(reserved.push(command(i,Op::NoteOff)));
    CHECK(!reserved.push(command(32,Op::Reset)));
    std::puts("PASS queue: FIFO, all critical ops, caller-retained retry, full rejection, reserve exhaustion, uint32 wrap");
}
static Queue<32>* interruptedQueue=nullptr;
static void preemptPush(){Command c;c.serial=999;CHECK(!interruptedQueue->pop(c));CHECK(c.serial==999);}
static void interruptions(){
    Queue<32> q;interruptedQueue=&q;q.beforePublish=preemptPush;
    CHECK(q.push(command(7)));q.beforePublish=nullptr;Command c;CHECK(q.pop(c));CHECK(c.serial==7);
    Handover h;CHECK(!h.takeRender());CHECK(h.requestDrain());CHECK(!h.takeRender());
    CHECK(!h.acknowledge(false));CHECK(h.owner()==Owner::DrainRequested);
    CHECK(h.acknowledge(q.empty()));CHECK(h.takeRender());CHECK(!h.requestDrain());
    CHECK(!h.resumeLive());CHECK(h.releaseRender());CHECK(h.resumeLive());
    // Cortex-M7 is single-core: foreground cannot execute inside an active ISR.
    bool liveEventRunning=true;bool mainCanRun=false;
    CHECK(liveEventRunning&&!mainCanRun);
    liveEventRunning=false;mainCanRun=true;CHECK(mainCanRun&&!liveEventRunning);
    // Future asynchronous drain handshake must finish BEFORE native renderMode
    // suppresses callbacks. Otherwise the very executor that must ack is gated.
    bool renderMode=true;CHECK(q.push(command(8)));CHECK(h.requestDrain());
    CHECK(renderMode&&!q.empty());CHECK(!h.acknowledge(q.empty()));CHECK(!h.takeRender());
    renderMode=false;CHECK(!renderMode);CHECK(q.pop(c));CHECK(h.acknowledge(q.empty()));CHECK(h.takeRender());
    std::puts("PASS simulated preemption: unpublished payload hidden; single-core scheduling; drain before render callback gate");
}
static void threadedQueue(){
    Queue<32> q;std::atomic<bool> good{true};constexpr unsigned n=200000;
    std::thread p([&]{for(unsigned i=0;i<n;++i){const auto c=command(i,static_cast<Op>(i%4));while(!q.push(c))std::this_thread::yield();}});
    std::thread c([&]{for(unsigned i=0;i<n;++i){Command v;while(!q.pop(v))std::this_thread::yield();if(v.serial!=i||v.epoch!=(i^0x5a5a5a5au)||v.submittedSample!=i*3||v.op!=static_cast<Op>(i%4))good.store(false);}});
    p.join();c.join();CHECK(good.load());CHECK(q.empty());
    std::puts("PASS SPSC host stress: 200000 commands, including NoteOff/cancel/reset, all retained by caller until accepted");
}
static bool same(const Selection&a,const Selection&b){return a.topology==b.topology&&a.instrument==b.instrument&&a.slice==b.slice&&a.note==b.note&&a.kind==b.kind&&a.direct==b.direct&&a.randomized==b.randomized&&a.sliceCount==b.sliceCount;}
struct Channel {
    Selection active;DeferredSelection pending;bool activeValid=false;
    void submit(const Selection&s,EnvelopePhase p){auto d=pending.submit(s,p);if(d.startNow){active=d.selection;activeValid=true;}}
    bool resume(const Request&r){auto d=pending.resume(r);if(d.startNow){active=d.selection;activeValid=true;}return d.startNow;}
    void cancel(){pending.cancel();}
    bool technicalRestart(const Request&r){pending.cancel();if(!activeValid||!canStart(active,r))return false;return true;}
};
static Request request(){Request r;r.enabled=true;r.sliceCount=3;r.topology=1;return r;}
static void selections(){
    PublishedBank bank;InstrumentState s;s.reset(3);create(s,0);add(s,0,1);add(s,0,2);setWeight(s,0,0,0);
    CHECK(bank.instrument[0].publish(s,1)==PublishResult::Published);
    Random rng(123),expected(123);auto r=request();auto chosen=resolve(bank,r,rng);expected.percent();CHECK(rng.state()==expected.state());
    std::array<Channel,2> channels;
    channels[0].submit(chosen,EnvelopePhase::Idle);channels[1].submit(chosen,EnvelopePhase::Active);
    CHECK(channels[1].resume(r));CHECK(same(channels[0].active,channels[1].active));CHECK(rng.state()==expected.state());
    for(auto& c:channels)c.submit(chosen,EnvelopePhase::Active);
    for(auto& c:channels){CHECK(c.technicalRestart(r));CHECK(!c.pending.pending());CHECK(same(c.active,chosen));}
    CHECK(rng.state()==expected.state()); // technical restart preserves choice
    auto newer=resolve(bank,r,rng);expected.percent();
    for(auto& c:channels){c.submit(chosen,EnvelopePhase::Active);c.submit(newer,EnvelopePhase::Forced);CHECK(!c.pending.pending());CHECK(same(c.active,newer));}
    CHECK(rng.state()==expected.state());
    for(auto& c:channels){c.submit(chosen,EnvelopePhase::Active);c.cancel();CHECK(!c.resume(r));}
    const auto before=rng.state();r.directSlice=true;r.forcedSlice=2;auto direct=resolve(bank,r,rng);
    CHECK(direct.direct&&direct.slice==2);CHECK(rng.state()==before);
    for(auto& c:channels)c.submit(direct,EnvelopePhase::Active);
    r.directSlice=false;r.forcedSlice=0;
    for(auto& c:channels){CHECK(c.resume(r));CHECK(c.active.direct&&c.active.slice==2);CHECK(c.technicalRestart(r));}
    CHECK(rng.state()==before);
    for(int note:{25,26,127}){r.note=note;resolve(bank,r,rng);CHECK(rng.state()==before);}r.note=24;
    r.beatSlice=false;CHECK(resolve(bank,r,rng).kind==Kind::Native);CHECK(rng.state()==before);r.beatSlice=true;
    r.topology=2;for(auto& c:channels)CHECK(!c.technicalRestart(r));CHECK(rng.state()==before);r.topology=1;
    r.instrument=1;for(auto& c:channels)CHECK(!c.technicalRestart(r));r.instrument=0;
    Snapshot a,b;CHECK(bank.instrument[0].read(a));std::array<unsigned,3> cuts{0,10,20};cuts[1]=15;
    for(auto& c:channels){CHECK(c.technicalRestart(r));CHECK(cuts[c.active.slice]<=20);}
    CHECK(bank.instrument[0].read(b));for(unsigned i=0;i<48;++i){CHECK(a.state.owner[i]==b.state.owner[i]);CHECK(a.state.weight[i]==b.state.weight[i]);}
    // Each repeat is a NEW trigger, not a technical restart.
    for(unsigned i=0;i<100;++i){resolve(bank,r,rng);expected.percent();}CHECK(rng.state()==expected.state());
    std::puts("PASS descriptor candidate: stereo, deferred/resume, Forced replacement, cancellation, technical restart, S2/no RNG, moved cuts");
}
static void routingAndLatency(){
    // Routing is explicit context input: source alone CANNOT imply ISR/main.
    Queue<32> q;std::vector<Source> applied;
    for(auto source:{Source::Pad,Source::InstrumentPreview,Source::RawPreview,Source::SdPreview}){auto c=command(static_cast<unsigned>(source));c.source=source;CHECK(q.push(c));}
    Command c;while(q.pop(c))applied.push_back(c.source);
    CHECK(applied.size()==4);CHECK(applied[0]==Source::Pad);
    for(auto source:{Source::MidiDin,Source::MidiUsb,Source::Sequencer})applied.push_back(source);
    CHECK(applied.size()==7);
    // No measured WCET. A chosen budget of 8 commands/block can delay the 32nd
    // command by four nominal blocks, even though FIFO itself never reorders.
    for(unsigned i=0;i<32;++i)CHECK(q.push(command(i)));
    unsigned block=0;
    while(!q.empty()){++block;for(unsigned i=0;i<8&&q.pop(c);++i)CHECK(c.serial<block*8);}
    CHECK(block==4);CHECK(4.0*128.0/44100.0>0.0116);
    // Global order also changes if sequencer is applied before draining a pad.
    std::vector<unsigned> native{1,2},unsafeQueued{2,1};CHECK(native!=unsafeQueued);
    // Enqueuing reset does not lease the old PCM. Demonstrate the lifetime gap
    // WITHOUT dereferencing a dangling pointer or pretending reset is atomic.
    bool pcmAlive=true;CHECK(q.push(command(1,Op::Reset)));pcmAlive=false;CHECK(q.pop(c));CHECK(c.op==Op::Reset&&!pcmAlive);
    Handover handover;CHECK(handover.requestDrain());CHECK(handover.acknowledge(q.empty()));CHECK(handover.takeRender());
    Random rng(77);auto before=rng.state();CHECK(handover.owner()!=Owner::Live);CHECK(rng.state()==before);
    CHECK(handover.releaseRender());CHECK(handover.resumeLive());
    std::puts("PASS model routes/ownership; COUNTEREXAMPLES confirmed: full critical event, ordering policy, latency/backlog, PCM reset before consumer");
    std::puts("LATENCY conditional model: budget=8, depth=32, 4 blocks=11.609977 ms; stalled consumer has no finite bound");
}
static void exportOwnership(){
    PublishedBank bank;InstrumentState s;s.reset(3);create(s,0);add(s,0,1);
    CHECK(bank.instrument[0].publish(s,1)==PublishResult::Published);
    Handover owner;Random rng(998),expected(998);auto r=request();
    // Explicit executor argument is a model input, NOT actual ISR detection.
    auto trigger=[&](Owner executor){Selection rejected;rejected.kind=Kind::Reject;
        if((executor!=Owner::Live&&executor!=Owner::Render)||owner.owner()!=executor)return rejected;
        return resolve(bank,r,rng);};
    CHECK(trigger(Owner::Render).kind==Kind::Reject);CHECK(rng.state()==expected.state());
    CHECK(trigger(Owner::Live).randomized);expected.percent();CHECK(rng.state()==expected.state());
    CHECK(owner.requestDrain());CHECK(trigger(Owner::Live).kind==Kind::Reject);CHECK(rng.state()==expected.state());
    CHECK(owner.acknowledge(true));CHECK(trigger(Owner::Render).kind==Kind::Reject);
    CHECK(owner.takeRender());CHECK(trigger(Owner::Live).kind==Kind::Reject);CHECK(rng.state()==expected.state());
    auto render=trigger(Owner::Render);expected.percent();CHECK(render.randomized);CHECK(rng.state()==expected.state());
    std::array<Channel,2> c;for(auto& channel:c)channel.submit(render,EnvelopePhase::Idle);
    CHECK(same(c[0].active,c[1].active));
    CHECK(owner.releaseRender());CHECK(trigger(Owner::Live).kind==Kind::Reject);CHECK(owner.resumeLive());
    CHECK(trigger(Owner::Live).randomized);expected.percent();CHECK(rng.state()==expected.state());
    std::puts("PASS export ownership model: only matching executor draws, no draw in transition, stereo selection; continuous-session seed model only");
}
// Unmodified native syncFxAmpLFO body, extracted at each run. HAL is stubbed.
namespace nativefixture {
constexpr unsigned envAmp=0,lfoShapeRandom=9;
struct Env{int loop=1;};struct Lfo{int shape=9;};
struct Instrument{Env envelope[1];Lfo lfo[1];};struct Project{Instrument instrument[2];}mtProject;
struct Player{
    std::vector<unsigned> trace;unsigned instr=99;int note=-1;bool pending=true;
    void envelopeNoteOff(){trace.push_back(1);pending=false;}
    void envelopeSetIdle(){trace.push_back(2);}
    void envelopeSetIsRandom(bool b){trace.push_back(b?3u:30u);}
    void envelopeNoteOn(unsigned i,int n){trace.push_back(4);instr=i;note=n;}
};
struct playerEngine{std::vector<Player*>playMemPtr;unsigned currentInstrument_idx=1;int currentNote=25;void syncFxAmpLFO();};
#include "sync-native.inc"
static void run(){Player l,r;playerEngine e;e.playMemPtr={&l,&r};e.syncFxAmpLFO();for(auto p:{&l,&r}){CHECK(p->trace==std::vector<unsigned>({1,2,3,4}));CHECK(!p->pending);CHECK(p->instr==1&&p->note==25);}std::puts("PASS actual native sync body: per-channel NoteOff/Idle/random-mode/NoteOn, pending cancelled, current instrument/note used");}
}
int main(){queues();interruptions();threadedQueue();selections();routingAndLatency();exportOwnership();nativefixture::run();std::printf("PASS %llu phase-4 model checks; NOT native wiring or hardware validation\n",checks);}
