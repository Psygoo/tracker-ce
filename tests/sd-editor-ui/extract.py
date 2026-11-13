from pathlib import Path
import sys,re
r=Path(sys.argv[1]);o=Path(sys.argv[2]);o.mkdir(parents=True,exist_ok=True)
def body(s,start):
 a=s.index(start);b=s.index('{',a);e=b+1;depth=1
 while depth:depth+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
f=(r/'core/interface/sampleEditor/fSampleEdit.cpp').read_text();g=(r/'core/interface/sampleEditor/gSampleEdit.cpp').read_text();h=(r/'core/interface/sampleEditor/sampleEditor.h').read_text();engine=(r/'core/effector/mtSampleEditorEngine.h').read_text()
(o/'ids.inc').write_text(body(engine,'enum enSampleEditorEffect')+';'+h[h.index('char* const effectNamesLabels'):h.index('struct strEffectDisplayParams')])
(o/'capabilities.inc').write_text('\n'.join(body(h,'inline bool '+n) for n in ['isMonoEffect','isSdMonoEffect','isSdRegionEffect']))
(o/'production.inc').write_text('\n'.join(body(f,n) for n in ['bool cSampleEditor::usesSdEditing','unsigned cSampleEditor::effectListRow','void cSampleEditor::configureEffectList','void cSampleEditor::modSelectedEffect','int cSampleEditor::getParamsPagesCount','int cSampleEditor::getCurrentPageParams','bool cSampleEditor::areThereMoreThanOneParamsPage']))
import subprocess
for name in ['core/interface/core/titleBar.cpp','core/interface/core/titleBar.h']:
 assert (r/name).read_bytes()==subprocess.check_output(['git','-C',str(r),'show','HEAD:'+name]),'Native TitleBar must remain byte-identical'
native=subprocess.check_output(['git','-C',str(r),'show','HEAD:core/interface/sampleEditor/gSampleEdit.cpp']).decode()
assert body(g,'void cSampleEditor::showTitleBar(')==body(native,'void cSampleEditor::showTitleBar(')
i=(r/'core/interface/core/interface.cpp').read_text();assert i.index('sampleEditor.initSdIndicatorControl();')<i.index('titleBar.initControls();')
assert 'refreshSdIndicator(paramsScreenLabel)' in body(g,'void cSampleEditor::showMainScreen(')
assert 'refreshSdIndicator(paramsScreenLabel)' in body(g,'void cSampleEditor::showEffectParamsScreen(')
assert 'display.setControlHide(sdModeLabel)' in body(f,'void cSampleEditor::stop')
tb=(r/'core/interface/core/titleBar.cpp').read_text();label=(r/'core/libs/NewDisplay/label.cpp').read_text();disp=(r/'core/libs/NewDisplay/display.cpp').read_text()
(o/'graphics-native.inc').write_text('\n'.join(body(label,n) for n in ['void cLabel::setStyle(','void cLabel::update()'])+'\n'+'\n'.join(body(disp,n) for n in ['void cDisplay::setControlStyle(','void cDisplay::setControlShow(','void cDisplay::setControlHide(','void cDisplay::step3()'])+'\n'+'\n'.join(body(tb,n) for n in ['void TitleBar::initControls()','void TitleBar::show(', 'void TitleBar::setTitle(', 'void TitleBar::showCurrentInstrumentOnRightLabel()'])+'\n'+'\n'.join(body(g,n) for n in ['void cSampleEditor::showTitleBar(', 'void cSampleEditor::initSdIndicatorControl()', 'void cSampleEditor::refreshSdIndicator(']))
ds=(r/'core/libs/NewDisplay/displayStructs.h').read_text();(o/'styles.inc').write_text(body(ds,'enum controlsStyle')+';')
dc=(r/'core/libs/NewDisplay/displayControls.cpp').read_text()
(o/'width.inc').write_text(body(dc,'uint16_t getTextWidth(')+'\n'+body(dc,'uint16_t getMaxTextLengthThatFitSpecificWidth('))
label=(r/'core/libs/NewDisplay/label.cpp').read_text();a=label.index('    int16_t border_x',label.index('void cLabel::update()'));b=label.index('    if (style & controlStyleBackground)',a)
(o/'label-y.inc').write_text('static int actualTextY(int16_t posX,int16_t posY,int16_t width,int16_t height,int value,uint32_t style,const strFont*font){'+label[a:b]+'(void)border_x;(void)border_y;(void)text_x;return text_y;}')
arrays=[]
for idx,file in [(3,'RobotoMono-Bold_17_L4.cpp'),(4,'RobotoMono-Regular_12_L4.cpp')]:
 font=(r/'core/libs/fonts'/file).read_text();raw=b''.join(bytes([int(v)])*int(c) for v,c in re.findall(r'\{\s*(\d+)\s*,\s*(\d+)\s*\}',font))
 arrays.append('static const uint8_t font'+str(idx)+'[]={'+','.join(str(x) for x in raw[:148])+'};')
(o/'fonts.inc').write_text('\n'.join(arrays))

li=(r/'core/libs/NewDisplay/list.cpp').read_text()
(o/'transition-native.inc').write_text('\n'.join(body(li,n) for n in ['void cList::setData(', 'void cList::setValue(', 'void cList::update()'])+'\n'+'\n'.join(body(f,n) for n in ['void cSampleEditor::start(', 'void cSampleEditor::cancelGlobalPopups(', 'static void functInstrument(uint8_t state)\n{', 'void cSampleEditor::refreshEffectList()'])+'\n'+body(g,'void cSampleEditor::showEffectList()'))
(o/'transition-list-types.inc').write_text((r/'core/libs/NewDisplay/list.h').read_text().split('class cList')[0].split('#define LIST_LENGTH_MAX')[1].split('50',1)[1].replace('#define BITMAP_ICON_OFFSET 5',''))

# Historical counterexample: reproduce the pre-fix descriptor-only update without touching production files.
import os
if os.environ.get('TRACKER_TRANSITION_REPRO'):
 p=o/'production.inc';text=p.read_text();old=body(text,'void cSampleEditor::configureEffectList(')
 marker='    effectListData.linesCount=std::min<uint16_t>(effectListData.length,14);'
 before=old[:old.index(marker)]+marker+'effectListData.start=0;\n}'
 p.write_text(text.replace(old,before))
