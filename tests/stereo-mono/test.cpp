#define _USE_MATH_DEFINES
#include <cassert>
#include "../../core/effector/sampleProcessing/NativeDrive.h"
#include "../../core/effector/sampleProcessing/Conversion.h"
namespace sdmono=sampleedit;
#include <algorithm>
#include <cmath>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <climits>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <new>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::abort();}}while(0)
#define TRACKER_CE_ENABLE_SD_MONO 1
#include "../../core/FileManager/SampleFileEdit.h"
constexpr unsigned PATCH_SIZE=256;
constexpr unsigned kStreamPreCacheFrames=32768,kClmtElementCount=64,kWaveformPeakPoints=600,MAX_16BIT=65535,EDIT_BUFF_SIZE=1024*1024,SAMPLE_MEMORY_SIZE=
#ifdef SAMPLE_PREVIEW_TESTS
8*1024*1024;
#else
1024*1024;
#endif
constexpr unsigned playModeSlice=4,playModeBeatSlice=5,fmIdle=0,fmNoOperation=1;
static bool isPlayModeStreamable(unsigned mode){return mode<6;}
struct WaveformPeakPoint{int16_t minVal,maxVal;};
struct strInstrument{unsigned isActive=1;struct{int16_t*address=nullptr;uint32_t length=0;char name[32]{};}sample;unsigned playMode=5,sliceNumber=4;uint16_t slices[48]{};unsigned startPoint=0,endPoint=65535,loopPoint1=1000,loopPoint2=60000,volume=50;char name[32]{};};
struct strInstrumentFile{struct{char magic[4];strInstrument instrument;}data;uint32_t checksum;};
struct strSampleMemory{enum Channel{kLeft,kRight,kCount};int16_t*address[2]{};bool isStreamed=false,isStereo=true,hasValidClmt=false,hasWaveformPeaks=false;uint32_t ramFrames=0,preCacheFrames=0,fileDataOffset=0,databaseSector=0,sectorsPerCluster=0,sampleHash=0;};
class Sequencer{public:enum{STEP_NOTE_OFF_CUT=-3};};
static struct{struct{unsigned lastUsedInstrument=0;}values;strInstrument instrument[48];strSampleMemory sampleMemory[48];uint32_t used_memory=0,max_memory=SAMPLE_MEMORY_SIZE;}mtProject;
alignas(32) int16_t externalRamSampleMemory[SAMPLE_MEMORY_SIZE/2]{};
static int16_t*effectArena=externalRamSampleMemory+SAMPLE_MEMORY_SIZE/4;static int16_t*externalRamEffectsBuff=effectArena;
static uint8_t isBlockedInstrument[48]{},isBlockedNoteOn[48]{};
static struct{void setBlockedInstrument(unsigned i){isBlockedInstrument[i]=1;}void clearBlockedInstrument(unsigned i){isBlockedInstrument[i]=0;}void setBlockedNoteOnFromInstrument(unsigned i){isBlockedNoteOn[i]=1;}void clearBlockedNoteOnFromInstrument(unsigned i){isBlockedNoteOn[i]=0;}}engine;
static struct{unsigned currentInstrument_idx=48,cuts=0;void noteOff(int value){CHECK(value==-3);++cuts;}}instrumentPlayer[8];
static char s_instrumentStreamPaths[48][256]{};static uint32_t s_instrumentClmt[48][64]{};
static WaveformPeakPoint s_instrumentWaveformPeaks[48][2][600]{};
struct SliceCacheEntry{uint32_t startFrame;uint16_t frameCount,bufferOffset;};static SliceCacheEntry s_instrumentSliceCache[48][48]{};static bool s_instrumentSliceCacheValid[48]{};
class VoiceStreamManager{public:
#ifdef SAMPLE_PREVIEW_TESTS
 bool readInstrumentFramesBlocking(int,int,uint32_t,uint32_t,int16_t*);
#endif
static constexpr unsigned kMaxChannels=2;
#include "cache.inc"
 void invalidateInstrumentVoices(unsigned){}char*getInstrumentStreamPath(unsigned i){return s_instrumentStreamPaths[i];}
 void installMonoCache(int,const char*,uint32_t,const MonoCache&,bool,uint32_t,const MonoCache*right=nullptr);
}voiceStreamManager;
#include "install.inc"
static std::string startupOrder;static unsigned startupPopups=0;
static struct FM{
 bool openProjectFromWorkspace(){startupOrder+="W";return true;}
 bool suspended=false;unsigned status=fmIdle,operation=fmNoOperation;unsigned getStatus(){return status;}unsigned getCurrentOperation(){return operation;}
 bool isInstrumentStereo(unsigned i){return mtProject.sampleMemory[i].isStereo;}void setWorkspaceSaveSuspended(bool b){suspended=b;}
 void writeInstrumentToFileStruct(const strInstrument*i,strInstrumentFile*out){std::memset(out,0,sizeof(*out));std::memcpy(out->data.magic,"TI16",4);out->data.instrument=*i;out->checksum=sdmono::crc(out,sizeof(*out)-4);}
 unsigned calcFirstSlotToMoveInMemory(unsigned i){for(;i<48&&!mtProject.instrument[i].isActive;++i){}return i;}
 void moveSampleMemory(unsigned first,int32_t delta){for(unsigned i=first;i<48;++i)if(mtProject.instrument[i].isActive){auto&m=mtProject.sampleMemory[i];const unsigned frames=m.ramFrames;std::memmove(reinterpret_cast<uint8_t*>(m.address[0])+delta,m.address[0],frames*(m.isStereo?4:2));m.address[0]=reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(m.address[0])+delta);m.address[1]=m.isStereo?m.address[0]+frames:m.address[0];mtProject.instrument[i].sample.address=m.address[0];}}
 unsigned getSampleMemorySize(unsigned a,unsigned b){unsigned size=0;for(unsigned i=a;i<=b;++i)if(mtProject.instrument[i].isActive)size+=mtProject.sampleMemory[i].ramFrames*(mtProject.sampleMemory[i].isStereo?4:2);return size;}
 void setInstrumentStructChanged(unsigned){}void clearSampleChanged(unsigned){}
}newFileManager;
static const char*cWorkspaceInstrumentFileFormatNew="Workspace/instruments/instrument_%02d.pti";
using DWORD=uint32_t;using UINT=unsigned;using FRESULT=int;
constexpr int FR_OK=0,FR_NO_FILE=1,FR_NO_PATH=2,FR_DISK_ERR=3,FR_EXIST=4,FA_READ=1,FA_WRITE=2,FA_CREATE_ALWAYS=4,FA_CREATE_NEW=8,AM_DIR=16;
constexpr uint32_t CREATE_LINKMAP=UINT32_MAX;
struct FATFS{uint32_t database=1000,csize=8;}fat;
struct Crash{};static unsigned operations=0;static int fault=-1,cut=-1;static bool afterCut=false,shortWrite=false;static uint32_t freeClusters=0x100000;
static bool step(){++operations;if(int(operations)==cut&&!afterCut)throw Crash{};return int(operations)!=fault;}
static void finishIO(){if(int(operations)==cut&&afterCut)throw Crash{};}
static int patternMode=0;
static int16_t signal(unsigned ch,uint32_t i){int16_t a=static_cast<int16_t>((i*109u+32761u)&65535);if(patternMode==4)return 0;if(patternMode==0)return a;if(patternMode==1)return ch?static_cast<int16_t>(-int32_t(a)):a;if(patternMode==2)return ch?0:a;return ch?static_cast<int16_t>((i*37u+511u)&65535):a;}
struct File{FILE*disk=nullptr;uint32_t size=0,frames=0;strInstrumentFile header{};bool generated=false;~File(){if(disk)std::fclose(disk);}};
static std::map<std::string,std::shared_ptr<File>>files;
struct FIL{std::shared_ptr<File>p;uint32_t pos=0;DWORD*cltbl=nullptr;struct{FATFS*fs=&fat;}obj;};struct FILINFO{uint32_t fsize=0;unsigned fattrib=0;};
static uint32_t f_size(const FIL*f){return f->p->size;}
static FRESULT f_stat(const char*name,FILINFO*out){if(!step())return FR_DISK_ERR;auto it=files.find(name);if(it==files.end())return FR_NO_FILE;out->fsize=it->second->size;finishIO();return FR_OK;}
static FRESULT f_open(FIL*f,const char*name,unsigned flags){if(!step())return FR_DISK_ERR;if((flags&FA_CREATE_NEW)&&files.count(name))return FR_DISK_ERR;if(flags&(FA_CREATE_NEW|FA_CREATE_ALWAYS)){auto p=std::make_shared<File>();p->disk=std::tmpfile();CHECK(p->disk);files[name]=p;}auto it=files.find(name);if(it==files.end())return FR_NO_FILE;f->p=it->second;f->pos=0;finishIO();return FR_OK;}
static FRESULT f_lseek(FIL*f,uint32_t pos){if(!step())return FR_DISK_ERR;if(pos==CREATE_LINKMAP){f->cltbl[0]=4;f->cltbl[1]=uint32_t((f->p->size+4095)/4096);f->cltbl[2]=100;f->cltbl[3]=0;}else f->pos=pos;finishIO();return FR_OK;}
static FRESULT f_read(FIL*f,void*dst,unsigned n,UINT*got){if(!step())return FR_DISK_ERR;n=std::min(n,f->p->size-f->pos);*got=n;auto*out=static_cast<uint8_t*>(dst);
 if(f->p->generated){for(unsigned j=0;j<n;++j){const uint32_t off=f->pos+j;if(off<sizeof(strInstrumentFile))out[j]=reinterpret_cast<const uint8_t*>(&f->p->header)[off];else{const uint32_t k=off-sizeof(strInstrumentFile),ch=k/(f->p->frames*2),frame=(k/2)%f->p->frames;const auto v=static_cast<uint16_t>(signal(ch,frame));out[j]=static_cast<uint8_t>(v>>((k&1u)*8));}}}
 else{CHECK(std::fseek(f->p->disk,f->pos,SEEK_SET)==0);*got=static_cast<UINT>(std::fread(dst,1,n,f->p->disk));}
 f->pos+=*got;finishIO();return FR_OK;}
static FRESULT f_write(FIL*f,const void*src,unsigned n,UINT*put){if(!step()){*put=0;return FR_DISK_ERR;}if(shortWrite)n/=2;CHECK(std::fseek(f->p->disk,f->pos,SEEK_SET)==0);*put=static_cast<UINT>(std::fwrite(src,1,n,f->p->disk));f->pos+=*put;f->p->size=std::max(f->p->size,f->pos);finishIO();return FR_OK;}
static FRESULT f_sync(FIL*f){if(!step())return FR_DISK_ERR;const auto result=std::fflush(f->p->disk)==0?FR_OK:FR_DISK_ERR;finishIO();return result;}
static FRESULT f_close(FIL*f){const bool ok=step();f->p.reset();finishIO();return ok?FR_OK:FR_DISK_ERR;}
static FRESULT f_rename(const char*from,const char*to){if(!step())return FR_DISK_ERR;auto it=files.find(from);if(it==files.end()||files.count(to))return FR_DISK_ERR;files[to]=it->second;files.erase(it);finishIO();return FR_OK;}
static FRESULT f_unlink(const char*p){if(!step())return FR_DISK_ERR;if(!files.erase(p))return FR_NO_FILE;finishIO();return FR_OK;}
static FRESULT f_getfree(const char*,DWORD*out,FATFS**fs){if(!step())return FR_DISK_ERR;*out=freeClusters;*fs=&fat;finishIO();return FR_OK;}
#include "../../core/FileManager/SampleEditNotifications.h"
#include "notifications.inc"
#ifdef SAMPLE_PREVIEW_TESTS
namespace samplepreview{bool active();}
#else
namespace samplepreview{static bool active(){return false;}}
#endif
#include "effect-base.inc"
#include "preview-spec.inc"
#ifndef SAMPLE_PREVIEW_TESTS
namespace samplepreview {const EffectCopy* effectCopy(unsigned){return nullptr;}}
#endif
#include "native.inc"
static struct{void showInfoPopup(const char*,const char*){++startupPopups;}}mtPopups;
class Interface{public:void openStartupProject();void openNewProject(){startupOrder+="N";}};
#include "startup.inc"
static const char*canonical="Workspace/instruments/instrument_01.pti";
static unsigned lastNativeOps=0;static sdmono::Mode conversionMode=sdmono::Mode::Mix;
static void reset(unsigned frames,bool streamed,int mode=3,bool external=false){
 if(sdmono::active()){sdmono::cancel();sdmono::result();}files.clear();operations=0;fault=cut=-1;afterCut=shortWrite=false;freeClusters=0x100000;patternMode=mode;conversionMode=sdmono::Mode::Mix;externalRamEffectsBuff=effectArena;
 mtProject={};for(auto&i:mtProject.instrument)i.isActive=0;std::memset(isBlockedInstrument,0,48);std::memset(isBlockedNoteOn,0,48);newFileManager=FM{};
 for(auto&v:instrumentPlayer){v.currentInstrument_idx=48;v.cuts=0;}instrumentPlayer[0].currentInstrument_idx=0;
 auto&i=mtProject.instrument[0];i={};i.isActive=1;i.sample.length=frames;i.sample.address=externalRamSampleMemory;i.slices[0]=0;i.slices[1]=10000;i.slices[2]=40000;i.slices[3]=65535;std::strcpy(i.name,"Original");std::strcpy(i.sample.name,"Sample");
 auto&m=mtProject.sampleMemory[0];m.isStereo=true;m.isStreamed=streamed;m.ramFrames=streamed?std::min(frames,kStreamPreCacheFrames):frames;m.preCacheFrames=m.ramFrames;m.address[0]=externalRamSampleMemory;m.address[1]=m.address[0]+m.ramFrames;m.fileDataOffset=sizeof(strInstrumentFile);m.sampleHash=123;
 for(unsigned c=0;c<2;++c)for(uint32_t j=0;j<m.ramFrames;++j)m.address[c][j]=signal(c,j);
 auto old=std::make_shared<File>();old->generated=true;old->frames=frames;old->size=static_cast<uint32_t>(sizeof(strInstrumentFile)+uint64_t(frames)*4);newFileManager.writeInstrumentToFileStruct(&i,&old->header);
 files[canonical]=old;const char*source=external?"OtherProject/instruments/source.pti":canonical;if(external)files[source]=old;std::snprintf(s_instrumentStreamPaths[0],256,"%s",source);
 mtProject.used_memory=m.ramFrames*4;
}
static std::string drive(){for(unsigned n=0;n<3000000&&sdmono::active()&&!sdmono::done();++n)sdmono::tick();CHECK(sdmono::done());lastNativeOps=operations;return sdmono::result();}
static void monoFile(unsigned frames){auto it=files.find(canonical);if(it==files.end()||it->second->generated||it->second->size!=sizeof(strInstrumentFile)+uint64_t(frames)*2)std::fprintf(stderr,"MONO FILE n=%u size=%u generated=%d\n",frames,it==files.end()?0:it->second->size,it==files.end()?0:int(it->second->generated));CHECK(it!=files.end()&&!it->second->generated&&it->second->size==sizeof(strInstrumentFile)+uint64_t(frames)*2);FIL f;CHECK(f_open(&f,canonical,FA_READ)==FR_OK);strInstrumentFile h;UINT got;CHECK(f_read(&f,&h,sizeof(h),&got)==FR_OK&&got==sizeof(h));CHECK(h.data.instrument.sample.length==frames&&h.data.instrument.loopPoint1==1000&&h.data.instrument.loopPoint2==60000);int16_t block[1024];for(unsigned pos=0;pos<frames;pos+=1024){const auto n=std::min(1024u,frames-pos);CHECK(f_read(&f,block,n*2,&got)==FR_OK&&got==n*2);for(unsigned j=0;j<n;++j){const int32_t sum=int32_t(signal(0,pos+j))+int32_t(signal(1,pos+j));CHECK(block[j]==(conversionMode==sdmono::Mode::Left?signal(0,pos+j):conversionMode==sdmono::Mode::Right?signal(1,pos+j):sum/2));}}CHECK(f_close(&f)==FR_OK);}
static void validRun(unsigned frames,bool stream,int mode,bool external=false,unsigned playback=5,sdmono::Mode choice=sdmono::Mode::Mix){reset(frames,stream,mode,external);conversionMode=choice;mtProject.instrument[0].playMode=playback;const auto original=mtProject.instrument[0];auto originalFile=files[canonical];if(!sdmono::begin(0,conversionMode)){std::fprintf(stderr,"begin frames=%u stream=%d: %s\n",frames,stream,sdmono::result());CHECK(false);}CHECK(drive().find("Converted")==0);CHECK(!std::memcmp(&original,&mtProject.instrument[0],sizeof(original)));const auto&m=mtProject.sampleMemory[0];CHECK(!m.isStereo&&m.isStreamed==stream&&m.address[0]==m.address[1]);CHECK(mtProject.used_memory==(stream?std::min(frames,kStreamPreCacheFrames):frames)*2);CHECK(!newFileManager.suspended&&!isBlockedInstrument[0]&&!isBlockedNoteOn[0]);
monoFile(frames);if(external)CHECK(files["OtherProject/instruments/source.pti"]==originalFile);if(stream){CHECK(s_instrumentSliceCacheValid[0]);for(unsigned k=0;k<4;++k){auto&entry=s_instrumentSliceCache[0][k];CHECK(entry.startFrame<frames&&entry.bufferOffset+entry.frameCount<=kStreamPreCacheFrames);for(unsigned j=0;j<entry.frameCount;++j)CHECK(m.address[0][entry.bufferOffset+j]==(conversionMode==sdmono::Mode::Left?signal(0,entry.startFrame+j):conversionMode==sdmono::Mode::Right?signal(1,entry.startFrame+j):(int32_t(signal(0,entry.startFrame+j))+int32_t(signal(1,entry.startFrame+j)))/2));}}}
static void numeric(){for(int left=-32768;left<=32767;++left)for(int right:{-32768,-32767,-1,0,1,32766,32767})CHECK(sdmono::mix(static_cast<int16_t>(left),static_cast<int16_t>(right))==(left+right)/2);CHECK(sdmono::mix(-32768,32767)==0);for(int left=-32768;left<=32767;++left)for(int right:{-32768,-32767,-1,0,1,32766,32767}){CHECK(sdmono::convert(int16_t(left),int16_t(right),sdmono::Mode::Left)==left);CHECK(sdmono::convert(int16_t(left),int16_t(right),sdmono::Mode::Right)==right);}std::puts("PASS PCM: 458752 extreme/sign pairs in all three modes, no overflow");}
static void streamedErrors(){
 reset(33000,true,3,true);CHECK(sdmono::begin(0,conversionMode));drive();const auto count=lastNativeOps;
 for(unsigned n=1;n<=count;++n){reset(33000,true,3,true);const auto inst=mtProject.instrument[0];const auto memory=mtProject.sampleMemory[0];auto external=files["OtherProject/instruments/source.pti"];std::vector<int16_t>originalCache(memory.address[0],memory.address[0]+memory.ramFrames*2);operations=0;fault=static_cast<int>(n);const bool started=sdmono::begin(0,conversionMode);std::string result=started?drive():sdmono::result();fault=-1;
  CHECK(files["OtherProject/instruments/source.pti"]==external&&!newFileManager.suspended);
  if(result.find("Converted")!=0){CHECK(!std::memcmp(&inst,&mtProject.instrument[0],sizeof(inst))&&!std::memcmp(&memory,&mtProject.sampleMemory[0],sizeof(memory)));CHECK(!std::memcmp(originalCache.data(),memory.address[0],originalCache.size()*2));}
 }
 std::printf("PASS streamed IO: %u source/precache/peaks/slices/CLMT/transaction failures, external PTI untouched\n",count);
}
static void errors(){
 reset(65,false);CHECK(sdmono::begin(0,conversionMode));CHECK(drive().find("Converted")==0);const auto count=lastNativeOps;
 for(unsigned n=1;n<=count;++n){reset(65,false);const auto inst=mtProject.instrument[0];const auto memory=mtProject.sampleMemory[0];auto original=files[canonical];std::vector<int16_t>ram(memory.address[0],memory.address[0]+130);operations=0;fault=static_cast<int>(n);bool started=sdmono::begin(0,conversionMode);std::string result=started?drive():sdmono::result();fault=-1;
  CHECK(!newFileManager.suspended);if(result.find("Converted")==0){monoFile(65);}else{CHECK(!std::memcmp(&inst,&mtProject.instrument[0],sizeof(inst)));CHECK(!std::memcmp(&memory,&mtProject.sampleMemory[0],sizeof(memory)));CHECK(!std::memcmp(ram.data(),memory.address[0],ram.size()*2));CHECK(files.count(canonical)&&files[canonical]==original);}
 }
 reset(100,false);const auto original=files[canonical];freeClusters=0;CHECK(!sdmono::begin(0,conversionMode)&&files[canonical]==original);freeClusters=0x100000;
 reset(100,false);shortWrite=true;CHECK(sdmono::begin(0,conversionMode));CHECK(drive().find("Converted")!=0&&files[canonical]->generated);
 for(unsigned ticks:{0u,1u,2u,10u,100u,500u}){reset(32769,true);const auto inst=mtProject.instrument[0];auto source=files[canonical];CHECK(sdmono::begin(0,conversionMode));for(unsigned i=0;i<ticks&&sdmono::cancellable();++i)sdmono::tick();sdmono::cancel();CHECK(sdmono::done());CHECK(std::string(sdmono::result()).find("Cancelled")==0&&files[canonical]==source&&!std::memcmp(&inst,&mtProject.instrument[0],sizeof(inst)));}
 std::printf("PASS IO: %u injected native-operation failures, SD full/short writes and cancellation; original retained\n",count);
}
static void power(){reset(65,false);CHECK(sdmono::begin(0,conversionMode));drive();const auto total=lastNativeOps;
 for(unsigned n=1;n<=total;++n)for(bool after:{false,true}){reset(65,false);operations=0;cut=static_cast<int>(n);afterCut=after;
  try{if(sdmono::begin(0,conversionMode)){while(sdmono::active()&&!sdmono::done())sdmono::tick();}}catch(const Crash&){}
  cut=-1;if(sdmono::job){sdmono::job->~Job();sdmono::job=nullptr;}CHECK(sdmono::recover());CHECK(files.count(canonical));if(!files[canonical]->generated)monoFile(65);
 }
 std::printf("PASS recovery: simulated reset before/after %u filesystem boundaries; old or fully validated mono canonical\n",total);
}
static void startupRecovery(){
 reset(65,false);CHECK(sdmono::begin(0,conversionMode));while(!sdmono::done()&&sdmono::job->step!=sdmono::Step::Ram)sdmono::tick();CHECK(sdmono::job->step==sdmono::Step::Ram);
 sdmono::job->~Job();sdmono::job=nullptr;startupOrder.clear();startupPopups=0;Interface ui;ui.openStartupProject();
 CHECK(startupOrder=="W");
 CHECK(!startupPopups&&!files.count(std::string(canonical)+".mono.pending")&&!files.count(std::string(canonical)+".mono.bak"));monoFile(65);
 reset(65,false);startupOrder.clear();startupPopups=0;fault=operations+1;ui.openStartupProject();fault=-1;CHECK(startupOrder.empty()&&startupPopups==1);
 std::puts("PASS actual startup hook: recovery precedes Workspace load; failed recovery blocks fallback");
}
static void workMemory(){
 reset(65,false);mtProject.instrument[2].isActive=1;CHECK(sdmono::begin(0,conversionMode));CHECK(drive().find("Converted")==0);
 reset(200000,false);externalRamEffectsBuff=externalRamSampleMemory+64;CHECK(sdmono::begin(0,conversionMode));CHECK(reinterpret_cast<uintptr_t>(sdmono::job)>=reinterpret_cast<uintptr_t>(externalRamSampleMemory+400000));CHECK(drive().find("Converted")==0);monoFile(200000);
 reset(250000,false);externalRamEffectsBuff=externalRamSampleMemory+64;auto original=files[canonical];CHECK(!sdmono::begin(0,conversionMode));CHECK(files[canonical]==original&&mtProject.sampleMemory[0].isStereo);externalRamEffectsBuff=effectArena;
 std::puts("PASS RAM occupancy: effect-arena overlap uses free tail; insufficient tail refuses before PCM/file mutation");
}
static void corruptAndNeighbors(){
 reset(33000,true);auto original=files[canonical];CHECK(sdmono::begin(0,conversionMode));while(sdmono::job->conversion.phase()==sdmono::Phase::Mixing)sdmono::tick();
 CHECK(sdmono::job->conversion.phase()==sdmono::Phase::Verifying);auto tmp=files[std::string(canonical)+".mono.tmp"];CHECK(tmp&&tmp->disk);CHECK(std::fseek(tmp->disk,sizeof(strInstrumentFile)+17,SEEK_SET)==0);const unsigned char corrupt=123;CHECK(std::fwrite(&corrupt,1,1,tmp->disk)==1);CHECK(std::fflush(tmp->disk)==0);
 CHECK(drive().find("Converted")!=0&&files[canonical]==original&&mtProject.sampleMemory[0].isStereo);
 for(bool stream:{false,true}){reset(33000,stream);const auto end=mtProject.sampleMemory[0].address[1]+mtProject.sampleMemory[0].ramFrames;
  auto&i=mtProject.instrument[1];i=mtProject.instrument[0];i.sample.length=700;i.sample.address=end;auto&m=mtProject.sampleMemory[1];m=mtProject.sampleMemory[0];m.address[0]=end;m.address[1]=end+700;m.ramFrames=700;m.isStreamed=false;
  for(unsigned n=0;n<1400;++n)end[n]=int16_t(n*17);std::vector<int16_t>before(end,end+1400);const auto inst=i;instrumentPlayer[1].currentInstrument_idx=1;isBlockedInstrument[1]=1;isBlockedNoteOn[1]=1;
  CHECK(sdmono::begin(0,conversionMode));CHECK(drive().find("Converted")==0);CHECK(m.address[0]==externalRamSampleMemory+(stream?kStreamPreCacheFrames:33000));CHECK(m.address[1]==m.address[0]+700);CHECK(!std::memcmp(before.data(),m.address[0],2800));auto after=i;after.sample.address=inst.sample.address;CHECK(!std::memcmp(&inst,&after,sizeof(inst)));CHECK(isBlockedInstrument[1]&&isBlockedNoteOn[1]&&instrumentPlayer[1].cuts==1);
 }
 std::puts("PASS corruption/relocation: reread CRC rejects changed payload; following stereo slot PCM/metadata/blocking survive RAM compaction");
}


namespace modeUi{
namespace samplepreview {static const ::samplepreview::EffectCopy* effectCopy(unsigned id){return ::samplepreview::effectCopy(id==10?9:id==11?10:11);}}
constexpr int editorEffectStereoMonoSD=0,editorEffectModular=1,editorEffectCut=2,editorEffectReverse=3,editorEffectFadeIn=4,editorEffectFadeOut=5,editorEffectNormalizer=6,editorEffectAmplifier=7,editorEffectOverdrive=8,editorEffectBitcrusher=9,editorEffectEQ=10,editorEffectLimiter=11,editorEffectCompresor=12,kMaxParametersInEffect=12,interfaceButton6=6,buttonPress=1;
static unsigned discarded=0,messages=0;static uint8_t s_modularListCounts[12]{};static char*s_modularListItems[12][3]{};
struct Input{void clearAll(){}void setButtonObj(int,int,void(*)()){}}input;
struct cSampleEditor{
 struct Spectrum{int16_t upperData[600]{},lowerData[600]{};int spectrumType=0;};int zoom=0;Spectrum spectrumData[2]{};strInstrument*editorInstrument=&mtProject.instrument[0];struct{uint16_t startPoint=0,endPoint=65535;}selection;
 bool confirmedDataIsChanged=false;struct{float parameter[12]{};}effectDisplayParamsToAdjust[13];int label[10]{};unsigned applyingProgress=0,currentEffectIdx=0;
 unsigned starts=0,progressHides=0;bool needRefreshSpectrum=false;void start(unsigned){++starts;selection.startPoint=0;selection.endPoint=65535;}void hideProgressPopup(){++progressHides;}void refreshStartPoint(){}void refreshEndPoint(){}void complete();
 Input*ip=&input;void showPopupApplying(){}uint8_t getParameterListCount(uint8_t)const;char**getParameterListItems(uint8_t)const;
}editor;static cSampleEditor*SE=&editor;
struct Effect:mtEffect{void setParameter(float&,int)override{} bool startProcess()override{return true;}int32_t processBlock()override{return 0;}bool getProcessState()override{return false;}uint32_t getExpectedProcessLength()override{return 0;}void discardForFileOperation(){++discarded;}}dummyEffect;static mtEffect*sampleEditorEffect[]={&dummyEffect,&dummyEffect,&dummyEffect,&dummyEffect,&dummyEffect,&dummyEffect,&dummyEffect,&dummyEffect,&dummyEffect,&dummyEffect,&dummyEffect,&dummyEffect,&dummyEffect};
static struct{void setControlText(int,const char*){}void refreshControl(int){}}display;
static bool cropInProgress=false;static std::string lastMessage;static void monoMessage(const char*text){++messages;lastMessage=text;}static void monoCancel(){sdmono::cancel();}
static void releaseLargePreview(){}
#include "mode-ui.inc"
#include "completion-ui.inc"
#if TRACKER_CE_ENABLE_SD_EDITS
static unsigned streamedDraws=0;static struct{void processSpectrum(strSampleMemory*m,strInstrument*i,int*,cSampleEditor::Spectrum*,strSampleMemory::Channel){CHECK(m==&mtProject.sampleMemory[0]&&i==&mtProject.instrument[0]);++streamedDraws;}}GP;
#include "stream-ui.inc"
#endif
static void completionTests(){
 auto finish=[](){while(sdmono::active()&&!sdmono::done())sdmono::tick();CHECK(sdmono::done());SE->complete();CHECK(!sdmono::active());};
 auto prepare=[](){SE->starts=SE->progressHides=messages=0;cropInProgress=false;SE->selection.startPoint=12345;SE->selection.endPoint=54321;};
 for(auto mode:{sdmono::Mode::Mix,sdmono::Mode::Left,sdmono::Mode::Right}){reset(1025,true,3,true);prepare();conversionMode=mode;CHECK(sdmono::begin(0,mode));finish();CHECK(!messages&&SE->starts==1&&SE->progressHides==1);CHECK(SE->selection.startPoint==12345&&SE->selection.endPoint==54321);monoFile(1025);}
#if TRACKER_CE_ENABLE_SD_EDITS
 for(auto edit:{sdmono::Edit::Crop,sdmono::Edit::Reverse,sdmono::Edit::FadeIn,sdmono::Edit::FadeOut}){reset(1025,true,3,true);prepare();cropInProgress=edit==sdmono::Edit::Crop;CHECK(sdmono::beginEdit(0,edit,12345,54321));finish();CHECK(!messages&&SE->starts==1&&SE->progressHides==1);CHECK(SE->selection.startPoint==(cropInProgress?0:12345)&&SE->selection.endPoint==(cropInProgress?65535:54321));CHECK(files.count(canonical)&&!files[canonical]->generated);}
#endif
 reset(65,false);prepare();auto original=files[canonical];CHECK(sdmono::begin(0));fault=int(operations)+2;finish();fault=-1;CHECK(messages==1&&lastMessage.find("error")!=std::string::npos&&files[canonical]==original);
 reset(65,false);prepare();original=files[canonical];CHECK(sdmono::begin(0));sdmono::cancel();finish();CHECK(messages==1&&lastMessage.find("Cancelled")==0&&files[canonical]==original);
 reset(65,false);prepare();CHECK(sdmono::begin(0));while(sdmono::job->step!=sdmono::Step::Finish)sdmono::tick();fault=int(operations)+2;finish();fault=-1;CHECK(messages==1&&lastMessage.find("backup retained")!=std::string::npos);monoFile(65);
 std::puts("PASS actual completion UI: silent success for three Mono modes and four SD edits; Workspace/selection retained; IO errors, cancellation and cleanup warnings still displayed");
}
static void test(){
 CHECK(SE->getParameterListCount(0)==4&&SE->getParameterListCount(1)==0);
 auto items=SE->getParameterListItems(0);CHECK(items&&!std::strcmp(items[0],"Merge L+R")&&!std::strcmp(items[1],"Left Only")&&!std::strcmp(items[2],"Right Only")&&!std::strcmp(items[3],"Side L-R"));CHECK(!SE->getParameterListItems(1));
 for(unsigned n=0;n<3;++n){reset(1025,n==2,3,true);conversionMode=static_cast<sdmono::Mode>(n);SE->selection.startPoint=0;SE->selection.endPoint=65535;SE->effectDisplayParamsToAdjust[0].parameter[0]=float(n);discarded=messages=0;monoBegin();CHECK(sdmono::active()&&discarded==13&&!messages);CHECK(sdmono::job->mode==conversionMode);CHECK(drive().find("Converted")==0);monoFile(1025);}
 reset(65,false);SE->confirmedDataIsChanged=true;messages=0;monoBegin();CHECK(!sdmono::active()&&messages==1);SE->confirmedDataIsChanged=false;
 std::puts("PASS actual UI hooks: three native list entries and monoBegin feed selected mode into production conversion; unsaved editor changes blocked");
}
}
#if TRACKER_CE_ENABLE_SD_EDITS
#include "../sd-effects/regions.inc"
#endif

static void sourceGainFiles(){
 for(bool stereo:{false,true})for(unsigned source=0;source<4;++source)for(float db:{-12.0f,0.0f,6.0f})for(unsigned pattern:{1u,2u,3u,4u}){
  const unsigned n=1025;reset(n,true,int(pattern),true);if(!stereo){auto&m=mtProject.sampleMemory[0];m.isStereo=false;m.address[1]=m.address[0];files[canonical]->size=sizeof(strInstrumentFile)+n*2;mtProject.used_memory=m.ramFrames*2;}
  const uint16_t pa=9000,pb=60000;const auto a=uint32_t(pa*float(n)/65535),b=uint32_t(pb*float(n)/65535);const float gain=db==0?1.0f:std::pow(10.0f,db/20.0f);
  CHECK(sdmono::begin(0,static_cast<sdmono::Mode>(source),db,pa,pb));CHECK(drive().find("Converted")==0);
  FIL f;CHECK(f_open(&f,canonical,FA_READ)==FR_OK);CHECK(f_lseek(&f,sizeof(strInstrumentFile))==FR_OK);int16_t data[n];UINT got=0;CHECK(f_read(&f,data,sizeof(data),&got)==FR_OK&&got==sizeof(data));CHECK(f_close(&f)==FR_OK);
  for(unsigned at=0;at<n;++at){int value=signal(0,at);if(at>=a&&at<b){if(stereo){const int right=signal(1,at);value=source==1?value:source==2?right:source==3?(value-right)/2:(value+right)/2;}value=int32_t(value*gain);value=std::clamp(value,-32768,32767);}CHECK(data[at]==value);}
 }
 CHECK(!modeUi::isSdMonoEffect(modeUi::editorEffectStereoMonoSD,8192));CHECK(modeUi::isSdMonoEffect(modeUi::editorEffectStereoMonoSD,EDIT_BUFF_SIZE));
#if TRACKER_CE_ENABLE_SD_EDITS
 for(auto op:{sdmono::Edit::Normalize,sdmono::Edit::Amplifier}){reset(1025,true,4,true);CHECK(sdmono::beginEdit(0,op,0,65535,op==sdmono::Edit::Normalize?100.0f:5.0f));CHECK(drive().find("Edited")==0);FIL f;CHECK(f_open(&f,canonical,FA_READ)==FR_OK&&f_lseek(&f,sizeof(strInstrumentFile))==FR_OK);int16_t zero[2050];UINT got;CHECK(f_read(&f,zero,sizeof(zero),&got)==FR_OK&&got==sizeof(zero));for(auto v:zero)CHECK(v==0);CHECK(f_close(&f)==FR_OK);}
#endif
 std::puts("PASS SD Source/Gain: all 4 modes, mono/stereo, partial selection, opposing/silent channels, clipping and silent Normalize/Amplifier; automatic RAM/SD boundary");
}
int main(){std::setvbuf(stdout,nullptr,_IONBF,0);sourceGainFiles();
#if TRACKER_CE_ENABLE_SD_EDITS
regionTests();
#endif
numeric();for(auto choice:{sdmono::Mode::Mix,sdmono::Mode::Left,sdmono::Mode::Right}){for(unsigned n:{1u,2u,1023u,1024u,1025u,65537u})for(int mode:{0,1,2,3,4})validRun(n,false,mode,false,5,choice);for(unsigned mode=0;mode<8;++mode)validRun(1025,false,3,false,mode,choice);validRun(50000000,true,3,true,5,choice);}std::puts("PASS large: 200 MB virtual stereo source -> 100 MB real temporary mono file, bounded buffers; frames/metadata/slices retained");errors();streamedErrors();power();
corruptAndNeighbors();workMemory();startupRecovery();modeUi::test();modeUi::completionTests();reset(65,false);const auto beforeInvalid=operations;CHECK(!sdmono::begin(0,static_cast<sdmono::Mode>(4))&&operations==beforeInvalid);reset(65,false);mtProject.sampleMemory[0].isStereo=false;mtProject.sampleMemory[0].address[1]=mtProject.sampleMemory[0].address[0];CHECK(sdmono::begin(0,conversionMode));CHECK(drive().find("Converted")==0);std::printf("PASS StereoMono: %u checks; complete production conversion/transaction/cache installer, filesystem simulated; no hardware validation\n",checks);}
