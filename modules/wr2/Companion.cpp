#include "Companion.h"
#include "sha256.h"
#include <cstring>
#include <cstdio>
#include <initializer_list>
namespace wr2 { namespace persist {
namespace {
std::uint32_t u32(const std::uint8_t*p){return p[0]|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);}
void le32(std::uint8_t*p,std::uint32_t n){for(unsigned i=0;i<4;++i)p[i]=static_cast<std::uint8_t>(n>>(8*i));}
void sha(const std::uint8_t*p,std::size_t n,std::uint8_t*out){wr2_sha256_t s;wr2_sha256_init(&s);wr2_sha256_update(&s,p,n);wr2_sha256_final(&s,out);}
bool format(char*out,const char*folder,unsigned i,char side,const char*suffix=""){
 const int n=std::snprintf(out,kPath,"%s/wr2_companions/instrument_%02u.%c.w2i%s",folder,i+1,side,suffix);return n>0&&n<int(kPath);
}
const char*leaf(const char*p){auto q=std::strrchr(p,'/');return q?q+1:p;}
bool clean(IO&io,const char*p){auto found=io.probe(p);return found==Probe::Missing||(found==Probe::Present&&io.remove(p));}
bool portableQuiet(IO&io,const char*pti){
 // Native project exports are not importable mid-Save. No root file is read.
 char path[kPath];const auto n=std::strlen(pti);if(n>=sizeof(path))return false;
 std::memcpy(path,pti,n+1);auto last=std::strrchr(path,'/');if(!last)return true;*last=0;
 last=std::strrchr(path,'/');if(!last||std::strcmp(last+1,"instruments"))return true;*last=0;
 const auto root=std::strlen(path);
 for(const char*suffix:{"/wr2_groups.wr2.pending","/wr2_groups.wr2.tmp"}){
  const auto len=std::strlen(suffix);if(root+len>=sizeof(path))return false;
  std::memcpy(path+root,suffix,len+1);if(io.probe(path)!=Probe::Missing)return false;
 }
 return true;
}
bool portableName(char*out,const char*pti,const char*suffix=""){
 const int n=std::snprintf(out,kPath,"%s.w2i%s",pti,suffix);return n>0&&n<int(kPath);
}
bool writeChecked(IO&io,const char*path,const char*tmp,const std::uint8_t*c,std::uint8_t*check){
 return io.write(tmp,c,kCompanionBytes)&&io.read(tmp,check,kCompanionBytes)&&!std::memcmp(c,check,kCompanionBytes)&&clean(io,path)&&io.move(tmp,path)&&io.read(path,check,kCompanionBytes)&&!std::memcmp(c,check,kCompanionBytes);
}
bool checkedPortable(const std::uint8_t*c,const std::uint8_t*hash,std::uint32_t size,const Slot&slot,InstrumentState&state){
 if(std::memcmp(c,"W2INST1",8)||c[8]!=2||c[9]||c[10]>=48||c[11]||u32(c+12)!=kCompanionBytes||u32(c+284)!=(crc(c,284)^0xffffffffu))return false;
 // Original project digest/name/slot are provenance in this standalone mode.
 const auto end=static_cast<const std::uint8_t*>(std::memchr(c+84,0,88));if(!end||end==c+84)return false;
 for(auto q=end;q<c+176;++q)if(*q)return false;
 for(unsigned i=280;i<284;++i)if(c[i])return false;
 const auto record=c+176;
 if(record[0]!=1||!record[1]||record[1]>48||record[2]!=c[10]||record[3])return false;
 state.reset(record[1]);std::memcpy(state.owner,record+8,48);std::memcpy(state.weight,record+56,48);
 if(!valid(state))return false;
 bool group=false;for(unsigned i=0;i<state.sliceCount;++i)if(state.owner[i]!=kFree)group=true;
 return group&&slot.active&&slot.frames&&slot.channels>=1&&slot.channels<=2&&slot.count==state.sliceCount&&identity(slot)==u32(record+4)&&size==u32(c+16)&&!std::memcmp(hash,c+52,32);
}
bool rootReady(IO&io,const char*folder,std::uint8_t*out){
 char p[kPath];
 for(const char*suffix:{".pending",".tmp"}){
  const int n=std::snprintf(p,kPath,"%s/wr2_groups.wr2%s",folder,suffix);if(n<=0||n>=int(kPath)||io.probe(p)!=Probe::Missing)return false;
 }
 const int n=std::snprintf(p,kPath,"%s/wr2_groups.wr2",folder);return n>0&&n<int(kPath)&&io.read(p,out,kFileBytes)&&validateRoot(out,kFileBytes);
}
bool encodeCompanion(std::uint8_t*c,IO&io,const char*pti,const std::uint8_t*root,unsigned source){
 const auto name=leaf(pti);const auto len=std::strlen(name);if(!len||len>=88)return false;
 std::memset(c,0,kCompanionBytes);std::memcpy(c,"W2INST1",7);c[8]=2;c[10]=static_cast<std::uint8_t>(source);le32(c+12,kCompanionBytes);
 std::uint32_t size=0;if(!io.digest(pti,c+52,size)||!size)return false;le32(c+16,size);sha(root,kFileBytes,c+20);std::memcpy(c+84,name,len);
 std::memcpy(c+176,root+32+source*kRecordBytes,kRecordBytes);le32(c+284,crc(c,284)^0xffffffffu);return true;
}
bool checked(const std::uint8_t*c,const std::uint8_t*ptiHash,std::uint32_t size,const char*pti,const std::uint8_t*root,unsigned source){
 if(std::memcmp(c,"W2INST1",8)||c[8]!=2||c[9]||c[10]!=source||c[11]||u32(c+12)!=kCompanionBytes||u32(c+284)!=(crc(c,284)^0xffffffffu))return false;
 for(unsigned i=172;i<176;++i)if(c[i])return false;
 for(unsigned i=280;i<284;++i)if(c[i])return false;
 std::uint8_t hash[32];sha(root,kFileBytes,hash);if(std::memcmp(hash,c+20,32)||std::memcmp(c+176,root+32+source*kRecordBytes,kRecordBytes))return false;
 const auto name=leaf(pti);const auto n=std::strlen(name);if(n>=88||std::memcmp(c+84,name,n)||c[84+n])return false;
 for(unsigned i=static_cast<unsigned>(n)+1;i<88;++i)if(c[84+i])return false;
 return size==u32(c+16)&&!std::memcmp(ptiHash,c+52,32);
}
}
std::uint64_t readReplacements(IO&io){
 const char*path="Workspace/wr2_replaced.mask";constexpr std::uint64_t all=(std::uint64_t(1)<<48)-1;auto found=io.probe(path);if(found==Probe::Missing)return 0;
 std::uint8_t data[32];if(found==Probe::Error||!io.read(path,data,32)||std::memcmp(data,"W2MASK2",8)||u32(data+28)!=(crc(data,28)^0xffffffffu))return all;
 for(unsigned i=14;i<28;++i)if(data[i])return all;
 std::uint64_t mask=0;for(unsigned i=0;i<6;++i)mask|=std::uint64_t(data[8+i])<<(8*i);return mask;
}
bool markReplacement(IO&io,unsigned i){
 if(i>=48)return false;
 const auto bits=readReplacements(io)|(std::uint64_t(1)<<i);std::uint8_t data[32]{},check[32];std::memcpy(data,"W2MASK2",7);
 for(unsigned j=0;j<6;++j)data[8+j]=static_cast<std::uint8_t>(bits>>(8*j));
 le32(data+28,crc(data,28)^0xffffffffu);
 return io.write("Workspace/wr2_replaced.mask",data,32)&&io.read("Workspace/wr2_replaced.mask",check,32)&&!std::memcmp(data,check,32);
}
bool clearReplacements(IO&io){return clean(io,"Workspace/wr2_replaced.mask");}
bool exportProject(IO&io,Model&model,Storage&scratch){
 if(!validate(scratch.bytes,kFileBytes,model))return false;
 bool made=false;char path[kPath],tmp[kPath],portable[kPath];std::uint8_t c[kCompanionBytes],check[kCompanionBytes];
 for(unsigned i=0;i<48;++i){
  const bool group=scratch.bytes[32+i*kRecordBytes]!=0;
  Slot slot;if(!model.slot(i,slot))return false;
  if(!slot.active)continue;
  int n=std::snprintf(path,kPath,"%s/instruments/%u %s.pti",scratch.folder,i+1,slot.name);if(n<=0||n>=int(kPath))return false;
  if(!group){
   // A successful Save with no groups must not leave a matching old portable.
   for(unsigned variant=0;variant<(i<9?2u:1u);++variant){
    n=std::snprintf(path,kPath,variant?"%s/instruments/%02u %s.pti":"%s/instruments/%u %s.pti",scratch.folder,i+1,slot.name);if(n<=0||n>=int(kPath))return false;
    if(!portableName(portable,path)||!portableName(tmp,path,".tmp")||!clean(io,portable)||!clean(io,tmp))return false;
   }
   continue;
  }
  auto found=io.probe(path);if(found==Probe::Error)return false;
  if(found==Probe::Missing){n=std::snprintf(path,kPath,"%s/instruments/%02u %s.pti",scratch.folder,i+1,slot.name);if(n<=0||n>=int(kPath)||io.probe(path)!=Probe::Present)return false;}
  if(!portableName(portable,path)||!encodeCompanion(c,io,path,scratch.bytes,i))return false;
  if(!made){n=std::snprintf(path,kPath,"%s/wr2_companions",scratch.folder);if(n<=0||n>=int(kPath)||!io.directory(path))return false;made=true;}
  for(char side:{'a','b'}){
   if(!format(path,scratch.folder,i,side)||!format(tmp,scratch.folder,i,side,".tmp")||!writeChecked(io,path,tmp,c,check))return false;
  }
  n=std::snprintf(tmp,kPath,"%s.tmp",portable);if(n<=0||n>=int(kPath)||!writeChecked(io,portable,tmp,c,check))return false;
 }
 return true;
}
bool importInstrument(IO&io,Model&model,Storage&scratch,unsigned destination,const char*pti){
 if(destination>=48)return false;
 if(!model.erase(destination))return false;
 if(!pti||std::strlen(pti)>=kPath||!portableQuiet(io,pti))return false;
 {
  char path[kPath];if(!portableName(path,pti,".tmp")||io.probe(path)!=Probe::Missing)return false;
  if(!portableName(path,pti))return false;
  const auto found=io.probe(path);if(found==Probe::Error)return false;
  if(found==Probe::Present){
   std::uint32_t length=0;if(!io.fileSize(path,length)||length!=kCompanionBytes||!io.read(path,scratch.bytes,kCompanionBytes))return false;
   std::uint8_t hash[32];std::uint32_t size=0;Slot slot;InstrumentState state;
   if(!io.digest(pti,hash,size)||!model.slot(destination,slot)||!checkedPortable(scratch.bytes,hash,size,slot,state))return false;
   return model.publish(destination,state);
  }
 }
 // No explicit portable file: retain the WR2LIFE rooted A/B lookup unchanged.
 char parent[kPath];std::memcpy(parent,pti,std::strlen(pti)+1);auto name=std::strrchr(parent,'/');if(!name)return false;*name++=0;
 unsigned number=0,digits=0;while(name[digits]>='0'&&name[digits]<='9'&&digits<2){number=number*10+unsigned(name[digits]-'0');++digits;}
 if(!digits||!number||number>48||name[digits]!=' ')return false;
 auto dir=std::strrchr(parent,'/');if(!dir||std::strcmp(dir+1,"instruments"))return false;*dir=0;
 if(std::strlen(parent)>=sizeof(scratch.folder))return false;
 std::memcpy(scratch.folder,parent,std::strlen(parent)+1);
 if(!rootReady(io,scratch.folder,scratch.bytes))return false;
 const unsigned source=number-1;
 InstrumentState state;if(!rootState(scratch.bytes,source,state)||!state.sliceCount)return false;
 Slot slot;if(!model.slot(destination,slot)||!slot.active||!slot.frames||slot.channels<1||slot.channels>2||slot.count!=state.sliceCount)return false;
 if(identity(slot)!=u32(scratch.bytes+32+source*kRecordBytes+4))return false;
 std::uint8_t c[kCompanionBytes],ptiHash[32];char path[kPath];std::uint32_t ptiSize=0;
 if(!io.digest(pti,ptiHash,ptiSize))return false;
 // A and B are equivalent caches for the same root digest. A corrupt copy may
 // fall back to the other; uncertain stat/read errors must fail closed.
 for(char side:{'a','b'}){if(!format(path,scratch.folder,source,side,".tmp")||io.probe(path)!=Probe::Missing)return false;}
 for(char side:{'a','b'}){
  if(!format(path,scratch.folder,source,side))return false;
  auto found=io.probe(path);if(found==Probe::Error)return false;
  if(found==Probe::Missing)continue;
  std::uint32_t size=0;if(!io.fileSize(path,size))return false;if(size!=sizeof(c))continue;
  if(!io.read(path,c,sizeof(c)))return false;
  if(checked(c,ptiHash,ptiSize,pti,scratch.bytes,source))return model.publish(destination,state);
 }
 return false;
}
bool copyInstrument(Model&model,unsigned source,unsigned destination){
 if(source>=48||destination>=48)return false;
 if(source==destination)return true;
 InstrumentState state;Slot a,b;
 const bool have=model.state(source,state)&&valid(state)&&model.slot(source,a)&&model.slot(destination,b);
 if(!model.erase(destination))return false;
 if(!have)return false;
 bool group=false;for(unsigned i=0;i<state.sliceCount;++i)if(state.owner[i]!=kFree)group=true;
 if(!group)return true;
 return a.active&&b.active&&a.frames&&a.channels>=1&&a.channels<=2&&state.sliceCount==a.count&&a.count==b.count&&identity(a)==identity(b)&&model.publish(destination,state);
}
} }
