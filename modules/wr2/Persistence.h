#pragma once
#include "AudioAdapter.h"
#include <cstddef>
namespace wr2 { namespace persist {
constexpr unsigned kFileBytes=5024,kRecordBytes=104,kPath=320;
// W2PROJ1 layout, CE version 2. Legacy RE v1 identity is deliberately not guessed.
struct Slot {bool active=false;unsigned count=0,type=0,channels=1;std::uint32_t frames=0;char name[64]{};std::uint16_t cuts[48]{};};
class Model {
public:
 virtual bool slot(unsigned,Slot&)=0;
 virtual bool state(unsigned,InstrumentState&)=0;
 virtual bool ready()=0;
 virtual bool publish(unsigned,const InstrumentState&)=0;
 virtual void clear()=0;
 virtual bool erase(unsigned i){InstrumentState empty;empty.reset(0);return publish(i,empty);}
 virtual ~Model()=default;
};
enum class Probe {Missing,Present,Error};
class IO {
public:
 virtual Probe probe(const char*)=0;
 virtual bool read(const char*,std::uint8_t*,std::size_t)=0; // exact length, checked close
 virtual bool write(const char*,const std::uint8_t*,std::size_t)=0; // create/truncate, checked sync/close
 virtual bool move(const char*,const char*)=0; // must not overwrite existing target
 virtual bool remove(const char*)=0;
 virtual bool fileSize(const char*,std::uint32_t&){return false;}
 virtual bool directory(const char*){return false;}
 virtual bool digest(const char*,std::uint8_t*,std::uint32_t&){return false;}
 virtual ~IO()=default;
};
enum class Result {Idle,Saved,Loaded,Missing,Interrupted,Invalid,Binding,IOError,Changed,PublishError,PathError};
struct Storage {std::uint8_t bytes[kFileBytes];char folder[256];}; // foreground scratch, not a second live bank
std::uint32_t crc(const std::uint8_t*,std::size_t,std::uint32_t initial=0xffffffffu);
std::uint32_t identity(const Slot&);
bool encode(std::uint8_t*,Model&);
bool validateRoot(const std::uint8_t*,std::size_t);
 bool rootState(const std::uint8_t*,unsigned,InstrumentState&);
bool validate(const std::uint8_t*,std::size_t,Model&,std::uint64_t suppressed=0);
bool matches(const std::uint8_t*,Model&);
class Service {
public:
 Service(IO&io,Model&model,Storage&scratch):io_(io),model_(model),scratch_(scratch){}
 bool beginSave(const char*folder);
 bool commitSave(bool (*derived)(IO&,Model&,Storage&)=nullptr);
 void stageStartupOpen(){phase_=Phase::StartupOpening;}
 bool projectOpening()const{return phase_==Phase::Opening;}
 bool startupOpening()const{return phase_==Phase::StartupOpening;}
 bool saveReady()const{return phase_==Phase::Saving;}
 bool stageOpen(const char*folder);
 Result finishOpen(std::uint64_t suppressed=0); // clear, validate entire file, then publish before audio resumes
 void abort(){phase_=Phase::Idle;} // never remove guard after an interrupted save
 Result result()const{return result_;}
private:
 enum class Phase {Idle,Saving,Opening,StartupOpening};
 IO&io_;Model&model_;Storage&scratch_;Phase phase_=Phase::Idle;Result result_=Result::Idle;
 bool folder(const char*);
 void path(char*,const char*)const;
 bool fail(Result r){result_=r;phase_=Phase::Idle;return false;}
};
} }
