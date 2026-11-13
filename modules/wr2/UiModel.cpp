#include "UiModel.h"
#include <limits>
namespace wr2 { namespace ui {
void Editor::bind(unsigned i,unsigned count,bool page){
    if(!page||i!=instrument_||count!=count_)finish();
    instrument_=i;count_=count;page_=page&&i<kMaxInstruments&&count>0&&count<=kMaxSlices;
    if(!page_){finish();return;}
    audio::Snapshot s;
    if(editing()&&(!read(s)||s.topology!=epoch_||master_>=count_||s.state.owner[master_]!=master_))finish();
}
bool Editor::load(audio::Snapshot&s,bool fresh)const{
    if(!page_)return false;
    if(bank_.instrument[instrument_].read(s)){
        if(s.state.sliceCount==count_)return true;
        // A cleared row is a free-slice view, without any automatic publication.
        if(!fresh&&s.state.sliceCount==0){s.state.reset(count_);return true;}
        if(!fresh||s.topology==std::numeric_limits<std::uint32_t>::max())return false;
        ++s.topology;
    }else s.topology=1;
    s.state.reset(count_);return true;
}
bool Editor::read(audio::Snapshot&s)const{return load(s,false);}
bool Editor::save(const audio::Snapshot&s){return bank_.instrument[instrument_].publish(s.state,s.topology)==audio::PublishResult::Published;}
Result Editor::trig(unsigned slice){
    if(editing()){finish();return Result::Applied;}
    audio::Snapshot s;if(slice>=count_||!load(s,true))return Result::Invalid;
    if(s.state.owner[slice]==kFree){if(create(s.state,slice)!=Result::Applied||!save(s))return Result::Invalid;}
    master_=s.state.owner[slice];epoch_=s.topology;return Result::Applied;
}
Result Editor::addMember(unsigned slice){
    audio::Snapshot s;if(!editing()||!read(s)||s.topology!=epoch_)return Result::Invalid;
    auto result=add(s.state,master_,slice);if(result==Result::Applied&&!save(s))return Result::Invalid;return result;
}
Result Editor::removeSlice(unsigned slice){
    audio::Snapshot s;if(!editing()||!read(s)||s.topology!=epoch_)return Result::Invalid;
    auto result=remove(s.state,master_,slice);
    if(result==Result::Applied){if(!save(s))return Result::Invalid;if(slice==master_)finish();}return result;
}
int Editor::weight(unsigned slice)const{
    audio::Snapshot s;if(!editing()||!read(s)||s.topology!=epoch_||slice>=count_||s.state.owner[slice]!=master_)return -1;
    return s.state.weight[slice];
}
Result Editor::weight(unsigned slice,int delta){
    audio::Snapshot s;const int current=weight(slice);if(current<0||!read(s))return Result::Invalid;
    // Encoder acceleration is preserved, and arithmetic cannot overflow.
    const int target=delta>100?100:delta< -100?0:current+delta;
    auto result=setWeight(s.state,master_,slice,target);if(result==Result::Applied&&!save(s))return Result::Invalid;return result;
}
bool Editor::invalidate(unsigned i){
    if(i>=kMaxInstruments)return false;
    if(i==instrument_)finish();
    audio::Snapshot s;if(!bank_.instrument[i].read(s))return true;
    if(s.topology==std::numeric_limits<std::uint32_t>::max()){bank_.instrument[i].retire();return false;}
    s.state.reset(0);
    if(bank_.instrument[i].publish(s.state,s.topology+1)==audio::PublishResult::Published)return true;
    bank_.instrument[i].retire();return false;
}
bool Editor::invalidateAll(){bool result=true;finish();for(unsigned i=0;i<kMaxInstruments;++i)result=invalidate(i)&&result;return result;}
static constexpr unsigned gamma[32]={0,1,2,4,6,10,13,18,22,28,33,39,46,53,61,69,78,86,96,106,116,126,138,149,161,173,186,199,212,226,240,255};
unsigned Editor::led(const InstrumentState&s,unsigned slice,unsigned master,unsigned ceiling){
    if(slice>=s.sliceCount||slice>=kMaxSlices)return 0;
    if(ceiling>31)ceiling=31;
    const auto owner=s.owner[slice];unsigned num,den;
    if(owner==kFree){num=2;den=51;}
    else if(master!=kFree&&owner!=master){num=11;den=85;}
    else if(owner==slice)return ceiling;
    else{num=32;den=85;}
    const unsigned target=(gamma[ceiling]*num+den/2)/den;
    unsigned best=0,error=256;
    for(unsigned level=0;level<=ceiling;++level){const unsigned diff=gamma[level]>target?gamma[level]-target:target-gamma[level];if(diff<error){error=diff;best=level;}}
    return best;
}
std::uint32_t Editor::marker(const InstrumentState&s,unsigned slice,unsigned master,std::uint32_t stock){
    if(slice>=s.sliceCount||slice>=kMaxSlices)return stock;
    const auto owner=s.owner[slice];auto rgb=owner==kFree?stock:owner==slice?0xff0000u:0xff8000u;
    if(master!=kFree&&owner!=master)rgb=(rgb>>1)&0x7f7f7fu;
    return rgb;
}
} }
