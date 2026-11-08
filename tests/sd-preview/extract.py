from pathlib import Path
import re,sys,subprocess
r=Path(sys.argv[1]);o=Path(sys.argv[2]);o.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,str(r/'tests/stereo-mono/extract.py'),str(r),str(o)],check=True)
def clean(s):
 s=re.sub(r'^#include.*$','',s,flags=re.M)
 s=re.sub(r'^#(?:ifndef|define) EFFECTOR_.*$','',s,flags=re.M)
 s=re.sub(r'^#endif /\* EFFECTOR_.*$','',s,flags=re.M)
 return s
names=['Normalizer','Amplifier','Overdrive','Bitcrusher','EQ','Limiter','Compresor','StereoMono']
(o/'preview-dsp.inc').write_text('\n'.join(clean((r/('core/effector/mtEffect'+n+'.'+ext)).read_text(encoding='utf-8')) for n in names for ext in ['h','cpp']),encoding='utf-8')
s=(r/'core/effector/mtSampleEditorEngine.h').read_text(encoding='utf-8');a=s.index('enum enSampleEditorEffect');b=s.index('};',a)+2;(o/'preview-enums.inc').write_text(s[a:b],encoding='utf-8')
for name,path in [('preview-copies.inc','core/effector/SamplePreviewEffects.cpp'),('preview-spec.inc','core/effector/SamplePreviewEffects.h'),('preview-native.inc','core/FileManager/imx/SamplePreview.cpp')]:
 (o/name).write_text(clean((r/path).read_text(encoding='utf-8')),encoding='utf-8')
s=(r/'core/effector/mtEffect.cpp').read_text(encoding='utf-8');a=s.index('void mtEffect::bindPreviewBuffers(');b=s.index('\n#endif',a);(o/'preview-bind.inc').write_text(s[a:b],encoding='utf-8')

a=s.index('bool mtEffect::hasPreviewWorkspaceConflict() const');b=s.index('\n#endif',a);(o/'preview-conflict.inc').write_text(s[a:b],encoding='utf-8')

f=(r/'core/interface/sampleEditor/fSampleEdit.cpp').read_text(encoding='utf-8')
for name in ['start([[maybe_unused]]','stop()','modSelectedEffect(','modParameter(','setDefaultParameter(','modStartPoint(','modEndPoint(']:
 a=f.index('cSampleEditor::'+name);b=f.index('\n}',a);assert 'releaseLargePreview();' in f[a:b],name
assert 'samplepreview::ready()||!samplepreview::valid()' in f

(o/'original-dynamics.inc').write_text('\n'.join(clean((r/('tests/ce-editor/reference/core/effector/mtEffect'+n+'.'+ext)).read_text()) for n in ['EQ','Limiter','Compresor'] for ext in ['h','cpp']),encoding='utf-8')
