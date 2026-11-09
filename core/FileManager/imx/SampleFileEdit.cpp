#if TRACKER_CE_ENABLE_SD_MONO || TRACKER_CE_ENABLE_SD_EDITS
#include "SampleFileEdit.h"
#include "SamplePreview.h"
#include "SamplePreviewEffects.h"
#include "SampleEditNotifications.h"
#include "mtStructs.h"
#include "fileManager.h"
#include "fileManagerDefs.h"
#include "VoiceStreamManager.h"
#include "mtAudioEngine.h"
#include "mtSequencer.h"
#include "sampleProcessing/Conversion.h"
#if TRACKER_CE_ENABLE_SD_EDITS
#include "sampleProcessing/NativeDrive.h"
#endif
#include "../../../platforms/imx/fatfs/source/ff.h"
#include "mtEffect.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <new>
#include <cmath>
// Foreground-only transaction; all PCM/IO preparation precedes live cache mutation.
// The existing Sample Editor arena owns this job until result() releases it.
namespace sampleedit { namespace {
const char*message="";
struct Paths{char main[256],temp[256],backup[256],pending[256],done[256],undo[256],undoNew[256],undoInfo[256],undoInfoNew[256];};
void paths(Paths&p,unsigned slot){std::snprintf(p.main,256,cWorkspaceInstrumentFileFormatNew,slot+1);std::snprintf(p.temp,256,"%s.mono.tmp",p.main);std::snprintf(p.backup,256,"%s.mono.bak",p.main);std::snprintf(p.pending,256,"%s.mono.pending",p.main);std::snprintf(p.done,256,"%s.mono.done",p.main);std::snprintf(p.undo,256,"%s.mono.undo",p.main);std::snprintf(p.undoNew,256,"%s.mono.undo.new",p.main);std::snprintf(p.undoInfo,256,"%s.mono.undo.info",p.main);std::snprintf(p.undoInfoNew,256,"%s.mono.undo.info.new",p.main);}
int exists(const char*p){FILINFO info{};auto r=f_stat(p,&info);if(r==FR_NO_FILE||r==FR_NO_PATH)return 0;return r==FR_OK&&!(info.fattrib&AM_DIR)?1:-1;}
bool remove(const char*p){const auto found=exists(p);return found==0||(found==1&&f_unlink(p)==FR_OK);}
bool exact(FIL&f,uint32_t offset,void*data,unsigned n){UINT got=0;return f_lseek(&f,offset)==FR_OK&&f_read(&f,data,n,&got)==FR_OK&&got==n;}
bool marker(const char*p,const char*magic,uint32_t frames){uint8_t b[16]{};std::memcpy(b,magic,8);std::memcpy(b+8,&frames,4);const auto c=crc(b,12);std::memcpy(b+12,&c,4);FIL f{};if(f_open(&f,p,FA_WRITE|FA_CREATE_ALWAYS)!=FR_OK)return false;UINT put=0;bool ok=f_write(&f,b,16,&put)==FR_OK&&put==16;if(f_sync(&f)!=FR_OK)ok=false;if(f_close(&f)!=FR_OK)ok=false;uint8_t check[16];if(!ok||f_open(&f,p,FA_READ)!=FR_OK)return false;ok=f_size(&f)==16&&exact(f,0,check,16)&&!std::memcmp(b,check,16);if(f_close(&f)!=FR_OK)ok=false;return ok;}
struct UndoDescriptor{char magic[8];uint32_t bytes,contentCrc,frames,channels,check;};
struct UndoContext{bool valid=false,streamed=false;unsigned slot=0,effect=0xffffffffu;uint32_t ramFrames=0,sampleHash=0,instrumentCrc=0;uint16_t start=0,end=65535;};
UndoContext undoContext{};
bool readMarker(const char*p,const char*magic,uint32_t*frames=nullptr){FIL f{};if(f_open(&f,p,FA_READ)!=FR_OK)return false;uint8_t b[16]{};bool ok=f_size(&f)==16&&exact(f,0,b,16);if(f_close(&f)!=FR_OK)ok=false;uint32_t check=0,n=0;std::memcpy(&check,b+12,4);std::memcpy(&n,b+8,4);if(!ok||std::memcmp(b,magic,8)||check!=crc(b,12))return false;if(frames)*frames=n;return true;}
bool writeUndoDescriptor(const char*p,UndoDescriptor&d){std::memcpy(d.magic,"SEUNDO01",8);d.check=crc(reinterpret_cast<uint8_t*>(&d),sizeof(d)-4);FIL f{};if(f_open(&f,p,FA_WRITE|FA_CREATE_ALWAYS)!=FR_OK)return false;UINT put=0;bool ok=f_write(&f,&d,sizeof(d),&put)==FR_OK&&put==sizeof(d)&&f_sync(&f)==FR_OK;if(f_close(&f)!=FR_OK)ok=false;UndoDescriptor check{};if(!ok||f_open(&f,p,FA_READ)!=FR_OK)return false;ok=f_size(&f)==sizeof(check)&&exact(f,0,&check,sizeof(check));if(f_close(&f)!=FR_OK)ok=false;return ok&&!std::memcmp(&d,&check,sizeof(d));}
bool readUndoDescriptor(const char*p,UndoDescriptor&d){FIL f{};if(f_open(&f,p,FA_READ)!=FR_OK)return false;bool ok=f_size(&f)==sizeof(d)&&exact(f,0,&d,sizeof(d));if(f_close(&f)!=FR_OK)ok=false;return ok&&!std::memcmp(d.magic,"SEUNDO01",8)&&d.frames&&d.channels>=1&&d.channels<=2&&d.check==crc(reinterpret_cast<uint8_t*>(&d),sizeof(d)-4)&&d.bytes==sizeof(strInstrumentFile)+uint64_t(d.frames)*d.channels*2;}
bool committed(const Paths&p,const char*magic){uint32_t frames=0;if(!readMarker(p.done,magic,&frames))return false;FILINFO info{};return f_stat(p.main,&info)==FR_OK&&uint64_t(info.fsize)==sizeof(strInstrumentFile)+uint64_t(frames)*2;}
bool finalizeApplyRecovery(const Paths&p){
 UndoDescriptor d{};const auto descriptorState=exists(p.undoInfoNew);if(descriptorState<0)return false;
 const bool descriptorInNew=descriptorState==1;
 if(!readUndoDescriptor(descriptorInNew?p.undoInfoNew:p.undoInfo,d))return false;
 if(uint64_t(d.bytes)!=uint64_t(sizeof(strInstrumentFile))+uint64_t(d.frames)*d.channels*2)return false;
 if(exists(p.undoNew)==1){if(!remove(p.undo)||f_rename(p.undoNew,p.undo)!=FR_OK)return false;}
 else if(exists(p.undo)!=1)return false;
 if(descriptorInNew&&(!remove(p.undoInfo)||f_rename(p.undoInfoNew,p.undoInfo)!=FR_OK))return false;
 return remove(p.temp)&&remove(p.backup)&&remove(p.done)&&remove(p.pending);
}
bool recoverOne(unsigned slot,bool startup=false){Paths p;paths(p,slot);int pending=exists(p.pending);if(pending<0)return false;
 if(!pending){if(!remove(p.temp)||!remove(p.done)||!remove(p.backup)||!remove(p.undoNew)||!remove(p.undoInfoNew))return false;if(startup)return remove(p.undo)&&remove(p.undoInfo);return true;}
 uint8_t op[16]{};FIL f{};const bool pendingOpened=f_open(&f,p.pending,FA_READ)==FR_OK;bool pendingRead=pendingOpened&&f_size(&f)==16&&exact(f,0,op,16);if(pendingOpened&&f_close(&f)!=FR_OK)pendingRead=false;
 if(!pendingRead){const auto backup=exists(p.backup),main=exists(p.main);if(backup!=0||main!=1)return false;if(!remove(p.pending)||!remove(p.temp)||!remove(p.done)||!remove(p.undoNew)||!remove(p.undoInfoNew))return false;return startup?(remove(p.undo)&&remove(p.undoInfo)):true;}
 const bool isUndo=!std::memcmp(op,"UNDOPEND",8);const bool isApply=!std::memcmp(op,"MONOPEND",8);if(!isUndo&&!isApply)return false;
 if(isApply&&committed(p,"MONODONE")){if(!finalizeApplyRecovery(p))return false;return startup?(remove(p.undo)&&remove(p.undoInfo)):true;}
 if(isUndo&&committed(p,"UNDODONE")){if(!remove(p.backup)||!remove(p.undo)||!remove(p.undoInfo)||!remove(p.temp)||!remove(p.pending)||!remove(p.done))return false;return true;}
 const auto backup=exists(p.backup);if(backup<0)return false;
 if(isUndo){
  const auto undo=exists(p.undo);
  if(undo<0)return false;
  if(backup&& !undo && exists(p.main)==1 && f_rename(p.main,p.undo)!=FR_OK)return false;
  if(backup&&!(remove(p.main)&&f_rename(p.backup,p.main)==FR_OK))return false;
 }else if(backup&&!(remove(p.main)&&f_rename(p.backup,p.main)==FR_OK))return false;
 if(!remove(p.temp)||!remove(p.done)||!remove(p.pending)||!remove(p.undoNew)||!remove(p.undoInfoNew))return false;
 return startup?(remove(p.undo)&&remove(p.undoInfo)):true;
}
enum class Step{Scan,Mix,Precache,Peaks,Slices,UndoScan,Promote,Ram,Finish,UndoValidate,UndoPrepareDone,UndoCommit,UndoSwap,UndoRollback};
struct Job final:IO{
 unsigned slot;Mode mode;float monoGain;uint32_t monoFirst,monoLast;bool completed=false;strInstrument original;strSampleMemory old;strInstrumentFile header{};Paths p;char source[256]{};
 FIL inputFile{},outputFile{},reader{};bool inputOpen=false,outputOpen=false,readerOpen=false,blocked=false,promoting=false;
 VoiceStreamManager::MonoCache cache{};Blocks blocks;Conversion conversion;Step step=Step::Mix;
 uint32_t cursor=0,peak=0,slice=0,slicePos=0;bool finished=false,undoing=false,undoCompleted=false,cancelRequested=false,memoryShifted=false,undoScanOpen=false,rollbackOpen=false;
 unsigned effectIndex=0xffffffffu;uint16_t appliedSelectionStart=0,appliedSelectionEnd=65535;uint32_t undoScanOffset=0,undoScanCrc=0,restoreFrames=0,restoreRamFrames=0,restoreCursor=0,currentRestoreCursor=0;unsigned restoreChannels=1,restoreChannel=0,currentRestoreChannel=0;bool restoreStreamed=false,restoreStereo=false,undoDescriptorValid=false;int32_t memoryDelta=0;UndoDescriptor undoDescriptor{};strInstrument restoreInstrument{};FIL rollbackFile{};
 const char*rollbackMessage="Undo cancelled; original preserved.";bool undoSnapshotMoved=false,undoFilesPromoted=false;uint32_t currentRamFrames=0;
#if TRACKER_CE_ENABLE_SD_EDITS
 const samplepreview::EffectCopy* nativeSpec=nullptr;mtEffect* nativeSeed=nullptr;mtEffect* nativeDsp=nullptr;mtEffect* nativeOwner=nullptr;void* nativeWorking=nullptr;
 ~Job(){if(nativeDsp)nativeSpec->destroy(nativeDsp);if(nativeSeed)nativeSpec->destroy(nativeSeed);}
 bool resetNative(){if(!nativeSpec)return true;if(nativeDsp)nativeSpec->destroy(nativeDsp);nativeDsp=nativeSpec->copy(nativeWorking,*nativeSeed);int16_t* empty[2]{};nativeDsp->bindPreviewBuffers(empty,empty,last-first);return nativeSpec->start(nativeDsp,nullptr,old.isStereo);}
 void nativeBlock(unsigned ch,uint32_t at,unsigned n,int16_t* data){const auto a=std::max(at,first),b=std::min(at+n,last);if(a<b){if(channels()==1){std::memcpy(blocks.right,data+a-at,(b-a)*2);nativeSpec->processChannel(nativeDsp,1,blocks.right,blocks.right,b-a);}nativeSpec->processChannel(nativeDsp,ch,data+a-at,data+a-at,b-a);}}
 float amplitudes[2]{1,1};int peakValues[2]{};float parameter=1,secondary=0;uint32_t scanPos=0;
 CrushState crusher[2]{};uint16_t sampleStep=1;
 Edit edit=Edit::None;strInstrument replacement{};VoiceStreamManager::MonoCache rightCache{};
 uint32_t first=0,last=0,editPos=0,expected=0xffffffffu,checked=0xffffffffu;unsigned fileChannel=0;bool verifying=false;
 uint32_t frames()const{
#if TRACKER_CE_ENABLE_SD_EDITS
  return undoing?restoreFrames:(edit==Edit::None?original.sample.length:replacement.sample.length);
#else
  return undoing?restoreFrames:original.sample.length;
#endif
 }
 unsigned channels()const{
  if(undoing)return restoreChannels;
#if TRACKER_CE_ENABLE_SD_EDITS
  return edit!=Edit::None&&old.isStereo?2:1;
#else
  return 1;
#endif
 }
 bool streamedTarget()const{return undoing?restoreStreamed:old.isStreamed;}
#else
 Edit edit=Edit::None;VoiceStreamManager::MonoCache rightCache{};unsigned fileChannel=0;
 uint32_t frames()const{return undoing?restoreFrames:original.sample.length;}
 unsigned channels()const{return undoing?restoreChannels:1;}
 bool streamedTarget()const{return undoing?restoreStreamed:old.isStreamed;}
#endif
 uint32_t fileOffset(uint32_t frame)const{
#if TRACKER_CE_ENABLE_SD_EDITS
  return sizeof(header)+(fileChannel*frames()+frame)*2;
#else
  return sizeof(header)+(fileChannel*frames()+frame)*2;
#endif
 }
 Job(unsigned i,Mode choice,float db,uint32_t a,uint32_t b):slot(i),mode(choice),monoGain(db==0.0f?1.0f:std::pow(10.0f,db/20.0f)),monoFirst(a),monoLast(b),original(mtProject.instrument[i]),old(mtProject.sampleMemory[i]),conversion(*this,blocks,original.sample.length,mode,monoGain,old.isStereo,a,b){std::memcpy(&original,&mtProject.instrument[i],sizeof(original));std::memcpy(&old,&mtProject.sampleMemory[i],sizeof(old));paths(p,i);}
 bool input(unsigned channel,uint32_t frame,unsigned count,int16_t*out)override{
  if(!old.isStreamed){std::memcpy(out,old.address[channel]+frame,count*2);return true;}
  const uint32_t off=old.fileDataOffset+(channel?original.sample.length*2:0)+frame*2;return exact(inputFile,off,out,count*2);
 }
 bool output(uint32_t frame,unsigned count,const int16_t*data)override{UINT put=0;return f_lseek(&outputFile,fileOffset(frame))==FR_OK&&f_write(&outputFile,data,count*2,&put)==FR_OK&&put==count*2;}
 bool seal()override{
  bool ok=f_sync(&outputFile)==FR_OK;if(f_close(&outputFile)!=FR_OK)ok=false;outputOpen=false;
  if(inputOpen){if(f_close(&inputFile)!=FR_OK)ok=false;inputOpen=false;}
  if(!ok||f_open(&reader,p.temp,FA_READ)!=FR_OK)return false;
  readerOpen=true;strInstrumentFile check;
  return uint64_t(f_size(&reader))==sizeof(header)+uint64_t(frames())*channels()*2&&exact(reader,0,&check,sizeof(check))&&!std::memcmp(&check,&header,sizeof(check));
 }
 bool verify(uint32_t frame,unsigned count,int16_t*out)override{return exact(reader,fileOffset(frame),out,count*2);}
 bool close(){bool ok=true;if(inputOpen){ok=f_close(&inputFile)==FR_OK&&ok;inputOpen=false;}if(outputOpen){ok=f_close(&outputFile)==FR_OK&&ok;outputOpen=false;}if(readerOpen){ok=f_close(&reader)==FR_OK&&ok;readerOpen=false;}return ok;}
 void shiftFollowing(int32_t delta){if(!delta)return;const unsigned firstSlot=newFileManager.calcFirstSlotToMoveInMemory(slot+1);if(firstSlot>=48)return;uint64_t oldBlocks=0,oldNotes=0;for(unsigned i=firstSlot;i<48;++i){if(isBlockedInstrument[i])oldBlocks|=uint64_t(1)<<i;if(isBlockedNoteOn[i])oldNotes|=uint64_t(1)<<i;engine.setBlockedNoteOnFromInstrument(i);}for(unsigned v=0;v<8;++v)if(instrumentPlayer[v].currentInstrument_idx>=firstSlot&&instrumentPlayer[v].currentInstrument_idx<48)instrumentPlayer[v].noteOff(Sequencer::STEP_NOTE_OFF_CUT);newFileManager.moveSampleMemory(firstSlot,delta);for(unsigned i=firstSlot;i<48;++i){if(oldBlocks&(uint64_t(1)<<i))engine.setBlockedInstrument(i);if(!(oldNotes&(uint64_t(1)<<i)))engine.clearBlockedNoteOnFromInstrument(i);}}
 bool rollbackUndoFiles(){
  if(exists(p.backup)!=1)return !undoFilesPromoted;
  if(exists(p.main)==1&&exists(p.undo)==0){if(f_rename(p.main,p.undo)!=FR_OK)return false;undoSnapshotMoved=false;}
  if(!remove(p.main)||f_rename(p.backup,p.main)!=FR_OK)return false;
  remove(p.done);remove(p.pending);undoFilesPromoted=false;return true;
 }
 void beginUndoRollback(const char*why){rollbackMessage=why;close();if(!rollbackUndoFiles()){message="Undo interrupted; restart with the SD card inserted to recover.";finished=true;completed=false;return;}step=Step::UndoRollback;currentRestoreChannel=0;currentRestoreCursor=0;rollbackOpen=false;}
 void tickUndoRollback(){
  if(!rollbackOpen){if(f_open(&rollbackFile,p.main,FA_READ)!=FR_OK){message="Undo rollback needs the SD card; instrument remains blocked until restart.";finished=true;completed=false;return;}rollbackOpen=true;}
  const unsigned n=old.isStreamed?std::min<uint32_t>(currentRamFrames,original.sample.length):original.sample.length;
  const unsigned count=std::min<uint32_t>(kBlock,n-currentRestoreCursor);auto*dst=old.address[currentRestoreChannel];const uint32_t off=sizeof(strInstrumentFile)+(currentRestoreChannel?original.sample.length*2:0)+currentRestoreCursor*2;
  if(!exact(rollbackFile,off,dst+currentRestoreCursor,count*2)){f_close(&rollbackFile);rollbackOpen=false;message="Undo rollback read failed; restart with the SD card inserted.";finished=true;completed=false;return;}
  currentRestoreCursor+=count;if(currentRestoreCursor<n)return;currentRestoreCursor=0;if(++currentRestoreChannel<(old.isStereo?2u:1u))return;
  f_close(&rollbackFile);rollbackOpen=false;if(memoryShifted&&memoryDelta>0){shiftFollowing(-memoryDelta);memoryShifted=false;}unblock();message=rollbackMessage;finished=true;completed=false;
 }
 void beginUndoFailure(const char*why){beginUndoRollback(why);}
 void tickUndoScan(){
  if(!undoScanOpen){
   undoDescriptor.bytes=sizeof(strInstrumentFile)+original.sample.length*(old.isStereo?4u:2u);undoDescriptor.frames=original.sample.length;undoDescriptor.channels=old.isStereo?2u:1u;
   if(old.isStreamed){if(f_open(&inputFile,source,FA_READ)!=FR_OK){fail("Cannot read Undo PCM source; original preserved.");return;}inputOpen=true;}
   if(f_open(&outputFile,p.undoNew,FA_WRITE|FA_CREATE_NEW)!=FR_OK){fail("Cannot create Undo snapshot; original preserved.");return;}outputOpen=true;
   strInstrumentFile snapshotHeader{};newFileManager.writeInstrumentToFileStruct(&original,&snapshotHeader);UINT put=0;
   if(f_write(&outputFile,&snapshotHeader,sizeof(snapshotHeader),&put)!=FR_OK||put!=sizeof(snapshotHeader)){fail("Cannot write Undo header; original preserved.");return;}
   undoScanCrc=crc(&snapshotHeader,sizeof(snapshotHeader));undoScanOffset=sizeof(snapshotHeader);undoScanOpen=true;return;
  }
  if(!undoDescriptorValid){
   if(undoScanOffset<undoDescriptor.bytes){const uint32_t pcm=(undoScanOffset-sizeof(strInstrumentFile))/2,ch=pcm/original.sample.length,at=pcm%original.sample.length;const unsigned n=std::min<uint32_t>(kBlock,original.sample.length-at);UINT put=0;
    if(!input(ch,at,n,blocks.left)||f_write(&outputFile,blocks.left,n*2,&put)!=FR_OK||put!=n*2){fail("Cannot protect Undo PCM; original preserved.");return;}undoScanCrc=crc(blocks.left,n*2,undoScanCrc);undoScanOffset+=n*2;return;
   }
   const bool synced=f_sync(&outputFile)==FR_OK;if(!close()||!synced){fail("Cannot close Undo snapshot; original preserved.");return;}
   undoDescriptor.contentCrc=undoScanCrc;if(f_open(&reader,p.undoNew,FA_READ)!=FR_OK){fail("Cannot verify Undo snapshot; original preserved.");return;}readerOpen=true;
   if(f_size(&reader)!=undoDescriptor.bytes){fail("Undo snapshot size changed; original preserved.");return;}undoDescriptorValid=true;undoScanOffset=0;undoScanCrc=0xffffffffu;return;
  }
  if(undoScanOffset<undoDescriptor.bytes){const unsigned count=std::min<uint32_t>(sizeof(blocks.left),undoDescriptor.bytes-undoScanOffset);if(!exact(reader,undoScanOffset,blocks.left,count)){fail("Cannot verify Undo snapshot; original preserved.");return;}undoScanCrc=crc(blocks.left,count,undoScanCrc);undoScanOffset+=count;return;}
  if(undoScanCrc!=undoDescriptor.contentCrc||!close()){fail("Undo snapshot checksum failed; original preserved.");return;}
  if(!writeUndoDescriptor(p.undoInfoNew,undoDescriptor)){fail("Cannot protect Undo snapshot; original preserved.");return;}
  step=Step::Promote;
 }
 void tickUndoValidation(){
  if(undoScanOffset>=undoDescriptor.bytes){if(undoScanCrc!=undoDescriptor.contentCrc){fail("Undo snapshot checksum failed; original preserved.");return;}step=Step::Precache;cursor=peak=slice=slicePos=0;fileChannel=0;return;}
  const unsigned count=std::min<uint32_t>(sizeof(blocks.left),undoDescriptor.bytes-undoScanOffset);if(!exact(reader,undoScanOffset,blocks.left,count)){fail("Cannot read Undo snapshot; original preserved.");return;}undoScanCrc=crc(blocks.left,count,undoScanCrc);undoScanOffset+=count;
 }
 void tickUndoCommit(){
  if(cancelRequested){beginUndoRollback("Undo cancelled; original preserved.");return;}
  if(!blocked){engine.setBlockedNoteOnFromInstrument(slot);engine.setBlockedInstrument(slot);blocked=true;for(unsigned v=0;v<8;++v)if(instrumentPlayer[v].currentInstrument_idx==slot)instrumentPlayer[v].noteOff(Sequencer::STEP_NOTE_OFF_CUT);voiceStreamManager.invalidateInstrumentVoices(slot);
   if(memoryDelta>0){shiftFollowing(memoryDelta);memoryShifted=true;}
   if(restoreStreamed){restoreRamFrames=std::min<uint32_t>(restoreRamFrames,restoreFrames);}
   else restoreRamFrames=restoreFrames;
   if(restoreStereo)mtProject.sampleMemory[slot].address[1]=old.address[0]+restoreRamFrames;
   if(restoreRamFrames>restoreFrames||!restoreRamFrames){beginUndoRollback("Invalid Undo memory layout; original preserved.");return;}
   if(!restoreStreamed){if(f_open(&reader,p.undo,FA_READ)!=FR_OK){beginUndoRollback("Cannot read Undo snapshot; original preserved.");return;}readerOpen=true;}
  }
  const unsigned n=restoreRamFrames;const unsigned count=std::min<uint32_t>(kBlock,n-restoreCursor);const int16_t*src=nullptr;int16_t*dst=old.address[0]+restoreChannel*restoreRamFrames+restoreCursor;
  if(restoreStreamed){src=(restoreChannel?rightCache.samples:cache.samples)+restoreCursor;std::memcpy(dst,src,count*2);}
  else{const uint32_t off=sizeof(strInstrumentFile)+(restoreChannel?restoreFrames*2:0)+restoreCursor*2;if(!exact(reader,off,dst,count*2)){beginUndoRollback("Undo read failed; original preserved.");return;}}
  restoreCursor+=count;if(restoreCursor<n)return;restoreCursor=0;if(++restoreChannel<restoreChannels)return;
  if(readerOpen){if(f_close(&reader)!=FR_OK){readerOpen=false;beginUndoRollback("Undo read failed; original preserved.");return;}readerOpen=false;}
  step=Step::UndoSwap;
 }
 void tickUndoSwap(){
  if(!metadataReady(slot)){beginUndoFailure("Instrument metadata unavailable; original preserved.");return;}
  if(!marker(p.pending,"UNDOPEND",restoreFrames*restoreChannels)){beginUndoFailure("Cannot protect Undo transaction; original preserved.");return;}
  if(f_rename(p.main,p.backup)!=FR_OK){remove(p.pending);beginUndoFailure("Cannot protect current sample; Undo cancelled.");return;}
  if(f_rename(p.undo,p.main)!=FR_OK){undoFilesPromoted=true;beginUndoFailure("Undo commit failed; original sample restored.");return;}
  undoSnapshotMoved=true;undoFilesPromoted=true;
  if(!marker(p.done,"UNDODONE",restoreFrames*restoreChannels)){beginUndoFailure("Undo commit failed; original sample restored.");return;}
  const int32_t shrink=memoryDelta<0?memoryDelta:0;
  if(shrink)shiftFollowing(shrink);
  const auto address=mtProject.instrument[slot].sample.address;mtProject.instrument[slot]=restoreInstrument;mtProject.instrument[slot].sample.address=address;
  voiceStreamManager.installMonoCache(slot,p.main,sizeof(strInstrumentFile),cache,restoreStreamed,restoreFrames,restoreChannels==2?&rightCache:nullptr);
  mtProject.used_memory=newFileManager.getSampleMemorySize(0,47);newFileManager.setInstrumentStructChanged(slot);newFileManager.clearSampleChanged(slot);
  metadataChanged(slot,original.sample.length!=restoreInstrument.sample.length);
  undoContext.valid=false;unblock();undoCompleted=true;completed=true;finished=true;step=Step::Finish;
  const bool clean=remove(p.backup)&&remove(p.undo)&&remove(p.undoInfo)&&remove(p.undoNew)&&remove(p.undoInfoNew)&&remove(p.temp)&&remove(p.done)&&remove(p.pending);
  message=clean?"":"Undo applied; recovery files retained.";
 }
 void unblock(){if(blocked){engine.clearBlockedInstrument(slot);engine.clearBlockedNoteOnFromInstrument(slot);blocked=false;}newFileManager.setWorkspaceSaveSuspended(false);}
 void fail(const char*why){close();bool restored=true;if(promoting){const auto backup=exists(p.backup),main=exists(p.main);restored=backup==1?(remove(p.main)&&f_rename(p.backup,p.main)==FR_OK):(backup==0&&main==1);if(restored){remove(p.done);remove(p.pending);}if(old.isStreamed&&!std::strcmp(source,p.main)&&exists(p.backup)==1)std::snprintf(voiceStreamManager.getInstrumentStreamPath(slot),256,"%s",p.backup);}remove(p.temp);remove(p.undoNew);remove(p.undoInfoNew);if(!restored){message="Commit recovery required; instrument remains blocked until restart.";finished=true;completed=false;return;}unblock();message=why;finished=true;completed=false;}
 bool prepare(){
#if TRACKER_CE_ENABLE_SD_EDITS
  auto&prepared=fileChannel?rightCache:cache;const auto&instrument=undoing?restoreInstrument:(edit==Edit::None?original:replacement);
#else
  auto&prepared=fileChannel?rightCache:cache;const auto&instrument=undoing?restoreInstrument:original;
#endif
  const auto n=frames(),pre=std::min<uint32_t>(n,kStreamPreCacheFrames);
  if(step==Step::Precache){if(cursor<pre){const unsigned count=std::min<uint32_t>(kBlock,pre-cursor);if(!verify(cursor,count,prepared.samples+cursor))return false;cursor+=count;return true;}
   uint32_t h=2166136261u;h=(h^n)*16777619u;h=(h^1u)*16777619u;const auto stride=pre>256?pre/256:1;
   for(uint32_t i=0;i<pre;i+=stride){h=(h^uint16_t(prepared.samples[i]))*16777619u;}
   prepared.hash=h?h:1;prepared.linearFrames=pre;step=Step::Peaks;return true;}
  if(step==Step::Peaks){if(peak<kWaveformPeakPoints){uint32_t a=uint64_t(peak)*n/kWaveformPeakPoints,b=uint64_t(peak+1)*n/kWaveformPeakPoints;if(b<=a)b=a+1;if(b>n)b=n;const bool inRam=b<=pre;const unsigned count=std::min<uint32_t>(b-a,inRam?200:64);const int16_t*data;
    if(inRam)data=prepared.samples+a;else{if(!verify(a,count,blocks.left))return false;data=blocks.left;}
    int16_t low=0,high=0;for(unsigned i=0;i<count;++i){low=std::min(low,data[i]);high=std::max(high,data[i]);}
    prepared.peaks[peak++]={int16_t(n<600?data[0]/300:low/300),int16_t(n<600?data[0]/300:high/300)};return true;}
   prepared.sliced=streamedTarget()&&pre==kStreamPreCacheFrames&&(instrument.playMode==playModeSlice||instrument.playMode==playModeBeatSlice)&&instrument.sliceNumber>0&&instrument.sliceNumber<=48;
   if(prepared.sliced){prepared.linearFrames=std::min<uint32_t>(n,kStreamPreCacheFrames/2);}
   step=Step::Slices;return true;}
  if(step==Step::Slices){if(prepared.sliced&&slice<instrument.sliceNumber){const uint32_t half=kStreamPreCacheFrames/2;uint32_t chunk=(half/instrument.sliceNumber)&~15u;if(!chunk)chunk=half/instrument.sliceNumber;
    uint32_t start=uint32_t(float(instrument.slices[slice])*(float(n)/MAX_16BIT));if(start>=n)start=n-1;const auto count=std::min<uint32_t>(chunk,n-start);
    if(start+count<=prepared.linearFrames){prepared.slices[slice]={start,uint16_t(std::min<uint32_t>(prepared.linearFrames-start,65535)),uint16_t(start)};++slice;return true;}
    const unsigned off=half+slice*chunk,part=std::min<uint32_t>(kBlock,count-slicePos);
    if(!verify(start+slicePos,part,prepared.samples+off+slicePos))return false;
    slicePos+=part;
    if(slicePos==count){prepared.slices[slice]={start,uint16_t(count),uint16_t(off)};slicePos=0;++slice;}return true;}
   if(fileChannel+1<channels()){++fileChannel;cursor=peak=slice=slicePos=0;step=Step::Precache;return true;}
   if(streamedTarget()){cache.clmt[0]=kClmtElementCount;reader.cltbl=reinterpret_cast<DWORD*>(cache.clmt);if(f_lseek(&reader,CREATE_LINKMAP)!=FR_OK)return false;cache.databaseSector=reader.obj.fs->database;cache.sectorsPerCluster=reader.obj.fs->csize;}
   if(!close())return false;
   step=undoing?Step::UndoCommit:Step::UndoScan;return true;}
  return false;
 }
#if TRACKER_CE_ENABLE_SD_EDITS
 bool editBlock(){
  const auto n=frames();unsigned count=std::min<uint32_t>(kBlock,n-editPos);
  if(verifying){if(!verify(editPos,count,blocks.left))return false;checked=crc(blocks.left,count*2,checked);}
  else{
   uint32_t sourceFrame=edit==Edit::Crop?first+editPos:editPos;
   if(edit==Edit::Reverse){if(editPos<first)count=std::min<uint32_t>(count,first-editPos);else if(editPos<last){count=std::min<uint32_t>(count,last-editPos);sourceFrame=first+last-editPos-count;}}
   if(!input(fileChannel,sourceFrame,count,blocks.left))return false;
   if(edit==Edit::Reverse&&editPos>=first&&editPos<last)std::reverse(blocks.left,blocks.left+count);
   if(edit==Edit::FadeIn||edit==Edit::FadeOut)for(unsigned j=0;j<count;++j){const auto at=editPos+j;if(at>=first&&at<last){const float progress=float(at-first)/float(last-first);const float gain=edit==Edit::FadeIn?progress:1.0f-progress;blocks.left[j]=int16_t(blocks.left[j]*gain);}}
   if(edit==Edit::Normalize||edit==Edit::Amplifier)for(unsigned j=0;j<count;++j)if(editPos+j>=first&&editPos+j<last)blocks.left[j]=ce_pcm_scale(blocks.left[j],amplitudes[fileChannel]);
   if(edit==Edit::Overdrive||edit==Edit::Bitcrusher)for(unsigned j=0;j<count;++j){const auto at=editPos+j;if(at>=first&&at<last)blocks.left[j]=edit==Edit::Overdrive?overdriveSample(blocks.left[j],amplitudes[fileChannel],unsigned(secondary)):crushSample(blocks.left[j],unsigned(parameter),sampleStep,at-first,crusher[fileChannel]);}
   if(nativeSpec)nativeBlock(fileChannel,editPos,count,blocks.left);
   expected=crc(blocks.left,count*2,expected);if(!output(editPos,count,blocks.left))return false;
  }
  editPos+=count;if(editPos<n)return true;editPos=0;
  if(++fileChannel<channels())return true;
  fileChannel=0;
  if(!verifying){if(!seal())return false;verifying=true;}
  else{if(checked!=expected)return false;step=Step::Precache;cursor=0;}
  return true;
 }
#endif

 void tick(){
  if(step==Step::UndoValidate){tickUndoValidation();return;}
  if(step==Step::UndoScan){tickUndoScan();return;}
  if(step==Step::UndoCommit){tickUndoCommit();return;}
  if(step==Step::UndoSwap){tickUndoSwap();return;}
  if(step==Step::UndoRollback){tickUndoRollback();return;}
#if TRACKER_CE_ENABLE_SD_EDITS
  if(step==Step::Scan){
   const auto count=std::min<uint32_t>(kBlock,last-scanPos);if(!input(fileChannel,scanPos,count,blocks.left)){fail("Cannot read normalization selection.");return;}
   for(unsigned i=0;i<count;++i)peakValues[fileChannel]=std::max(peakValues[fileChannel],ce_pcm_peak(blocks.left[i]));
   scanPos+=count;if(scanPos==last){amplitudes[fileChannel]=ce_pcm_normalize_gain(int(parameter),peakValues[fileChannel]);scanPos=first;if(++fileChannel==channels()){fileChannel=0;step=Step::Mix;}}
   return;
  }
#endif
  if(step==Step::Mix){
#if TRACKER_CE_ENABLE_SD_EDITS
   if(edit!=Edit::None){if(!editBlock())fail("IO/validation error; original preserved.");return;}
#endif
   conversion.tick();if(conversion.phase()==Phase::Failed){fail("IO/validation error; original preserved.");return;}if(conversion.phase()==Phase::Ready){step=Step::Precache;cursor=0;}return;}
  if(step==Step::Precache||step==Step::Peaks||step==Step::Slices){if(!prepare())fail("Preparation failed; original preserved.");return;}
  if(step==Step::Promote){
   if(std::memcmp(&original,&mtProject.instrument[slot],sizeof(original))){fail("Instrument changed; conversion cancelled.");return;}
   if(!metadataReady(slot)){fail("Instrument metadata unavailable.");return;}
   if(!remove(p.backup)||!remove(p.done)||!marker(p.pending,"MONOPEND",frames()*channels())){fail("Cannot protect original file.");return;}
   engine.setBlockedNoteOnFromInstrument(slot);engine.setBlockedInstrument(slot);blocked=true;
   for(unsigned v=0;v<8;++v)if(instrumentPlayer[v].currentInstrument_idx==slot)instrumentPlayer[v].noteOff(Sequencer::STEP_NOTE_OFF_CUT);
   voiceStreamManager.invalidateInstrumentVoices(slot);promoting=true;
   if(f_rename(p.main,p.backup)!=FR_OK||f_rename(p.temp,p.main)!=FR_OK||!marker(p.done,"MONODONE",frames()*channels())){fail("Commit failed; original retained for recovery.");return;}
   cursor=0;
#if TRACKER_CE_ENABLE_SD_EDITS
   crusher[0]=CrushState{};crusher[1]=CrushState{};
   fileChannel=0;
   if(nativeSpec&&!old.isStreamed)resetNative(); // Same captured initial state for infallible RAM installation.
#endif
   step=Step::Ram;return;
  }
  // No fallible PCM IO follows a successfully promoted Apply.
  if(step==Step::Ram){
   const uint32_t n=frames(),oldFrames=old.ramFrames?old.ramFrames:original.sample.length,newFrames=old.isStreamed?std::min<uint32_t>(n,kStreamPreCacheFrames):n;
   if(old.isStreamed){std::memcpy(old.address[0],cache.samples,newFrames*2);
#if TRACKER_CE_ENABLE_SD_EDITS
    if(channels()==2)std::memcpy(old.address[0]+newFrames,rightCache.samples,newFrames*2);
#endif
   }else{
#if TRACKER_CE_ENABLE_SD_EDITS
    if(edit!=Edit::None){
     // Bounded, infallible RAM installation only after the file transaction commits.
     const auto end=std::min<uint32_t>(n,cursor+(nativeSpec?kBlock:32*kBlock));
     if(nativeSpec){for(unsigned ch=0;ch<channels();++ch)nativeBlock(ch,cursor,end-cursor,old.address[ch]+cursor);cursor=end;if(cursor<n)return;}
     else if(edit==Edit::Crop){std::memmove(old.address[0]+fileChannel*n+cursor,old.address[fileChannel]+first+cursor,(end-cursor)*2);cursor=end;if(cursor<n)return;cursor=0;if(++fileChannel<channels())return;}
     else{
     for(unsigned ch=0;ch<channels();++ch){auto*data=old.address[ch];for(uint32_t j=cursor;j<end;++j){if(edit==Edit::Reverse){if(j>=first&&j<first+(last-first)/2)std::swap(data[j],data[first+last-1-j]);}
       else if(j>=first&&j<last){if(edit==Edit::Overdrive||edit==Edit::Bitcrusher)data[j]=edit==Edit::Overdrive?overdriveSample(data[j],amplitudes[ch],unsigned(secondary)):crushSample(data[j],unsigned(parameter),sampleStep,j-first,crusher[ch]);else if(edit==Edit::Normalize||edit==Edit::Amplifier)data[j]=ce_pcm_scale(data[j],amplitudes[ch]);else{const float f=float(j-first)/float(last-first);data[j]=int16_t(data[j]*(edit==Edit::FadeIn?f:1.0f-f));}}}
     }
     cursor=end;if(cursor<n)return;
     }
    }else
#endif
    {const auto end=std::min<uint32_t>(n,cursor+64*kBlock);for(;cursor<end;++cursor)if(cursor>=monoFirst&&cursor<monoLast)old.address[0][cursor]=ce_pcm_mono(old.address[0][cursor],old.isStereo?old.address[1][cursor]:old.address[0][cursor],static_cast<unsigned>(mode),monoGain,old.isStereo);if(cursor<n)return;}
   }
   const uint32_t freed=(oldFrames*(old.isStereo?2:1)-newFrames*channels())*2;
   if(freed){
   const unsigned following=newFileManager.calcFirstSlotToMoveInMemory(slot+1);
   uint64_t oldBlocks=0,oldNotes=0;
   for(unsigned i=following;i<48;++i){if(isBlockedInstrument[i])oldBlocks|=uint64_t(1)<<i;if(isBlockedNoteOn[i])oldNotes|=uint64_t(1)<<i;engine.setBlockedNoteOnFromInstrument(i);}
   for(unsigned v=0;v<8;++v)if(instrumentPlayer[v].currentInstrument_idx>=following&&instrumentPlayer[v].currentInstrument_idx<48)instrumentPlayer[v].noteOff(Sequencer::STEP_NOTE_OFF_CUT);
   newFileManager.moveSampleMemory(following,-int32_t(freed));
   for(unsigned i=following;i<48;++i){if(oldBlocks&(uint64_t(1)<<i))engine.setBlockedInstrument(i);if(!(oldNotes&(uint64_t(1)<<i)))engine.clearBlockedNoteOnFromInstrument(i);}
   }
   #if TRACKER_CE_ENABLE_SD_EDITS
   if(edit!=Edit::None){const auto address=mtProject.instrument[slot].sample.address;mtProject.instrument[slot]=replacement;mtProject.instrument[slot].sample.address=address;}
   voiceStreamManager.installMonoCache(slot,p.main,sizeof(header),cache,old.isStreamed,n,channels()==2?&rightCache:nullptr);
#else
   voiceStreamManager.installMonoCache(slot,p.main,sizeof(header),cache,old.isStreamed,n);
#endif
   mtProject.used_memory=newFileManager.getSampleMemorySize(0,47);newFileManager.setInstrumentStructChanged(slot);newFileManager.clearSampleChanged(slot);
   metadataChanged(slot,
#if TRACKER_CE_ENABLE_SD_EDITS
       edit==Edit::Crop
#else
       false
#endif
   );
   unblock();step=Step::Finish;return;
  }
#if TRACKER_CE_ENABLE_SD_EDITS
  if(nativeSpec&&nativeSpec->commitHistory)nativeSpec->commitHistory(nativeOwner,nativeDsp);
#endif
  const unsigned previousUndoSlot=undoContext.valid?undoContext.slot:48;
  bool finalized=finalizeApplyRecovery(p);
  if(finalized){
   if(previousUndoSlot<48&&previousUndoSlot!=slot){Paths previous;paths(previous,previousUndoSlot);finalized=remove(previous.undo)&&remove(previous.undoInfo)&&remove(previous.undoNew)&&remove(previous.undoInfoNew);}
  }
  const bool commitPresent=committed(p,"MONODONE")||(step==Step::Finish&&exists(p.main)==1);
  if(finalized||commitPresent){undoContext.valid=exists(p.undo)==1&&(readUndoDescriptor(p.undoInfo,undoDescriptor)||readUndoDescriptor(p.undoInfoNew,undoDescriptor));undoContext.slot=slot;undoContext.effect=effectIndex;undoContext.streamed=old.isStreamed;undoContext.ramFrames=mtProject.sampleMemory[slot].ramFrames?mtProject.sampleMemory[slot].ramFrames:mtProject.instrument[slot].sample.length;undoContext.sampleHash=mtProject.sampleMemory[slot].sampleHash;undoContext.instrumentCrc=crc(reinterpret_cast<const uint8_t*>(&mtProject.instrument[slot]),sizeof(strInstrument));undoContext.start=appliedSelectionStart;undoContext.end=appliedSelectionEnd;}
  message=finalized?(edit==Edit::None?"Converted.":"Edited."):commitPresent?(edit==Edit::None?"Converted; Undo backup recovery pending.":"Edited; Undo backup recovery pending."):"Edited; Undo backup recovery pending.";
  completed=finalized||commitPresent;finished=true;
 }
};
static_assert(sizeof(Job)<EDIT_BUFF_SIZE*sizeof(int16_t),"Mono scratch must fit existing editor arena");
Job*job=nullptr;
} // namespace
void* workBuffer(unsigned bytes){
 const auto base=reinterpret_cast<uintptr_t>(externalRamSampleMemory);
 if(job||samplepreview::active()){message="Sample editor busy.";return nullptr;}
 // The native effect arena overlaps the sample pool: never assume it is free.
 uintptr_t scratch=reinterpret_cast<uintptr_t>(externalRamEffectsBuff),liveEnd=base;bool overlap=false;
 for(unsigned i=0;i<48;++i){if(!mtProject.instrument[i].isActive||!mtProject.instrument[i].sample.length)continue;const auto&m=mtProject.sampleMemory[i];
  const uint64_t a=reinterpret_cast<uintptr_t>(m.address[0]),count=m.ramFrames?m.ramFrames:mtProject.instrument[i].sample.length;
  const uint64_t end=a+count*(m.isStereo?4:2);
  if(a<base||end>uint64_t(base)+SAMPLE_MEMORY_SIZE){message="Invalid sample allocation; nothing changed.";return nullptr;}
  liveEnd=std::max<uintptr_t>(liveEnd,uintptr_t(end));if(a<scratch+bytes&&end>scratch)overlap=true;
 }
 if(overlap)scratch=(liveEnd+31u)&~uintptr_t(31u);
 if(scratch<base||uint64_t(scratch)+bytes>uint64_t(base)+SAMPLE_MEMORY_SIZE){
  message="Not enough safe work RAM; original preserved.";return nullptr;
 }
 return reinterpret_cast<void*>(scratch);
}
static bool start(unsigned slot,Mode mode,Edit edit,uint16_t startPoint,uint16_t endPoint,float parameter,float secondary=0,const samplepreview::EffectCopy* nativeSpec=nullptr,mtEffect* nativeSource=nullptr,unsigned effectIndex=0xffffffffu){
 if(job||samplepreview::active()){message="Sample editor busy.";return false;}
 setUndoContext(slot);
 if(!valid(mode)){message="Invalid conversion mode; original preserved.";return false;}
 message="Select an active stereo instrument.";
 if(slot>=48||!mtProject.instrument[slot].isActive||!mtProject.instrument[slot].sample.length)return false;
 if(!std::isfinite(parameter)||(edit==Edit::None&&(parameter< -12||parameter>6))||(edit==Edit::Normalize&&(parameter<0||parameter>100))||(edit==Edit::Amplifier&&(parameter<0.01f||parameter>5))){message="Invalid effect parameter.";return false;}
#if TRACKER_CE_ENABLE_SD_EDITS
 if(!std::isfinite(secondary)||(edit==Edit::Overdrive&&(parameter<0.01f||parameter>5||secondary<0||secondary>2||secondary!=int(secondary)))||(edit==Edit::Bitcrusher&&(parameter<1||parameter>16||parameter!=int(parameter)||secondary<500||secondary>44100||secondary!=int(secondary)))){message="Invalid effect parameters.";return false;}
#else
 (void)secondary;
#endif
 if((edit>=Edit::Equalizer)?!(nativeSpec&&nativeSource&&nativeSpec->processChannel):(nativeSpec||nativeSource)){message="Invalid native effect; original preserved.";return false;}
 if(newFileManager.getStatus()!=fmIdle||newFileManager.getCurrentOperation()!=fmNoOperation){message="File manager busy.";return false;}
 if(!recoverOne(slot)){message="Resolve pending SD recovery first.";return false;}
 auto&mem=mtProject.sampleMemory[slot];const auto frames=mtProject.instrument[slot].sample.length,ram=mem.ramFrames?mem.ramFrames:frames;
 if(!mem.address[0]||mem.address[1]!=mem.address[0]+(mem.isStereo?ram:0)||uint64_t(frames)*(mem.isStereo?4:2)+sizeof(strInstrumentFile)>UINT32_MAX){message="Unsupported sample layout or file length.";return false;}
 const auto base=reinterpret_cast<uintptr_t>(externalRamSampleMemory),address=reinterpret_cast<uintptr_t>(mem.address[0]);
 if(address<base||uint64_t(address-base)+uint64_t(ram)*(mem.isStereo?4:2)>SAMPLE_MEMORY_SIZE||(!mem.isStreamed&&ram!=frames)||isBlockedInstrument[slot]||isBlockedNoteOn[slot]){message="Sample memory is unavailable.";return false;}
 if(mem.isStreamed&&!isPlayModeStreamable(mtProject.instrument[slot].playMode)){message="Unsupported streaming mode.";return false;}
 if(startPoint>endPoint)std::swap(startPoint,endPoint);
 const uint32_t selectedFirst=std::min<uint32_t>(frames,uint32_t(uint32_t(startPoint)*float(frames)/MAX_16BIT)),selectedLast=std::min<uint32_t>(frames,uint32_t(uint32_t(endPoint)*float(frames)/MAX_16BIT));
 if(selectedFirst>=selectedLast){message="Empty selection; original preserved.";return false;}
 uint32_t outputFrames=frames;
#if TRACKER_CE_ENABLE_SD_EDITS
 if(edit!=Edit::None){if(startPoint>endPoint)std::swap(startPoint,endPoint);const auto a=std::min<uint32_t>(frames,uint32_t(uint32_t(startPoint)*float(frames)/MAX_16BIT)),b=std::min<uint32_t>(frames,uint32_t(uint32_t(endPoint)*float(frames)/MAX_16BIT));if(a>=b){message="Empty selection; original preserved.";return false;}if(edit==Edit::Crop)outputFrames=b-a;}
#endif
 DWORD free=0;FATFS*fs=nullptr;if(f_getfree("2:",&free,&fs)!=FR_OK||!fs||uint64_t(free)*fs->csize*512<uint64_t(outputFrames)*(edit!=Edit::None&&mem.isStereo?4:2)+uint64_t(frames)*(mem.isStereo?4:2)+2*sizeof(strInstrumentFile)+8192){message="Not enough free SD space.";return false;}
 const auto offset=(sizeof(Job)+31u)&~31u;const auto copyBytes=nativeSpec?((nativeSpec->bytes+31u)&~31u):0;
 void*scratch=workBuffer(unsigned(offset+2*copyBytes));if(!scratch)return false;
 job=new(reinterpret_cast<void*>(scratch))Job(slot,mode,edit==Edit::None?parameter:0,selectedFirst,selectedLast);
 job->effectIndex=effectIndex;job->appliedSelectionStart=startPoint;job->appliedSelectionEnd=endPoint;
#if TRACKER_CE_ENABLE_SD_EDITS
 job->parameter=parameter;job->secondary=secondary;job->edit=edit;std::memcpy(&job->replacement,&job->original,sizeof(job->original));
 if(edit!=Edit::None){if(startPoint>endPoint)std::swap(startPoint,endPoint);job->first=std::min<uint32_t>(frames,uint32_t(uint32_t(startPoint)*float(frames)/MAX_16BIT));job->last=std::min<uint32_t>(frames,uint32_t(uint32_t(endPoint)*float(frames)/MAX_16BIT));
  if(job->first>=job->last){job->finished=true;message="Empty selection; original preserved.";return true;}
  if(edit==Edit::Crop)job->replacement.sample.length=job->last-job->first;
  if(edit==Edit::Normalize){job->step=Step::Scan;job->scanPos=job->first;}
  if(edit==Edit::Amplifier)job->amplitudes[0]=job->amplitudes[1]=parameter;
  if(edit==Edit::Overdrive)job->amplitudes[0]=job->amplitudes[1]=overdriveGain(parameter,unsigned(secondary));
  if(edit==Edit::Bitcrusher)job->sampleStep=uint16_t(crusherStep(unsigned(secondary)));
 }
#else
 (void)startPoint;(void)endPoint;
#endif
#if TRACKER_CE_ENABLE_SD_EDITS
 if(nativeSpec){job->nativeOwner=nativeSource;job->nativeSpec=nativeSpec;job->nativeSeed=nativeSpec->copy(static_cast<char*>(scratch)+offset,*nativeSource);job->nativeWorking=static_cast<char*>(scratch)+offset+copyBytes;if(!job->resetNative()){job->fail("Cannot initialize native effect.");return true;}}
#endif
 newFileManager.setWorkspaceSaveSuspended(true);
 if(exists(job->p.main)!=1||!remove(job->p.temp)){job->fail("Workspace file unavailable; original preserved.");return true;}
 #if TRACKER_CE_ENABLE_SD_EDITS
 newFileManager.writeInstrumentToFileStruct(edit==Edit::None?&job->original:&job->replacement,&job->header);
#else
 newFileManager.writeInstrumentToFileStruct(&job->original,&job->header);
#endif
 if(job->old.isStreamed){std::snprintf(job->source,256,"%s",voiceStreamManager.getInstrumentStreamPath(slot));if(f_open(&job->inputFile,job->source,FA_READ)!=FR_OK){job->fail("Cannot read source; original preserved.");return true;}job->inputOpen=true;
  if(uint64_t(f_size(&job->inputFile))<uint64_t(mem.fileDataOffset)+uint64_t(frames)*(mem.isStereo?4:2)){job->fail("Truncated source; original preserved.");return true;}}
 if(f_open(&job->outputFile,job->p.temp,FA_WRITE|FA_CREATE_NEW)!=FR_OK){job->fail("Cannot create temporary file.");return true;}job->outputOpen=true;UINT put=0;
 if(f_write(&job->outputFile,&job->header,sizeof(job->header),&put)!=FR_OK||put!=sizeof(job->header))job->fail("Cannot write temporary header.");
 return true;
}
bool begin(unsigned slot,Mode mode,float gainDb,unsigned short a,unsigned short b,unsigned effectIndex){return start(slot,mode,Edit::None,a,b,gainDb,0,nullptr,nullptr,effectIndex);}
#if TRACKER_CE_ENABLE_SD_EDITS
bool beginEdit(unsigned slot,Edit edit,unsigned short a,unsigned short b,float parameter,float secondary,const samplepreview::EffectCopy* nativeSpec,mtEffect* nativeSource,unsigned effectIndex){if(edit<Edit::Crop||edit>Edit::Compressor){message="Invalid effect; original preserved.";return false;}return start(slot,Mode::Mix,edit,a,b,parameter,secondary,nativeSpec,nativeSource,effectIndex);}
#endif
bool undoAvailable(unsigned slot,unsigned effectIndex){if(job||slot>=48||!undoContext.valid||undoContext.slot!=slot||undoContext.effect!=effectIndex||undoContext.instrumentCrc!=crc(reinterpret_cast<const uint8_t*>(&mtProject.instrument[slot]),sizeof(strInstrument))||undoContext.sampleHash!=mtProject.sampleMemory[slot].sampleHash)return false;if(!recoverOne(slot))return false;Paths p;paths(p,slot);UndoDescriptor d{};return exists(p.undo)==1&&readUndoDescriptor(p.undoInfo,d)&&d.frames>0;}
bool beginUndo(unsigned slot,unsigned effectIndex){
 if(job||samplepreview::active()||slot>=48||!undoAvailable(slot,effectIndex)){message="No SD Undo data.";return false;}
 if(newFileManager.getStatus()!=fmIdle||newFileManager.getCurrentOperation()!=fmNoOperation){message="File manager busy.";return false;}
 Paths p;paths(p,slot);UndoDescriptor d{};if(!readUndoDescriptor(p.undoInfo,d)){message="Undo snapshot is invalid.";return false;}
 FIL f{};if(f_open(&f,p.undo,FA_READ)!=FR_OK){message="Cannot read Undo snapshot.";return false;}strInstrumentFile h{};const auto size=f_size(&f);uint32_t offset=0;strInstrument target{};bool ok=size==d.bytes&&exact(f,0,&h,sizeof(h))&&newFileManager.loadInstrumentFormFileStruct(&target,&h,size,offset);if(f_close(&f)!=FR_OK)ok=false;
 if(!ok||offset!=sizeof(strInstrumentFile)||target.sample.length!=d.frames){message="Undo instrument header is invalid.";return false;}
 const bool stereo=d.channels==2;const auto&mem=mtProject.sampleMemory[slot];const uint32_t currentRam=mem.ramFrames?mem.ramFrames:mtProject.instrument[slot].sample.length;const uint32_t targetRam=undoContext.streamed?std::min<uint32_t>(target.sample.length,kStreamPreCacheFrames):target.sample.length;
 const uint64_t currentBytes=uint64_t(currentRam)*(mem.isStereo?4:2),targetBytes=uint64_t(targetRam)*(stereo?4:2);const uint64_t used=newFileManager.getSampleMemorySize(0,47);
 if(target.sample.length==0||!targetRam||currentBytes>used||targetBytes>INT32_MAX||targetBytes>SAMPLE_MEMORY_SIZE||targetBytes>mtProject.max_memory||used-currentBytes>SAMPLE_MEMORY_SIZE-targetBytes||used-currentBytes>mtProject.max_memory-targetBytes||(undoContext.streamed&&!isPlayModeStreamable(target.playMode))){message="Not enough safe sample memory for Undo.";return false;}
 const int64_t signedDelta=int64_t(targetBytes)-int64_t(currentBytes);if(signedDelta<INT32_MIN||signedDelta>INT32_MAX){message="Undo memory layout is too large.";return false;}const int32_t delta=int32_t(signedDelta);
 void*scratch=workBuffer(sizeof(Job));if(!scratch)return false;job=new(scratch)Job(slot,Mode::Mix,0,0,MAX_16BIT);job->undoing=true;job->effectIndex=effectIndex;job->restoreInstrument=target;job->restoreFrames=target.sample.length;job->restoreChannels=d.channels;job->restoreStereo=stereo;job->restoreStreamed=undoContext.streamed;job->restoreRamFrames=targetRam;job->memoryDelta=delta;job->currentRamFrames=currentRam;job->undoDescriptor=d;job->undoDescriptorValid=true;job->undoScanOffset=0;job->undoScanCrc=0xffffffffu;job->step=Step::UndoValidate;job->appliedSelectionStart=undoContext.start;job->appliedSelectionEnd=undoContext.end;
 if(f_open(&job->reader,p.undo,FA_READ)!=FR_OK){job->~Job();job=nullptr;message="Cannot read Undo snapshot.";return false;}job->readerOpen=true;newFileManager.setWorkspaceSaveSuspended(true);message="Undoing sample...";return true;
}
bool undoing(){return job&&job->undoing;}
bool undone(){return job&&job->finished&&job->undoCompleted;}
void getUndoSelection(unsigned short&start,unsigned short&end){if(job&&job->undoing){start=job->appliedSelectionStart;end=job->appliedSelectionEnd;}else{start=undoContext.start;end=undoContext.end;}}
void setUndoContext(unsigned slot){if(!undoContext.valid)return;if(slot!=undoContext.slot||undoContext.instrumentCrc!=crc(reinterpret_cast<const uint8_t*>(&mtProject.instrument[slot]),sizeof(strInstrument))||undoContext.sampleHash!=mtProject.sampleMemory[slot].sampleHash)discardUndo();}
void discardUndo(){if(!undoContext.valid)return;Paths p;paths(p,undoContext.slot);remove(p.undo);remove(p.undoInfo);remove(p.undoNew);remove(p.undoInfoNew);undoContext.valid=false;}
bool active(){return job!=nullptr;}
void tick(){if(job&&!job->finished)job->tick();}
unsigned progress(){if(!job)return 0;if(job->finished)return 100;
#if TRACKER_CE_ENABLE_SD_EDITS
 if(job->step==Step::Scan)return unsigned((uint64_t(job->fileChannel)*(job->last-job->first)+job->scanPos-job->first)*20/(uint64_t(job->channels())*(job->last-job->first)));
#endif
if(job->step==Step::Mix){
#if TRACKER_CE_ENABLE_SD_EDITS
 if(job->edit!=Edit::None){const auto value=unsigned((uint64_t(job->fileChannel)*job->frames()+job->editPos)*45/(uint64_t(job->frames())*job->channels()))+(job->verifying?45:0);return job->edit==Edit::Normalize?20+value*70/90:value;}
#endif
 return job->conversion.progress();}if(job->step==Step::Precache)return 91;if(job->step==Step::Peaks)return 92+job->peak*5/600;if(job->step==Step::Slices)return 98;return 99;}
bool done(){return job&&job->finished;}
bool cancellable(){if(!job||job->finished)return false;if(job->undoing)return job->step==Step::UndoValidate;return job->step<Step::Promote;}
void cancel(){if(!job||job->finished)return;if(job->undoing){if(job->step==Step::UndoValidate){job->close();newFileManager.setWorkspaceSaveSuspended(false);message="";job->finished=true;job->completed=false;}return;}if(job->step>=Step::Ram){while(!job->finished)job->tick();}else job->fail("");}
bool succeeded(){return job&&job->finished&&job->completed;}
bool applied(){return job&&job->finished&&job->step==Step::Finish;}
bool needsRecovery(){return message&&std::strstr(message,"recovery")!=nullptr;}
const char* result(){if(job&&job->finished){job->~Job();job=nullptr;}return message;}
bool recover(){undoContext=UndoContext{};bool ok=true;for(unsigned i=0;i<48;++i)ok=recoverOne(i,true)&&ok;return ok;}
}
#endif
