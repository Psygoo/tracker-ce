from pathlib import Path
import sys,re,json,hashlib
r=Path(sys.argv[1]);o=Path(sys.argv[2]);o.mkdir(parents=True,exist_ok=True)
s=(r/'core/FileManager/imx/SampleFileEdit.cpp').read_text(encoding='utf-8-sig');s=re.sub(r'^#include.*$','',s,flags=re.M);(o/'native.inc').write_text(s,encoding='utf-8')
s=(r/'core/AudioEngine/VoiceStreamManager.h').read_text(encoding='utf-8');a=s.index('    struct MonoCache');b=s.index('    // IO-free commit',a);(o/'cache.inc').write_text(s[a:b],encoding='utf-8')
s=(r/'core/AudioEngine/VoiceStreamManager.cpp').read_text(encoding='utf-8');a=s.index('void VoiceStreamManager::installMonoCache(');b=s.index('#endif',a);(o/'install.inc').write_text(s[a:b],encoding='utf-8')
(o/'manifest.json').write_text(json.dumps([dict(path=str(p.relative_to(r)),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in [r/'core/FileManager/imx/SampleFileEdit.cpp',r/'core/AudioEngine/VoiceStreamManager.cpp',r/'core/effector/sampleProcessing/Conversion.h']],indent=2),encoding='utf-8')
print('Extracted complete native conversion/transaction/recovery and actual cache installer')

s=(r/'core/interface/core/interfaceMisc.cpp').read_text(encoding='utf-8');a=s.index('void Interface::openStartupProject()');b=s.index('{',a);e=b+1;depth=1
while depth:depth+=(s[e]=='{')-(s[e]=='}');e+=1
(o/'startup.inc').write_text(s[a:e],encoding='utf-8')

def body(text,start):
 a=text.index(start);b=text.index('{',a);e=b+1;depth=1
 while depth:depth+=(text[e]=='{')-(text[e]=='}');e+=1
 return text[a:e]
f=(r/'core/interface/sampleEditor/fSampleEdit.cpp').read_text(encoding='utf-8')
h=(r/'core/interface/sampleEditor/sampleEditor.h').read_text(encoding='utf-8')
parts=[body(h,'inline bool isMonoEffect'),body(h,'inline bool isSdMonoEffect'),body(h,'inline bool isSdRegionEffect'),body(h,'namespace sdMonoParams'),body(f,'uint8_t cSampleEditor::getParameterListCount'),body(f,'char** cSampleEditor::getParameterListItems'),body(f,'static void monoBegin()')]
(o/'mode-ui.inc').write_text('\n'.join(parts),encoding='utf-8')
assert '{}, // Stereo to Mono SD' in h,'Native mode default must remain zero/Mix'

legacy=[]
for name in ['Cut','Reverse','FadeIn','FadeOut']:
 for ext in ['h','cpp']:
  text=(r/('core/effector/mtEffect'+name+'.'+ext)).read_text(encoding='utf-8')
  legacy.append(re.sub(r'^#include.*$','',text,flags=re.M))
(o/'legacy.inc').write_text('\n'.join(legacy),encoding='utf-8')

(o/'stream-ui.inc').write_text(body(f,'static bool streamedEditorSelected')+'\n'+body(f,'static bool streamedEditorSpectrum'),encoding='utf-8')

(o/'completion-ui.inc').write_text('void cSampleEditor::complete(){'+body(f,'if(sampleedit::done())')+'}',encoding='utf-8')
assert 'Sample file operation; save after Apply.' not in f

s=(r/'core/FileManager/SampleEditNotifications.cpp').read_text(encoding='utf-8');(o/'notifications.inc').write_text(re.sub(r'^#include.*$','',s,flags=re.M),encoding='utf-8')

s=(r/'tests/ce-editor/reference/core/effector/mtEffectOverdrive.cpp').read_text(encoding='utf-8');a=s.index('            int16_t outVal = 0;');b=s.index('            *channelParams[channel].dstAddr',a);(o/'original-drive.inc').write_text('static int16_t referenceDrive(int16_t sample,float gainNorm,unsigned type){enum{QUADRATIC,EXPONENTIAL,RECIPROCAL};'+s[a:b].replace('*channelParams[channel].srcAddr','sample')+'return outVal;}\n',encoding='utf-8')

s=(r/'core/effector/SamplePreviewEffects.h').read_text();(o/'preview-spec.inc').write_text(re.sub(r'^#include.*$','',s,flags=re.M),encoding='utf-8')
s=(r/'core/effector/mtEffect.cpp').read_text();a=s.index('void mtEffect::bindPreviewBuffers(');b=s.index('\n#endif',a);(o/'preview-bind.inc').write_text(s[a:b])
a=s.index('bool mtEffect::hasPreviewWorkspaceConflict() const');b=s.index('\n#endif',a);(o/'preview-conflict.inc').write_text(s[a:b])
