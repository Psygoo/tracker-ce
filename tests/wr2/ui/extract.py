from pathlib import Path
import sys,json,hashlib
r=Path(sys.argv[1]);o=Path(sys.argv[2]);o.mkdir(parents=True,exist_ok=True);manifest=[]
def extract(path,sig):
 s=(r/path).read_text();a=s.index(sig);b=s.index('{',a);e=b+1;d=1
 while d:d+=(s[e]=='{')-(s[e]=='}');e+=1
 body=s[a:e];manifest.append(dict(path=path,signature=sig,sha256=hashlib.sha256(body.encode()).hexdigest()));return body
p='core/interface/samplePlayback/fSamplePlay.cpp'
bodies=[extract(p,sig) for sig in ['bool cSamplePlayback::wr2Page()const','void cSamplePlayback::wr2Finish(','void cSamplePlayback::wr2DismissNotice()','void cSamplePlayback::wr2Refresh()','bool cSamplePlayback::wr2Button(']]
bodies.append(extract(p,'static void functButton5()\n{'))
bodies.append(extract('core/interface/samplePlayback/gSamplePlay.cpp','void cSamplePlayback::activateLabelsBorder()'))
(o/'ui.inc').write_text('\n'.join(bodies))
(o/'elapsedMillis.h').write_text('''#pragma once
#include <cstdint>
struct elapsedMillis{unsigned value=0;elapsedMillis&operator=(unsigned n){value=n;return *this;}operator unsigned()const{return value;}};
''')
(o/'Platform.h').write_text('''#pragma once
#include <cstdint>
class Leds{public:unsigned calls=0;uint8_t level[48]{};void setLed(uint8_t n,uint8_t state,uint8_t value){++calls;level[n]=state?value:0;}};
''')
(o/'manifest.json').write_text(json.dumps(manifest,indent=2))
print('Extracted four complete production UI methods; compiling actual native pad manager')

s=(r/'core/interface/core/interfaceDefs.h').read_text();a=s.index('enum buttonStates');b=s.index('};',a)+2
(o/'buttons.inc').write_text(s[a:b])
(o/'markers.inc').write_text(extract('core/libs/NewDisplay/slice.cpp','void cSlicePoints::update()'))
s=(r/p).read_text();a=s.index('static void functEncoder(int16_t value)\n{');b=s.index('#endif',a)+6
prefix=s[a:b].replace('functEncoder(', 'encoderUnderTest(',1)+'\n++nativeEncoderFallback;\n}'
(o/'encoder.inc').write_text(prefix)
(o/'manifest.json').write_text(json.dumps(manifest,indent=2))

s=(r/'core/FileManager/fileManagerInstrument.cpp').read_text();a=s.index('bool cFileManager::loadInstrumentFormFileStruct(');a=s.index('#if TRACKER_CE_ENABLE_WR2',a);b=s.index('#endif',a)+6
(o/'header-invalidation.inc').write_text(s[a:b])
(o/'native-invalidate.inc').write_text(extract('core/AudioEngine/NativeUi.cpp','void invalidate(strInstrument*'))
(o/'manifest.json').write_text(json.dumps(manifest,indent=2))

# Execute the native validated-PTI tail (metadata invalidation + native memcpy).
s=(r/'core/FileManager/fileManagerInstrument.cpp').read_text(encoding='utf-8');a=s.index('bool cFileManager::loadInstrumentFormFileStruct(');a=s.index('#if TRACKER_CE_ENABLE_WR2',a);b=s.index('    return true;',a)+len('    return true;')
(o/'pti-tail.inc').write_text(s[a:b],encoding='utf-8')
s=(r/'core/FileManager/SampleImport.cpp').read_text(encoding='utf-8');a=s.index('if (mtProject.instrument[instrIdx].isActive == 0)');b=s.index('{',a);e=s.index('}',b)+1
(o/'wav-active-guard.inc').write_text(s[a:e],encoding='utf-8')

arrow=[]
for name in ['functLeft','functRight']:
 s=(r/p).read_text();a=s.index('static void '+name+'()\n{');b=s.index('#endif',a)+6
 arrow.append(s[a:b]+'\n}')
(o/'arrows.inc').write_text('\n'.join(arrow))

# Execute actual entry prefix (through native page reset and instrument binding),
# complete stop and module-switch handler. Drawing/other modules are mocked.
s=(r/p).read_text();a=s.index('void cSamplePlayback::start(uint32_t options)');b=s.index('        loadedInstrumentType = editorInstrument->sample.type;',a)
(o/'lifecycle.inc').write_text(s[a:b]+'\n    }\n}\n'+extract(p,'void cSamplePlayback::stop()')+'\n'+extract(p,'static void functSwitchModule(uint8_t button)\n{'))
(o/'manifest.json').write_text(json.dumps(manifest,indent=2))
