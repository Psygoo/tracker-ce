from pathlib import Path
import re,sys,subprocess,json,hashlib
r=Path(sys.argv[1]);o=Path(sys.argv[2]);manifest=[]
def body(path,sig):
 s=(r/path).read_text(encoding="utf-8");a=s.index(sig);b=s.index('{',a);e=b+1;d=1
 while d:d+=(s[e]=='{')-(s[e]=='}');e+=1
 v=s[a:e]
 original=subprocess.run(['git','-C',str(r),'show','998b06d:'+path],capture_output=True,check=True).stdout.decode().replace('\r\n','\n')
 assert v in original, 'IO body differs from original: '+sig
 manifest.append(dict(path=path,signature=sig,line=s.count('\n',0,a)+1,body_sha256=hashlib.sha256(v.encode()).hexdigest(),original_ce_identical=True))
 return v
h=(r/'core/AudioEngine/VoiceStreamManager.h').read_text(encoding="utf-8")
h=re.sub(r'^#include[^\n]*','',h,flags=re.M).replace('  private:','  public:')
decl='''// Host-only filesystem boundary. No real files/card access.
struct WaveformPeakPoint;
struct FileSystem {
 unsigned waits=0;bool busy=false,ok=true;
 bool waitAsyncRead(){CHECK(!Registers::mask&&Registers::nvic);++waits;busy=false;return ok;}
 bool isAsyncReadBusy(){return busy;}
 bool pollAsyncRead(bool*out){*out=ok;busy=false;return true;}
};
'''+h+'''
VoiceStreamManager voiceStreamManager;
struct SliceCacheEntry{uint32_t startFrame;uint16_t frameCount,bufferOffset;};
static SliceCacheEntry s_instrumentSliceCache[48][48]{};
static bool s_instrumentSliceCacheValid[48]{};
static int16_t s_voiceRingBuffers[8][2][16384]{},s_voiceLoopWrapBuffers[8][2][2048]{};
alignas(32) static uint8_t s_dmaStagingBuffer[9216]{};
'''
decl+=body('core/libs/Audio/play_memory.cpp','static uint32_t getMaxReadableFramesForAddress(')+'\n'
for sig in ['AUDIO_CORE_RAM bool VoiceStreamManager::getSlicePreCache(', 'void VoiceStreamManager::notifyNoteOn(', 'void VoiceStreamManager::notifyNoteOff(', 'AUDIO_CORE_RAM void VoiceStreamManager::updateVoicePlayhead(', 'AUDIO_CORE_RAM int16_t VoiceStreamManager::getSample(', 'void VoiceStreamManager::completeChannelTransfer(', 'void VoiceStreamManager::update()', 'void VoiceStreamManager::updateSynchronous()']:
 decl+=body('core/AudioEngine/VoiceStreamManager.cpp',sig)+'\n'
(o/'io-stream-declarations.inc').write_text(decl)
(o/'io-refresh.inc').write_text(body('core/libs/Audio/play_memory.cpp','void AudioPlayMemory::refreshStartParameters()'))
(o/'io-export-body.inc').write_text(body('core/Exporter/mtAudioExport.cpp','void mtAudioExport::updateRender()'))
(o/'io-manifest.json').write_text(json.dumps(manifest,indent=2))
print('IO: extracted',len(manifest),'complete bodies, identical to CE 998b06d; scheduler/filesystem/DSP remain test seams')

(o/'import-completion.inc').write_text(body('core/FileManager/fileManagerSamples.cpp','void cFileManager::completeLoadedInstrumentStruct(').replace('cFileManager::','ImportCompletion::',1),encoding='utf-8')
(o/'io-manifest.json').write_text(json.dumps(manifest,indent=2))
