#include "Persistence.h"
#include <cstring>
#include <cstdio>
namespace wr2 { namespace persist {
static std::uint32_t get32(const std::uint8_t*p){return p[0]|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);}
static void put32(std::uint8_t*p,std::uint32_t n){for(unsigned i=0;i<4;++i)p[i]=static_cast<std::uint8_t>(n>>(i*8));}
std::uint32_t crc(const std::uint8_t*p,std::size_t n,std::uint32_t value){
 while(n--){value^=*p++;for(unsigned bit=0;bit<8;++bit)value=(value>>1)^((value&1)?0x82f63b78u:0);}
 return value;
}
std::uint32_t identity(const Slot&s){
 std::uint8_t data[8];data[0]=static_cast<std::uint8_t>(s.type);data[1]=static_cast<std::uint8_t>(s.channels);data[2]=static_cast<std::uint8_t>(s.count);data[3]=1;put32(data+4,s.frames);
 auto sum=crc(data,sizeof(data));unsigned start=0,end=0;
 for(;end<sizeof(s.name)&&s.name[end];++end)if(s.name[end]=='/'||s.name[end]=='\\')start=end+1;
 auto lower=[](unsigned c){return c>='A'&&c<='Z'?c+('a'-'A'):c;};
 if(end>=start+4&&s.name[end-4]=='.'&&lower(s.name[end-3])=='w'&&lower(s.name[end-2])=='a'&&lower(s.name[end-1])=='v')end-=4;
 for(unsigned i=start;i<end;++i){auto b=static_cast<std::uint8_t>(lower(static_cast<unsigned char>(s.name[i])));sum=crc(&b,1,sum);}
 const std::uint8_t zero=0;sum=crc(&zero,1,sum);
 for(unsigned i=0;i<s.count&&i<48;++i){data[0]=static_cast<std::uint8_t>(s.cuts[i]);data[1]=static_cast<std::uint8_t>(s.cuts[i]>>8);sum=crc(data,2,sum);}
 return sum^0xffffffffu;
}
static bool grouped(const InstrumentState&s){for(unsigned i=0;i<s.sliceCount;++i)if(s.owner[i]!=kFree)return true;return false;}
static bool record(std::uint8_t*out,unsigned i,Model&model){
 InstrumentState state;Slot slot;
 if(!model.state(i,state)||!valid(state)||!model.slot(i,slot))return false;
 std::memset(out,0,104);out[2]=static_cast<std::uint8_t>(i);std::memset(out+8,kFree,48);
 if(!grouped(state))return true;
 if(!slot.active||!slot.frames||!slot.count||slot.count>48||slot.count!=state.sliceCount||slot.channels<1||slot.channels>2)return false;
 out[0]=1;out[1]=state.sliceCount;put32(out+4,identity(slot));std::memcpy(out+8,state.owner,48);std::memcpy(out+56,state.weight,48);return true;
}
bool encode(std::uint8_t*out,Model&model){
 std::memset(out,0,32);std::memcpy(out,"W2PROJ1",7);out[8]=2;out[10]=48;put32(out+12,5024);put32(out+16,104);
 for(unsigned i=0;i<48;++i)if(!record(out+32+i*104,i,model))return false;
 auto sum=crc(out,28);sum=crc(out+32,4992,sum);put32(out+28,sum^0xffffffffu);return true;
}
static bool decode(const std::uint8_t*p,unsigned i,InstrumentState&s,Model*model){
 if(p[2]!=i||p[3]||p[0]>1)return false;
 if(!p[0]){
  if(p[1]||get32(p+4))return false;
  for(unsigned j=0;j<48;++j)if(p[8+j]!=kFree||p[56+j])return false;
  s.reset(0);return true;
 }
 if(!p[1]||p[1]>48)return false;
 s.reset(p[1]);std::memcpy(s.owner,p+8,48);std::memcpy(s.weight,p+56,48);
 if(!valid(s)||!grouped(s))return false;
 if(model){Slot slot;if(!model->slot(i,slot)||!slot.active||!slot.frames||slot.channels<1||slot.channels>2||slot.count!=s.sliceCount||identity(slot)!=get32(p+4))return false;}
 return true;
}
bool validateRoot(const std::uint8_t*p,std::size_t n){
 if(n!=5024||std::memcmp(p,"W2PROJ1",8)||p[8]!=2||p[9]||p[10]!=48||p[11]||get32(p+12)!=5024||get32(p+16)!=104)return false;
 for(unsigned i=20;i<28;++i)if(p[i])return false;
 auto sum=crc(p,28);sum=crc(p+32,4992,sum);if((sum^0xffffffffu)!=get32(p+28))return false;
 InstrumentState s;for(unsigned i=0;i<48;++i)if(!decode(p+32+i*104,i,s,nullptr))return false;
 return true;
}
bool rootState(const std::uint8_t*p,unsigned i,InstrumentState&s){return i<48&&decode(p+32+i*104,i,s,nullptr);}
bool validate(const std::uint8_t*p,std::size_t n,Model&model,std::uint64_t suppressed){
 if(!validateRoot(p,n))return false;
 InstrumentState s;for(unsigned i=0;i<48;++i)if(!(suppressed&(std::uint64_t(1)<<i))&&!decode(p+32+i*104,i,s,&model))return false;return true;
}
bool matches(const std::uint8_t*p,Model&model){
 std::uint8_t row[104];for(unsigned i=0;i<48;++i)if(!record(row,i,model)||std::memcmp(row,p+32+i*104,104))return false;return true;
}
bool Service::folder(const char*name){if(!name||!name[0]||std::strlen(name)>=sizeof(scratch_.folder))return fail(Result::PathError);std::memcpy(scratch_.folder,name,std::strlen(name)+1);return true;}
void Service::path(char*out,const char*leaf)const{std::snprintf(out,kPath,"%s/%s",scratch_.folder,leaf);}
bool Service::beginSave(const char*name){
 phase_=Phase::Idle;if(!folder(name))return false;
 if(!encode(scratch_.bytes,model_))return fail(Result::Binding);
 char guard[kPath];path(guard,"wr2_groups.wr2.pending");const std::uint8_t marker[8]={'W','2','P','E','N','D','2','\n'};std::uint8_t check[8];
 if(!io_.write(guard,marker,8)||!io_.read(guard,check,8)||std::memcmp(marker,check,8))return fail(Result::IOError);
 phase_=Phase::Saving;result_=Result::Idle;return true;
}
bool Service::commitSave(bool (*derived)(IO&,Model&,Storage&)){
 if(phase_!=Phase::Saving)return fail(Result::Interrupted);
 if(!matches(scratch_.bytes,model_))return fail(Result::Changed);
 char root[kPath],tmp[kPath],bak[kPath],guard[kPath];path(root,"wr2_groups.wr2");path(tmp,"wr2_groups.wr2.tmp");path(bak,"wr2_groups.wr2.bak");path(guard,"wr2_groups.wr2.pending");
 if(!io_.write(tmp,scratch_.bytes,5024)||!io_.read(tmp,scratch_.bytes,5024))return fail(Result::IOError);
 if(!validate(scratch_.bytes,5024,model_)||!matches(scratch_.bytes,model_))return fail(Result::Invalid);
 auto backup=io_.probe(bak);auto original=io_.probe(root);
 if(backup==Probe::Error||original==Probe::Error)return fail(Result::IOError);
 if(backup==Probe::Present&&!io_.remove(bak))return fail(Result::IOError);
 if(original==Probe::Present&&!io_.move(root,bak))return fail(Result::IOError);
 if(!io_.move(tmp,root)){
  if(original==Probe::Present)io_.move(bak,root); // best-effort recovery, guard still blocks authority
  return fail(Result::IOError);
 }
 if(!io_.read(root,scratch_.bytes,5024)||!validate(scratch_.bytes,5024,model_)||!matches(scratch_.bytes,model_))return fail(Result::IOError);
 if(derived&&!derived(io_,model_,scratch_))return fail(Result::IOError);
 if(!io_.remove(guard))return fail(Result::IOError);
 phase_=Phase::Idle;result_=Result::Saved;return true;
}
bool Service::stageOpen(const char*name){phase_=Phase::Idle;if(!folder(name))return false;phase_=Phase::Opening;return true;}
Result Service::finishOpen(std::uint64_t suppressed){
 if(phase_!=Phase::Opening)return result_;
 phase_=Phase::Idle;model_.clear();
 char name[kPath];path(name,"wr2_groups.wr2.pending");auto guard=io_.probe(name);
 path(name,"wr2_groups.wr2.tmp");auto tmp=io_.probe(name);
 if(guard==Probe::Error||tmp==Probe::Error)return result_=Result::IOError;
 if(guard==Probe::Present||tmp==Probe::Present)return result_=Result::Interrupted;
 path(name,"wr2_groups.wr2");auto root=io_.probe(name);
 if(root==Probe::Missing)return result_=Result::Missing;
 if(root==Probe::Error||!io_.read(name,scratch_.bytes,5024))return result_=Result::IOError;
 if(!validate(scratch_.bytes,5024,model_,suppressed))return result_=Result::Invalid;
 if(!model_.ready())return result_=Result::PublishError;
 InstrumentState state;
 for(unsigned i=0;i<48;++i){
  if(suppressed&(std::uint64_t(1)<<i))state.reset(0);
  else if(!decode(scratch_.bytes+32+i*104,i,state,&model_)){model_.clear();return result_=Result::Invalid;}
  if(!model_.publish(i,state)){model_.clear();return result_=Result::PublishError;}
 }
 return result_=Result::Loaded;
}
} }
