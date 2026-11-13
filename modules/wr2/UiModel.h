#pragma once
#include "AudioAdapter.h"
namespace wr2 { namespace ui {
// Foreground-only editor. The atomic bank is the sole resident group authority.
class Editor {
public:
    explicit Editor(audio::PublishedBank& bank):bank_(bank){}
    void bind(unsigned instrument,unsigned count,bool page);
    void finish(){master_=kFree;}
    bool editing()const{return master_!=kFree;}
    unsigned master()const{return master_;}
    bool read(audio::Snapshot&)const;
    Result trig(unsigned slice);
    Result addMember(unsigned slice);
    Result removeSlice(unsigned slice);
    Result weight(unsigned slice,int delta);
    int weight(unsigned slice)const;
    bool invalidate(unsigned instrument);
    bool invalidateAll();
    static unsigned led(const InstrumentState&,unsigned slice,unsigned master,unsigned ceiling);
    static std::uint32_t marker(const InstrumentState&,unsigned slice,unsigned master,std::uint32_t stock);
private:
    audio::PublishedBank& bank_;
    unsigned instrument_=kMaxInstruments,count_=0,master_=kFree;
    std::uint32_t epoch_=0;
    bool page_=false;
    bool load(audio::Snapshot&,bool fresh)const;
    bool save(const audio::Snapshot&);
};
} }
