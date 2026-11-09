#define _CRT_SECURE_NO_WARNINGS
#define TEXT_SCROLL_SPEED_DIV 30
#define TEXT_SCROLL_SPACER 30
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#define TRACKER_CE_ENABLE_SD_MONO 1
#define TRACKER_CE_ENABLE_SD_EDITS 1
#define GUI_HANDLE_SYNTH 0
constexpr unsigned EDIT_BUFF_SIZE=1048576,kSampleInstrumentsCount=48,MAX_16BIT=65535;constexpr int kEffectPageParams=4;
#include "ids.inc"
#include "capabilities.inc"
#include "styles.inc"
#include "transition-list-types.inc"
template<class T>T clamp(T a,T b,T c){return std::clamp(a,b,c);}
struct strFont{uint8_t data[148]{};int16_t height=16,handle=0,width=8;}fonts[5];
#include "width.inc"
constexpr int SRC_ALPHA=1,ZERO=0,ONE_MINUS_SRC_ALPHA=2,RECTS=1,LINE_STRIP=2,LINES=3,BITMAPS=4,NEAREST=0,BORDER=0,OPT_CENTERY=1,kRedFrameWidth=16;
static std::vector<std::string>drawn;static void badText();
static void API_CMD_TEXT(int,int,int,int,const char*t,int=0){if(!t)badText();drawn.emplace_back(t,std::strlen(t));}
#define API_COLOR(...) ((void)0)
#define API_LINE_WIDTH(...) ((void)0)
#define API_BEGIN(...) ((void)0)
#define API_VERTEX2F(...) ((void)0)
#define API_END(...) ((void)0)
#define API_BLEND_FUNC(...) ((void)0)
#define API_SAVE_CONTEXT(...) ((void)0)
#define API_RESTORE_CONTEXT(...) ((void)0)
#define API_SCISSOR_XY(...) ((void)0)
#define API_SCISSOR_SIZE(...) ((void)0)
#define API_CMD_GRADIENT(...) ((void)0)
#define API_BITMAP_SOURCE(...) ((void)0)
#define API_BITMAP_LAYOUT(...) ((void)0)
#define API_BITMAP_SIZE(...) ((void)0)
struct{int address=0,format=0,linestride=0,height=0,width=0;}bitmaps[256];
class Timer{public:unsigned millis(){return 0;}}testTimer;
class Tracker{public:static Tracker*getInstance(){static Tracker t;return &t;}Tracker*getPlatform(){return this;}Timer*getTimer(){return &testTimer;}};
class cList{public:strList*list=nullptr;int16_t listPosition=0;uint16_t barPos=0,textListPos=0;uint8_t disableBar=0;int8_t listAnimationStep=0;uint32_t textScrollTime=0,textScrollTimeStart=0;uint8_t textScrollItem=255;int posX=601,posY=29,width=197,height=394;unsigned style=0,textStyle=0;uint32_t colors[7]{};strFont*font=&fonts[0];bool autoRefresh=false,highPrioRefresh=false;void setData(void*);void setValue(int);void update();}widget;
static struct{struct{uint32_t red=0xff0000,activeButtonLabelsColors[4]{},inactiveLabelsColors[4]{};}theme;auto&getTheme(){return theme;}bool visible=true;void setControlValue(int id,int val){if(id==1)widget.setValue(val);}void setControlData(int id,void*d){if(id==1)widget.setData(d);}void setControlShow(int id){if(id==1)visible=true;}void setControlHide(int id){if(id==1)visible=false;}void refreshControl(int id){if(id==1&&visible){drawn.clear();widget.update();}}void setControlText(int,const char*){}void setControlText2(int,const char*){}void setControlColors(int,const uint32_t*){}void synchronizeRefresh(){}}display;
struct Instrument{bool isActive=true;struct{unsigned length=0;int16_t*address=nullptr;char file_name[32]{};}sample;float tuneResample=0;};
struct Memory{enum Channel{kLeft,kRight};int16_t*address[2]{};bool isStreamed=false;};
static struct{struct{unsigned lastUsedInstrument=0;}values;Instrument instrument[48];Memory sampleMemory[48];}mtProject;
struct mtEffect{enum Channel{kLeft,kRight};enum class LoadingResult{loadingSuccessful};};
struct Effect{unsigned length=0;struct{bool isEnable=false;}undoParams;void clearLoadedData(){}void clearProcessedData(){}void setBaseSampleLength(unsigned n){length=n;}void changeSelectionRange(unsigned,unsigned){}void setParameter(float&,unsigned){}void update(){}bool isDataLoaded(){return true;}mtEffect::LoadingResult startLoad(unsigned){return mtEffect::LoadingResult::loadingSuccessful;}int16_t*getAddresToPolyphonicPlay(int){return nullptr;}unsigned getLengthToPolyphonicPlay(){return length;}}effects[editorEffectMax];
static Effect*sampleEditorEffect[editorEffectMax];static int sampleEditorModular=0;
constexpr unsigned kBuiltInEffectsCount=editorEffectModular;
namespace PtfxManager{static unsigned plugins=0;static char pluginNames[128][32]{};static char*names[kBuiltInEffectsCount+128]{};void scanEffects(){for(unsigned i=0;i<kBuiltInEffectsCount;++i)names[i]=effectNamesLabels[i];for(unsigned i=0;i<plugins;++i)names[kBuiltInEffectsCount+i]=pluginNames[i];}unsigned getTotalEffectListCount(){return kBuiltInEffectsCount+plugins;}char**getEffectListNames(){return names;}bool loadPlugin(unsigned i,int){assert(i<plugins);return true;}int getActiveHeader(){return 0;}}
static unsigned released=0;static void releaseLargePreview(){++released;}
namespace sampleedit{void setUndoContext(unsigned){}}
class Sequencer{public:enum{SEQ_STATE_STOP,midiInVoiceMode_SampleEditor};int getSeqState(){return SEQ_STATE_STOP;}void setMidiInVoiceMode(int){}}sequencer;
static struct{void blockDelayRefresh(){}void disableLineInImpact(){}}engine;static struct{void hide(){}}songTimer;
static struct{bool isPopupVisible(){return false;}}mtMenuPopup;
constexpr int buttonPress=1,buttonRelease=0,stepPopupNone=0,stepPopupInstr=1,effectButton=3,interfaceButton5=5;
static struct{int state=0;int getStepPopupState(){return state;}void showStepPopup(int,int){state=1;}void hideStepPopups(){state=0;}}mtPopups;
static struct{bool importSampleFromSampleEditor(int16_t*,int16_t*,unsigned,unsigned){return true;}}newFileManager;
struct Input{void blockAllInputs(){}}input;
struct Params{int paramsNumber=2;};struct Parameters{float parameter[12]{};};static Parameters effectDefaultParamsTable[editorEffectMax]{};
class cSampleEditor{public:Instrument copied;Instrument*editorInstrument=&copied;Memory sampleEditorSampleMemory;struct{int16_t*address=nullptr;unsigned length=0;}spectrumParams[2];Effect*currentEffect=&effects[0],*lastEffect=nullptr;unsigned selectedEffectListIdx=0,currentEffectIdx=0,currentModularPluginIdx=0;strList effectListData{};uint8_t sdEffectIds[editorEffectMax]{};char*sdEffectNames[editorEffectMax]{};unsigned sdEffectCount=0;Parameters effectDisplayParamsToAdjust[editorEffectMax]{};Params params;enum{mainScreen,effectScreen};int screenType=mainScreen,effectScreenPage=0,selectedPlace[2]{};enum{previewStatePreview};int previewState=0;bool confirmedDataIsChanged=false,convertedToMono=false,undoWasMono=false,isLoadedData=false,reloadOnEndSaving=false;int moduleRefresh=0,effectList=1,label[8]{};unsigned beforeReloadInstrumentIdx=0,afterReloadInstrumentIdx=0;char paramsScreenLabel[40]{};Input*ip=&input;struct{uint16_t startPoint=0,endPoint=65535;}selection;
 void start(uint32_t);void cancelGlobalPopups();void refreshEffectList();void showEffectList();bool usesSdEditing()const;unsigned effectListRow()const;void configureEffectList();void modSelectedEffect(int16_t);int getParamsPagesCount();int getCurrentPageParams();bool areThereMoreThanOneParamsPage();
 void hideUselessControls(){display.setControlHide(effectList);}void handleStartupWithPopupAndMenu(){}void showPopupTooLongSampleWindow(){}void setTooLongSampleFunctions(){}void showPopupSeqWindow(){}void setStopSeqFunctions(){}void setMainScreenFunctions(){}void setCommonFunctions(){}void loadModularEffectUI(int){}void reloadSpectrumData(){}void prepareDisplayDataMainScreen(){}void showMainScreen(){refreshEffectList();}void reloadCurrentEffect(){currentEffect=sampleEditorEffect[currentEffectIdx];}Params&getCurrentEffectDisplayParams(){params.paramsNumber=currentEffectIdx==editorEffectEQ?9:2;return params;}bool isCurrentSampleStereo(){return false;}bool isParametersScreen(){return currentEffectIdx!=editorEffectCut&&currentEffectIdx!=editorEffectReverse&&currentEffectIdx!=editorEffectFadeIn&&currentEffectIdx!=editorEffectFadeOut;}bool canUseMainAction(){return true;}void showTitleBar(const char*){}void refreshSdIndicator(const char*){}
}editor;static cSampleEditor*SE=&editor;
static void badText(){std::fprintf(stderr,"INVALID TEXT: slot=%u SD=%d length=%u lines=%u start=%d listPosition=%d textListPos=%u barPos=%u selectedRaw=%u currentEffect=%u\n",mtProject.values.lastUsedInstrument,editor.usesSdEditing(),widget.list->length,widget.list->linesCount,widget.list->start,widget.listPosition,widget.textListPos,widget.barPos,editor.selectedEffectListIdx,editor.currentEffectIdx);}
#include "production.inc"
#include "transition-native.inc"
static void settle(){for(unsigned i=0;i<1000;++i){display.refreshControl(1);if(widget.list->start==widget.listPosition&&!widget.highPrioRefresh)return;}assert(false);}
static void check(){auto&e=editor;assert(e.usesSdEditing()==(mtProject.instrument[mtProject.values.lastUsedInstrument].sample.length>EDIT_BUFF_SIZE/2));assert(widget.list==&e.effectListData);assert(widget.listPosition==int(e.effectListRow()));assert(widget.list->start==widget.listPosition);assert(e.currentEffectIdx==(e.selectedEffectListIdx<kBuiltInEffectsCount?e.selectedEffectListIdx:editorEffectModular));assert(e.effectListData.data==(e.usesSdEditing()?e.sdEffectNames:PtfxManager::getEffectListNames()));assert(widget.textListPos+std::min(e.effectListData.length,e.effectListData.linesCount)<=e.effectListData.length);assert(e.currentEffect==sampleEditorEffect[e.currentEffectIdx]);if(e.currentEffectIdx==editorEffectModular)assert(e.currentModularPluginIdx<PtfxManager::plugins);for(unsigned i=0;i<e.effectListData.length;++i){auto id=e.usesSdEditing()?e.sdEffectIds[i]:i;char*expected=id<kBuiltInEffectsCount?effectNamesLabels[id]:PtfxManager::pluginNames[id-kBuiltInEffectsCount];assert(e.effectListData.data[i]&&std::strcmp(e.effectListData.data[i],expected)==0);}for(const auto&text:drawn){bool found=false;for(unsigned i=0;i<e.effectListData.length;++i)found|=text==e.effectListData.data[i];assert(found);}}
static void change(unsigned slot){functInstrument(buttonPress);mtProject.values.lastUsedInstrument=slot;functInstrument(buttonRelease);settle();check();}
int main(){for(unsigned i=0;i<editorEffectMax;++i)sampleEditorEffect[i]=&effects[i];mtProject.instrument[0].sample.length=EDIT_BUFF_SIZE*2;mtProject.instrument[47].sample.length=1024;std::strcpy(mtProject.instrument[0].sample.file_name,"long streamed");std::strcpy(mtProject.instrument[47].sample.file_name,"short RAM");editor.effectListData.length=kBuiltInEffectsCount;editor.effectListData.linesCount=14;editor.effectListData.data=const_cast<char**>(effectNamesLabels);widget.setData(&editor.effectListData);
 for(unsigned i=0;i<128;++i)std::snprintf(PtfxManager::pluginNames[i],32,"Plugin %03u",i);
 unsigned cycles=0;
 for(unsigned plugins:{0u,3u,128u})for(unsigned first:{0u,47u}){
  if(std::getenv("TRACKER_TRANSITION_REPRO")&&first!=47)continue;
  PtfxManager::plugins=plugins;mtProject.values.lastUsedInstrument=first;editor.start(0);settle();check();
  for(unsigned n=0;n<50;++n){
   editor.screenType=n%2;editor.effectScreenPage=n%3;
   const auto choose=[&](unsigned row){while(editor.effectListRow()) {editor.modSelectedEffect(-5);settle();}for(unsigned i=0;i<row;++i){editor.modSelectedEffect(1);settle();}check();};
   choose(n==0?editor.effectListData.length-1:n%editor.effectListData.length);
   change(first==0?47:0);assert(editor.effectListData.length==(editor.usesSdEditing()?12:kBuiltInEffectsCount+plugins));
   editor.screenType=(n+1)%2;editor.effectScreenPage=(n+1)%3;
   choose(n==0?editor.effectListData.length-1:(n*3)%editor.effectListData.length);
   change(first);assert(editor.effectListData.length==(editor.usesSdEditing()?12:kBuiltInEffectsCount+plugins));++cycles;
  }
 }
 std::printf("PASS real transition pipeline: instrument press/release -> popup cancel -> complete start -> real cList animation/render; %u cycles / %u switches, both directions, slots/names/pages/0-3-128 plugins/all strings/indices checked\n",cycles,cycles*2);
}
