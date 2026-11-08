from pathlib import Path
import re,sys
r=Path(sys.argv[1]);o=Path(sys.argv[2]);o.mkdir(parents=True,exist_ok=True)
def clean(s):
 s=re.sub(r'^#include.*$','',s,flags=re.M)
 s=re.sub(r'^#(?:ifndef|define) EFFECTOR_.*$','',s,flags=re.M)
 s=re.sub(r'^#endif /\* EFFECTOR_.*$','',s,flags=re.M)
 return s
before=r/'tests/ce-editor/reference'
for kind,root in [('original',before),('current',r)]:
 text=[]
 for name in ['Normalizer','Amplifier','Overdrive','Bitcrusher']:
  for ext in ['h','cpp']:text.append(clean((root/('core/effector/mtEffect'+name+'.'+ext)).read_text(encoding='utf-8')))
 (o/(kind+'.inc')).write_text('\n'.join(text),encoding='utf-8')
 (o/(kind+'-plugin.inc')).write_text(clean((root/'editor-fx-sdk/effects/stereo_to_mono.c').read_text(encoding='utf-8')),encoding='utf-8')
(o/'mono.inc').write_text(clean((r/'core/effector/mtEffectStereoMono.h').read_text(encoding='utf-8'))+'\n'+clean((r/'core/effector/mtEffectStereoMono.cpp').read_text(encoding='utf-8')),encoding='utf-8')
s=(r/'core/effector/ptfx_manager.cpp').read_text(encoding='utf-8');a=s.index('static bool bundledMono(');b=s.index('\n}',a)+2;(o/'dedup.inc').write_text(s[a:b],encoding='utf-8')

s=(r/'core/interface/sampleEditor/fSampleEdit.cpp').read_text(encoding='utf-8');a=s.index('bool cSampleEditor::isCurrentSampleStereo()');b=s.index('void cSampleEditor::prepareDisplayDataMainScreen',a);(o/'stereo-ui.inc').write_text(s[a:b],encoding='utf-8')

def body(s,name):
 a=s.index(name);b=s.index('{',a);e=b+1;depth=1
 while depth:depth+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
f=(r/'core/interface/sampleEditor/fSampleEdit.cpp').read_text(encoding='utf-8');g=(r/'core/interface/sampleEditor/gSampleEdit.cpp').read_text(encoding='utf-8')
pieces=[body(f,'static void releaseLargePreview()'),body(f,'static void cancelLargePreview()'),body(f,'bool cSampleEditor::canPreviewCurrentEffect() const'),body(f,'void cSampleEditor::setPreviewFunction()'),body(f,'void cSampleEditor::setPlayFunction()'),body(g,'void cSampleEditor::showPreviewLabel()'),body(g,'void cSampleEditor::showPlayLabel()')]
for name,end in [('functPreview','    if (!SE->canApplyCurrentEffect())'),('functPlay','    mtPadBoard.cutAllInstrument();')]:
 a=f.index('static void '+name+'()\n{');b=f.index(end,a);pieces.append(f[a:b]+'    ++nativePreviewCalls;\n}')
(o/'preview-ui.inc').write_text('\n'.join(pieces),encoding='utf-8')
assert 'Use Sample Playback to audition SD; Apply edits.' not in f
assert 'Audition SD samples in Sample Playback.' not in f

a=f.index('void cSampleEditor::setParamsScreenFunctions()');a=f.index('    switch (previewState)',a);b=f.index('    ip->setButtonObj(interfaceButton7',a)
with (o/'preview-ui.inc').open('a',encoding='utf-8') as out:out.write('\nvoid cSampleEditor::registerParamsPreview(){'+f[a:b]+'}\n')

parts=[body(f,'bool cSampleEditor::canUseMainAction()'),body(f,'static void functApply()\n{')]
a=f.index('void cSampleEditor::refreshEffectList()');a=f.index('    display.setControlColors(',a);b=f.index('    display.refreshControl(label[5]);',a);parts.append('void cSampleEditor::refreshMainActionColor(){'+f[a:b]+'}')
a=f.index('static void functSave()\n{');b=f.index('    uint32_t processedLength',a);parts.append(f[a:b]+'    ++nativeApplyCalls;\n}')
(o/'settings-ui.inc').write_text('\n'.join(parts),encoding='utf-8')
