#if TRACKER_CE_ENABLE_WR2
#include "mtStructs.h"
#include "fileManager.h"
#include "NativeBridge.h"
#include "NativeUi.h"
#include "NativePersistence.h"
#include "log.h"
#include "../../platforms/imx/fatfs/source/ff.h"
#include "../../modules/wr2/Persistence.cpp"
#include "../../modules/wr2/sha256.cpp"
#include "../../modules/wr2/Companion.cpp"
#include <cr_section_macros.h>
#include <algorithm>
#include <cstring>
#include <limits>
namespace {
using namespace wr2;
class NativeModel final:public persist::Model {
public:
 bool slot(unsigned i,persist::Slot&out)override{
  if(i>=48)return false;
  const auto&inst=mtProject.instrument[i];
  out.active=inst.isActive==1;out.count=inst.sliceNumber;out.type=inst.sample.type;
  out.frames=inst.sample.length;out.channels=mtProject.sampleMemory[i].isStereo?2:1;
  const auto n=std::min(sizeof(out.name)-1,sizeof(inst.sample.file_name));
  std::memcpy(out.name,inst.sample.file_name,n);out.name[n]=0;
  for(unsigned j=0;j<48;++j)out.cuts[j]=inst.slices[j];
  return true;
 }
 bool state(unsigned i,InstrumentState&out)override{
  auto&row=wr2native::publication().instrument[i];if(!row.writable())return false;
  audio::Snapshot snapshot;if(row.read(snapshot)){out=snapshot.state;return true;}
  out.reset(0);return row.context()==0;
 }
 bool ready()override{
  for(auto&row:wr2native::publication().instrument)if(!row.writable()||row.context()==std::numeric_limits<uint32_t>::max())return false;
  return true;
 }
 bool erase(unsigned i)override{return i<48&&wr2ui::editor().invalidate(i);}
 bool publish(unsigned i,const InstrumentState&state)override{
  if(i>=48)return false;
  auto&target=wr2native::publication().instrument[i];if(!target.writable()||target.context()==std::numeric_limits<uint32_t>::max())return false;
  if(!state.sliceCount)return true; // clear() already removed previous context; keep empty rows dormant
  auto&row=wr2native::publication().instrument[i];return row.publish(state,row.context()+1)==audio::PublishResult::Published;
 }
 void clear()override{wr2ui::editor().invalidateAll();}
};
// The same foreground FatFs/SD driver as CE. Unlike FileSystem's bool exists and
// void close, FRESULT distinguishes absence/errors and checks sync/close.
class FatIO final:public persist::IO {
public:
 persist::Probe probe(const char*p)override{
  FILINFO info{};auto rc=f_stat(p,&info);
  if(rc==FR_NO_FILE||rc==FR_NO_PATH)return persist::Probe::Missing;
  if(rc!=FR_OK||(info.fattrib&AM_DIR))return persist::Probe::Error;
  return persist::Probe::Present;
 }
 bool read(const char*p,uint8_t*out,std::size_t n)override{
  FIL file{};if(f_open(&file,p,FA_READ)!=FR_OK)return false;
  UINT got=0;bool ok=f_size(&file)==n&&f_read(&file,out,static_cast<UINT>(n),&got)==FR_OK&&got==n;
  auto closed=f_close(&file);return ok&&closed==FR_OK;
 }
 bool write(const char*p,const uint8_t*data,std::size_t n)override{
  FIL file{};if(f_open(&file,p,FA_WRITE|FA_CREATE_ALWAYS)!=FR_OK)return false;
  UINT put=0;bool ok=f_write(&file,data,static_cast<UINT>(n),&put)==FR_OK&&put==n;
  if(f_sync(&file)!=FR_OK)ok=false;
  auto closed=f_close(&file);return ok&&closed==FR_OK;
 }
 bool fileSize(const char*p,std::uint32_t&size)override{FILINFO f{};if(f_stat(p,&f)!=FR_OK||(f.fattrib&AM_DIR))return false;size=f.fsize;return true;}
 bool directory(const char*p)override{auto rc=f_mkdir(p);if(rc==FR_OK)return true;FILINFO f{};return rc==FR_EXIST&&f_stat(p,&f)==FR_OK&&(f.fattrib&AM_DIR);}
 bool digest(const char*p,uint8_t*out,std::uint32_t&size)override{
  FIL file{};if(f_open(&file,p,FA_READ)!=FR_OK)return false;size=f_size(&file);
  wr2_sha256_t hash;wr2_sha256_init(&hash);uint8_t buffer[512];std::uint32_t remaining=size;bool ok=true;
  while(remaining){UINT got=0;const UINT chunk=std::min<std::uint32_t>(remaining,sizeof(buffer));if(f_read(&file,buffer,chunk,&got)!=FR_OK||got!=chunk){ok=false;break;}wr2_sha256_update(&hash,buffer,chunk);remaining-=chunk;}
  if(f_close(&file)!=FR_OK)ok=false;
  if(ok)wr2_sha256_final(&hash,out);
  return ok;
 }
 bool move(const char*a,const char*b)override{return f_rename(a,b)==FR_OK;}
 bool remove(const char*p)override{return f_unlink(p)==FR_OK;}
};
__NOINIT(EXTERNAL_RAM) persist::Storage scratch;
NativeModel model;FatIO io;persist::Service service(io,model,scratch);
bool currentFolder(char*path){
 const int count=std::snprintf(path,256,"%s/%s",newFileManager.getCurrentProjectPath(),newFileManager.getCurrentProjectName());
 return count>0&&count<256&&newFileManager.getCurrentProjectName()[0];
}
}
namespace wr2persist {
bool beginSave(){char path[256];if(!currentFolder(path))return false;return service.beginSave(path);}
bool saveReady(){return service.saveReady();}
bool finishSave(){return service.commitSave(wr2::persist::exportProject)&&persist::clearReplacements(io);}
void stageStartupOpen(){service.stageStartupOpen();}
void stageOpen(){char path[256];if(!currentFolder(path)||!service.stageOpen(path)){service.abort();LOG("WR2: invalid project sidecar path");}}
void finishOpen(){const bool explicitOpen=service.projectOpening();const auto suppressed=service.startupOpening()?persist::readReplacements(io):0;if(service.startupOpening())stageOpen();auto result=service.finishOpen(suppressed);(void)result;if(explicitOpen)resetWorkspace();if(suppressed){LOG("WR2 startup: replaced Workspace slots suppressed");}LOGF("WR2 project groups result: %u\n",static_cast<unsigned>(result));}
bool markInstrumentReplacement(unsigned i){return persist::markReplacement(io,i);}
void resetWorkspace(){if(!persist::clearReplacements(io)){LOG("WR2: Workspace replacement marker retained after IO failure");}}
void copyInstrument(unsigned source,unsigned destination){if(!persist::copyInstrument(model,source,destination)){LOG("WR2: copy metadata rejected");}}
void importInstrument(unsigned destination,const char*folder,const char*name){char path[persist::kPath];const int n=std::snprintf(path,sizeof(path),"%s/%s",folder,name);if(n<=0||n>=int(sizeof(path))){model.erase(destination);return;}if(!persist::importInstrument(io,model,scratch,destination,path)){LOG("WR2: no valid portable instrument groups");}}
void abort(){service.abort();}
}
#endif
