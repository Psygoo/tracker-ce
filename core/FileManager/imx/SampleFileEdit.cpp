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
struct Paths{char main[256],temp[256],backup[256],pending[256],done[256];};
void paths(Paths&p,unsigned slot){std::snprintf(p.main,256,cWorkspaceInstrumentFileFormatNew,slot+1);std::snprintf(p.temp,256,"%s.mono.tmp",p.main);std::snprintf(p.backup,256,"%s.mono.bak",p.main);std::snprintf(p.pending,256,"%s.mono.pending",p.main);std::snprintf(p.done,256,"%s.mono.done",p.main);}
int exists(const char*p){FILINFO info{};auto r=f_stat(p,&info);if(r==FR_NO_FILE||r==FR_NO_PATH)return 0;return r==FR_OK&&!(info.fattrib&AM_DIR)?1:-1;}
bool remove(const char*p){const auto found=exists(p);return found==0||(found==1&&f_unlink(p)==FR_OK);}
bool exact(FIL&f,uint32_t offset,void*data,unsigned n){UINT got=0;return f_lseek(&f,offset)==FR_OK&&f_read(&f,data,n,&got)==FR_OK&&got==n;}
bool marker(const char*p,const char*magic,uint32_t frames){uint8_t b[16]{};std::memcpy(b,magic,8);std::memcpy(b+8,&frames,4);const auto c=crc(b,12);std::memcpy(b+12,&c,4);FIL f{};if(f_open(&f,p,FA_WRITE|FA_CREATE_ALWAYS)!=FR_OK)return false;UINT put=0;bool ok=f_write(&f,b,16,&put)==FR_OK&&put==16;if(f_sync(&f)!=FR_OK)ok=false;if(f_close(&f)!=FR_OK)ok=false;uint8_t check[16];if(!ok||f_open(&f,p,FA_READ)!=FR_OK)return false;ok=f_size(&f)==16&&exact(f,0,check,16)&&!std::memcmp(b,check,16);if(f_close(&f)!=FR_OK)ok=false;return ok;}
bool committed(const Paths&p){FIL f{};if(f_open(&f,p.done,FA_READ)!=FR_OK)return false;uint8_t b[16]{};bool ok=f_size(&f)==16&&exact(f,0,b,16);if(f_close(&f)!=FR_OK)ok=false;uint32_t checksum=0,frames=0;std::memcpy(&checksum,b+12,4);std::memcpy(&frames,b+8,4);if(!ok||std::memcmp(b,"MONODONE",8)||checksum!=crc(b,12))return false;FILINFO info{};return f_stat(p.main,&info)==FR_OK&&uint64_t(info.fsize)==sizeof(strInstrumentFile)+uint64_t(frames)*2;}
bool recoverOne(unsigned slot){Paths p;paths(p,slot);int pending=exists(p.pending);if(pending<0)return false;if(!pending)return remove(p.temp)&&remove(p.done);
 if(committed(p)){if(!remove(p.backup)||!remove(p.temp)||!remove(p.pending))return false;return remove(p.done);}
 const auto backup=exists(p.backup);if(backup<0)return false;
 if(backup&&! (remove(p.main)&&f_rename(p.backup,p.main)==FR_OK))return false;
 return remove(p.temp)&&remove(p.done)&&remove(p.pending);
}
enum class Step{Scan,Mix,Precache,Peaks,Slices,Promote,Ram,Finish};
struct Job final:IO{
 unsigned slot;Mode mode;float monoGain;uint32_t monoFirst,monoLast;bool completed=false;strInstrument original;strSampleMemory old;strInstrumentFile header{};Paths p;char source[256]{};
 FIL inputFile{},outputFile{},reader{};bool inputOpen=false,outputOpen=false,readerOpen=false,blocked=false,promoting=false;
 VoiceStreamManager::MonoCache cache{};Blocks blocks;Conversion conversion;Step step=Step::Mix;
 uint32_t cursor=0,peak=0,slice=0,slicePos=0;bool finished=false;
#if TRACKER_CE_ENABLE_SD_EDITS
 const samplepreview::EffectCopy* nativeSpec=nullptr;mtEffect* nativeSeed=nullptr;mtEffect* nativeDsp=nullptr;mtEffect* nativeOwner=nullptr;void* nativeWorking=nullptr;
 ~Job(){if(nativeDsp)nativeSpec->destroy(nativeDsp);if(nativeSeed)nativeSpec->destroy(nativeSeed);}
 bool resetNative(){if(!nativeSpec)return true;if(nativeDsp)nativeSpec->destroy(nativeDsp);nativeDsp=nativeSpec->copy(nativeWorking,*nativeSeed);int16_t* empty[2]{};nativeDsp->bindPreviewBuffers(empty,empty,last-first);return nativeSpec->start(nativeDsp,nullptr,old.isStereo);}
 void nativeBlock(unsigned ch,uint32_t at,unsigned n,int16_t* data){const auto a=std::max(at,first),b=std::min(at+n,last);if(a<b){if(channels()==1){std::memcpy(blocks.right,data+a-at,(b-a)*2);nativeSpec->processChannel(nativeDsp,1,blocks.right,blocks.right,b-a);}nativeSpec->processChannel(nativeDsp,ch,data+a-at,data+a-at,b-a);}}
 float amplitudes[2]{1,1};int peakValues[2]{};float parameter=1,secondary=0;uint32_t scanPos=0;
 CrushState crusher[2]{};uint16_t sampleStep=1;
 Edit edit=Edit::None;strInstrument replacement{};VoiceStreamManager::MonoCache rightCache{};
 uint32_t first=0,last=0,editPos=0,expected=0xffffffffu,checked=0xffffffffu;unsigned fileChannel=0;bool verifying=false;
 uint32_t frames()const{return edit==Edit::None?original.sample.length:replacement.sample.length;}
 unsigned channels()const{return edit!=Edit::None&&old.isStereo?2:1;}
#else
 uint32_t frames()const{return original.sample.length;}unsigned channels()const{return 1;}
#endif
 uint32_t fileOffset(uint32_t frame)const{
#if TRACKER_CE_ENABLE_SD_EDITS
  return sizeof(header)+(fileChannel*frames()+frame)*2;
#else
  return sizeof(header)+frame*2;
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
 void unblock(){if(blocked){engine.clearBlockedInstrument(slot);engine.clearBlockedNoteOnFromInstrument(slot);blocked=false;}newFileManager.setWorkspaceSaveSuspended(false);}
 void fail(const char*why){close();if(promoting){const bool restored=exists(p.backup)==1&&remove(p.main)&&f_rename(p.backup,p.main)==FR_OK;if(restored){remove(p.done);remove(p.pending);}else{remove(p.done);}if(old.isStreamed&&!std::strcmp(source,p.main)&&exists(p.backup)==1)std::snprintf(voiceStreamManager.getInstrumentStreamPath(slot),256,"%s",p.backup);}remove(p.temp);unblock();message=why;finished=true;}
 bool prepare(){
#if TRACKER_CE_ENABLE_SD_EDITS
  auto&prepared=fileChannel?rightCache:cache;const auto&instrument=edit==Edit::None?original:replacement;
#else
  auto&prepared=cache;const auto&instrument=original;
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
   prepared.sliced=old.isStreamed&&pre==kStreamPreCacheFrames&&(instrument.playMode==playModeSlice||instrument.playMode==playModeBeatSlice)&&instrument.sliceNumber>0&&instrument.sliceNumber<=48;
   if(prepared.sliced){prepared.linearFrames=std::min<uint32_t>(n,kStreamPreCacheFrames/2);}
   step=Step::Slices;return true;}
  if(step==Step::Slices){if(prepared.sliced&&slice<instrument.sliceNumber){const uint32_t half=kStreamPreCacheFrames/2;uint32_t chunk=(half/instrument.sliceNumber)&~15u;if(!chunk)chunk=half/instrument.sliceNumber;
    uint32_t start=uint32_t(float(instrument.slices[slice])*(float(n)/MAX_16BIT));if(start>=n)start=n-1;const auto count=std::min<uint32_t>(chunk,n-start);
    if(start+count<=prepared.linearFrames){prepared.slices[slice]={start,uint16_t(std::min<uint32_t>(prepared.linearFrames-start,65535)),uint16_t(start)};++slice;return true;}
    const unsigned off=half+slice*chunk,part=std::min<uint32_t>(kBlock,count-slicePos);
    if(!verify(start+slicePos,part,prepared.samples+off+slicePos))return false;
    slicePos+=part;
    if(slicePos==count){prepared.slices[slice]={start,uint16_t(count),uint16_t(off)};slicePos=0;++slice;}return true;}
#if TRACKER_CE_ENABLE_SD_EDITS
   if(fileChannel+1<channels()){++fileChannel;cursor=peak=slice=slicePos=0;step=Step::Precache;return true;}
#endif
   if(old.isStreamed){cache.clmt[0]=kClmtElementCount;reader.cltbl=reinterpret_cast<DWORD*>(cache.clmt);if(f_lseek(&reader,CREATE_LINKMAP)!=FR_OK)return false;cache.databaseSector=reader.obj.fs->database;cache.sectorsPerCluster=reader.obj.fs->csize;}
   if(!close())return false;
   step=Step::Promote;return true;}
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
  // No fallible IO or cancellation after the validated file has been promoted.
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
  const bool cleaned=remove(p.backup)&&remove(p.pending)&&remove(p.done);message=cleaned?"Converted to mono. Save the project.":"Converted; recovery backup retained.";
#if TRACKER_CE_ENABLE_SD_EDITS
  if(edit!=Edit::None)message=cleaned?"Edited sample. Save the project.":"Edited; recovery backup retained.";
#endif
  completed=cleaned;finished=true;
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
static bool start(unsigned slot,Mode mode,Edit edit,uint16_t startPoint,uint16_t endPoint,float parameter,float secondary=0,const samplepreview::EffectCopy* nativeSpec=nullptr,mtEffect* nativeSource=nullptr){
 if(job||samplepreview::active()){message="Sample editor busy.";return false;}
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
 DWORD free=0;FATFS*fs=nullptr;if(f_getfree("2:",&free,&fs)!=FR_OK||!fs||uint64_t(free)*fs->csize*512<uint64_t(outputFrames)*(edit!=Edit::None&&mem.isStereo?4:2)+sizeof(strInstrumentFile)+8192){message="Not enough free SD space.";return false;}
 const auto offset=(sizeof(Job)+31u)&~31u;const auto copyBytes=nativeSpec?((nativeSpec->bytes+31u)&~31u):0;
 void*scratch=workBuffer(unsigned(offset+2*copyBytes));if(!scratch)return false;
 job=new(reinterpret_cast<void*>(scratch))Job(slot,mode,edit==Edit::None?parameter:0,selectedFirst,selectedLast);
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
bool begin(unsigned slot,Mode mode,float gainDb,unsigned short a,unsigned short b){return start(slot,mode,Edit::None,a,b,gainDb);}
#if TRACKER_CE_ENABLE_SD_EDITS
bool beginEdit(unsigned slot,Edit edit,unsigned short a,unsigned short b,float parameter,float secondary,const samplepreview::EffectCopy* nativeSpec,mtEffect* nativeSource){if(edit<Edit::Crop||edit>Edit::Compressor){message="Invalid effect; original preserved.";return false;}return start(slot,Mode::Mix,edit,a,b,parameter,secondary,nativeSpec,nativeSource);}
#endif
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
bool cancellable(){return job&&job->step<Step::Promote&&!job->finished;}
void cancel(){if(!job)return;if(job->step>=Step::Ram){while(!job->finished)job->tick();}else if(!job->finished)job->fail("Cancelled; original preserved.");}
bool succeeded(){return job&&job->finished&&job->completed;}
bool applied(){return job&&job->finished&&job->step==Step::Finish;}
const char* result(){if(job&&job->finished){job->~Job();job=nullptr;}return message;}
bool recover(){bool ok=true;for(unsigned i=0;i<48;++i)ok=recoverOne(i)&&ok;return ok;}
}
#endif
