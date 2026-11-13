#include "../../../modules/wr2/Persistence.h"
#include "../../../modules/wr2/Companion.h"
#include "../../../modules/wr2/sha256.h"
#include <array>
#include <map>
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
using namespace wr2;using namespace wr2::persist;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::abort();}}while(0)
struct MemoryModel:Model {
 std::array<Slot,48> slots;std::array<InstrumentState,48> rows;unsigned writes=0;int reject=-1;bool readyValue=true;
 MemoryModel(){for(unsigned i=0;i<48;++i){auto&s=slots[i];s.active=true;s.count=48;s.channels=i%2+1;s.type=0;s.frames=100000+i;std::snprintf(s.name,64,"sample-%u.wav",i);for(unsigned j=0;j<48;++j)s.cuts[j]=static_cast<uint16_t>(j*1000);}}
 void populate(){for(unsigned i=0;i<48;++i){auto&s=rows[i];s.reset(48);create(s,i);add(s,i,(i+1)%48);add(s,i,(i+2)%48);setWeight(s,i,i,static_cast<int>(i));create(s,(i+3)%48);}}
 bool slot(unsigned i,Slot&out)override{out=slots[i];return true;}
 bool state(unsigned i,InstrumentState&out)override{out=rows[i];return true;}
 bool ready()override{return readyValue;}
 bool publish(unsigned i,const InstrumentState&s)override{if(static_cast<int>(i)==reject)return false;rows[i]=s;++writes;return true;}
 void clear()override{for(auto&s:rows)s.reset(0);}
 bool erase(unsigned i)override{if(i>=48)return false;rows[i].reset(0);return true;}
 bool empty()const{for(const auto&s:rows)for(unsigned i=0;i<s.sliceCount;++i)if(s.owner[i]!=kFree)return false;return true;}
};
struct Crash{};
struct MemoryIO:IO {
 std::map<std::string,std::vector<uint8_t>> files;unsigned operations=0;int fault=-1,cut=-1;bool after=false;
 bool step(){++operations;if(static_cast<int>(operations)==cut&&!after)throw Crash{};return static_cast<int>(operations)!=fault;}
 void done(){if(static_cast<int>(operations)==cut&&after)throw Crash{};}
 Probe probe(const char*p)override{if(!step())return Probe::Error;auto r=files.count(p)?Probe::Present:Probe::Missing;done();return r;}
 bool read(const char*p,uint8_t*out,std::size_t n)override{if(!step())return false;auto i=files.find(p);if(i==files.end()||i->second.size()!=n)return false;std::memcpy(out,i->second.data(),n);done();return true;}
 bool write(const char*p,const uint8_t*d,std::size_t n)override{if(!step()){files[p]=std::vector<uint8_t>(d,d+n/2);return false;}files[p]=std::vector<uint8_t>(d,d+n);done();return true;}
 bool move(const char*a,const char*b)override{if(!step())return false;auto i=files.find(a);if(i==files.end()||files.count(b))return false;files[b]=i->second;files.erase(i);done();return true;}
 bool remove(const char*p)override{if(!step())return false;if(!files.erase(p))return false;done();return true;}
 bool fileSize(const char*p,uint32_t&size)override{if(!step())return false;auto it=files.find(p);if(it==files.end())return false;size=static_cast<uint32_t>(it->second.size());done();return true;}
 bool directory(const char*)override{if(!step())return false;done();return true;}
 bool digest(const char*p,uint8_t*out,uint32_t&size)override{if(!step())return false;auto it=files.find(p);if(it==files.end())return false;size=static_cast<uint32_t>(it->second.size());wr2_sha256_t h;wr2_sha256_init(&h);wr2_sha256_update(&h,it->second.data(),size);wr2_sha256_final(&h,out);done();return true;}
 void reset(){operations=0;fault=cut=-1;after=false;}
};
static void repair(std::vector<uint8_t>&v){auto value=crc(v.data(),28);value=crc(v.data()+32,4992,value)^0xffffffffu;for(unsigned i=0;i<4;++i)v[28+i]=static_cast<uint8_t>(value>>(8*i));}
static bool equalRows(const MemoryModel&a,const MemoryModel&b){for(unsigned i=0;i<48;++i)if(std::memcmp(&a.rows[i],&b.rows[i],sizeof(InstrumentState)))return false;return true;}
static void save(MemoryIO&io,MemoryModel&model,const char*folder){Storage scratch{};Service service(io,model,scratch);CHECK(service.beginSave(folder));CHECK(service.commitSave());}
static wr2::persist::Result load(MemoryIO&io,MemoryModel&model,const char*folder){Storage scratch{};Service service(io,model,scratch);CHECK(service.stageOpen(folder));return service.finishOpen();}
static void codecTests(){
 CHECK((crc(reinterpret_cast<const uint8_t*>("123456789"),9)^0xffffffffu)==0xe3069283u);
 MemoryModel model;model.populate();Storage scratch{};CHECK(encode(scratch.bytes,model));CHECK(validate(scratch.bytes,5024,model));
 CHECK(!validate(scratch.bytes,5023,model)&&!validate(scratch.bytes,5025,model));
 for(unsigned i=0;i<48;++i){CHECK(scratch.bytes[32+i*104]==1);CHECK(scratch.bytes[32+i*104+2]==i);}
 for(unsigned i=0;i<5024;i+=7){auto old=scratch.bytes[i];scratch.bytes[i]^=static_cast<uint8_t>(1u<<(i%8));CHECK(!validate(scratch.bytes,5024,model));scratch.bytes[i]=old;}
 std::vector<uint8_t> original(scratch.bytes,scratch.bytes+5024);
 for(auto pos:{0u,7u,8u,9u,10u,11u,12u,16u,20u,27u,32u,33u,34u,35u,36u,40u,88u,5023u}){
  auto v=original;v[pos]=static_cast<uint8_t>(v[pos]^0xff);repair(v);CHECK(!validate(v.data(),v.size(),model));
 }
 auto legacy=original;legacy[8]=1;repair(legacy);CHECK(!validate(legacy.data(),5024,model));
 for(unsigned i=0;i<48;++i){++model.slots[i].frames;CHECK(!validate(original.data(),5024,model));--model.slots[i].frames;}
 ++model.slots[0].cuts[1];CHECK(!validate(original.data(),5024,model));--model.slots[0].cuts[1];
 auto id=identity(model.slots[0]);const char upper[]="SAMPLE-0.WAV";std::memcpy(model.slots[0].name,upper,sizeof(upper));CHECK(identity(model.slots[0])==id);
 std::puts("PASS codec: v2, CRC32C, reserved/length/slot/owners/weights, all 48 instruments, zero weights and native binding");
}
static void transactions(){
 MemoryModel a;a.populate();MemoryModel expected=a;MemoryIO io;io.files["A/native.ptp"]={1,2,3};save(io,a,"A");CHECK(io.files["A/native.ptp"]==std::vector<uint8_t>({1,2,3}));
 MemoryModel restored;CHECK(load(io,restored,"A")==wr2::persist::Result::Loaded&&equalRows(restored,expected));
 auto original=io.files["A/wr2_groups.wr2"];setWeight(a.rows[0],0,0,70);save(io,a,"B");CHECK(io.files["A/wr2_groups.wr2"]==original);
 CHECK(load(io,restored,"B")==wr2::persist::Result::Loaded&&equalRows(restored,a));CHECK(load(io,restored,"A")==wr2::persist::Result::Loaded&&equalRows(restored,expected));
 CHECK(load(io,restored,"legacy")==wr2::persist::Result::Missing&&restored.empty());
 io.files["A/wr2_groups.wr2"].push_back(0);CHECK(load(io,restored,"A")==wr2::persist::Result::IOError&&restored.empty());io.files["A/wr2_groups.wr2"]=original;
 io.files["A/wr2_groups.wr2"][400]^=1;CHECK(load(io,restored,"A")==wr2::persist::Result::Invalid&&restored.empty());io.files["A/wr2_groups.wr2"]=original;
 io.files["A/wr2_groups.wr2.pending"]={0};CHECK(load(io,restored,"A")==wr2::persist::Result::Interrupted&&restored.empty());io.files.erase("A/wr2_groups.wr2.pending");
 io.files["A/wr2_groups.wr2.tmp"]={0};CHECK(load(io,restored,"A")==wr2::persist::Result::Interrupted);io.files.erase("A/wr2_groups.wr2.tmp");
 ++restored.slots[2].frames;CHECK(load(io,restored,"A")==wr2::persist::Result::Invalid&&restored.empty());--restored.slots[2].frames;
 restored.readyValue=false;CHECK(load(io,restored,"A")==wr2::persist::Result::PublishError&&restored.empty());restored.readyValue=true;
 restored.reject=47;CHECK(load(io,restored,"A")==wr2::persist::Result::PublishError&&restored.empty());restored.reject=-1;
 // Changed UI or geometry while native Save is copying must never commit stale groups.
 Storage scratch{};Service svc(io,a,scratch);CHECK(svc.beginSave("B"));setWeight(a.rows[1],1,1,90);CHECK(!svc.commitSave()&&svc.result()==wr2::persist::Result::Changed);CHECK(!a.empty());
 CHECK(load(io,restored,"B")==wr2::persist::Result::Interrupted);save(io,a,"B");CHECK(load(io,restored,"B")==wr2::persist::Result::Loaded&&equalRows(a,restored));
 CHECK(!svc.beginSave(std::string(256,'x').c_str()));
 std::puts("PASS Save/Open/Save As: source intact, missing/corrupt/interrupted/context-change, all-or-none publication and explicit retry");
}
static void failureMatrix(bool previousBackup){
 MemoryModel old;old.populate();MemoryIO base;save(base,old,"P");base.files["P/native.ptp"]={4,5,6};
 if(previousBackup)base.files["P/wr2_groups.wr2.bak"]=base.files["P/wr2_groups.wr2"];
 MemoryModel changed=old;setWeight(changed.rows[0],0,0,72);
 MemoryIO count=base;count.reset();Storage scratch{};Service svc(count,changed,scratch);CHECK(svc.beginSave("P"));CHECK(svc.commitSave());const unsigned total=count.operations;
 for(unsigned n=1;n<=total;++n){
  MemoryIO io=base;io.reset();io.fault=static_cast<int>(n);MemoryModel model=changed;Storage buffer{};Service operation(io,model,buffer);
  bool started=operation.beginSave("P");bool completed=started&&operation.commitSave();CHECK(!completed);CHECK(equalRows(model,changed));CHECK(io.files["P/native.ptp"]==base.files["P/native.ptp"]);
  io.reset();MemoryModel destination;auto result=load(io,destination,"P");if(result==wr2::persist::Result::Loaded)CHECK(equalRows(destination,old)||equalRows(destination,changed));else CHECK(destination.empty());
  if(started&&result==wr2::persist::Result::Loaded)CHECK(equalRows(destination,changed));
 }
 for(unsigned n=1;n<=total;++n)for(bool after:{false,true}){
  MemoryIO io=base;io.reset();io.cut=static_cast<int>(n);io.after=after;MemoryModel model=changed;Storage buffer{};Service operation(io,model,buffer);bool nativeStarted=false;
  try{if(operation.beginSave("P")){nativeStarted=true;operation.commitSave();}}catch(const Crash&){}
  io.reset();MemoryModel destination;auto result=load(io,destination,"P");
  if(result==wr2::persist::Result::Loaded){CHECK(equalRows(destination,old)||equalRows(destination,changed));if(nativeStarted)CHECK(equalRows(destination,changed));}else CHECK(destination.empty());
  CHECK(io.files["P/native.ptp"]==base.files["P/native.ptp"]);
 }
 // Every read/stat failure on Open is fail-closed. A backup is never authority.
 MemoryIO countLoad=base;countLoad.reset();MemoryModel target;load(countLoad,target,"P");
 for(unsigned n=1;n<=countLoad.operations;++n){auto io=base;io.reset();io.fault=static_cast<int>(n);MemoryModel targetModel;targetModel.populate();CHECK(load(io,targetModel,"P")!=wr2::persist::Result::Loaded&&targetModel.empty());}
 auto io=base;io.files["P/wr2_groups.wr2.bak"]=io.files["P/wr2_groups.wr2"];io.files.erase("P/wr2_groups.wr2");CHECK(load(io,target,"P")==wr2::persist::Result::Missing&&target.empty());
 std::printf("PASS failure matrix: %u save IO boundaries, errors and before/after power cuts, open errors, native-file sentinel untouched\n",total);
}
#include "lifecycle.inc"
#include "portable.inc"
#include "native.inc"
int main(){companionTests();portableTests();nativeIntegrationTests();codecTests();transactions();failureMatrix(false);failureMatrix(true);std::printf("PASS persistence: %u checks; simulated IO/power cuts, no hardware validation\n",checks);}
