#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#define TRACKER_CE_ENABLE_SD_MONO 1
#define TRACKER_CE_ENABLE_SD_EDITS 1
constexpr unsigned EDIT_BUFF_SIZE=1048576;
#include "ids.inc"
#include "capabilities.inc"
template<class T>T clamp(T a,T b,T c){return std::clamp(a,b,c);}
constexpr unsigned kBuiltInEffectsCount=editorEffectModular;
constexpr int kEffectPageParams=4;
namespace PtfxManager{static unsigned loads=0;static unsigned lastPlugin=0;static char*names[editorEffectMax+3]{};static unsigned getTotalEffectListCount(){return kBuiltInEffectsCount+3;}static char**getEffectListNames(){return names;}static bool loadPlugin(unsigned p,int){++loads;lastPlugin=p;return true;}static int getActiveHeader(){return 0;}}
struct Instrument{bool isActive=true;struct{unsigned length=16;}sample;};
struct Effect{void clearProcessedData(){}void setBaseSampleLength(unsigned){}void changeSelectionRange(unsigned,unsigned){}void setParameter(float&,unsigned){}}effects[editorEffectMax];
static Effect*sampleEditorEffect[editorEffectMax];static int sampleEditorModular=0;static unsigned releases=0;static void releaseLargePreview(){++releases;}
static struct{void setControlData(int,void*){}}display;
struct cSampleEditor{
 Instrument instrument;Instrument*editorInstrument=&instrument;
 int effectList=1;unsigned selectedEffectListIdx=0,currentEffectIdx=0,currentModularPluginIdx=0;Effect*currentEffect=&effects[0];
 struct{unsigned length=0,linesCount=0,start=0;char**data=nullptr;}effectListData;
 char*sdEffectNames[editorEffectMax]{};uint8_t sdEffectIds[editorEffectMax]{};unsigned sdEffectCount=0;
 struct{unsigned startPoint=0,endPoint=65535;}selection;
 struct{float parameter[12]{};}effectDisplayParamsToAdjust[editorEffectMax];
 struct Params{int paramsNumber=0;}params;unsigned effectScreenPage=0;
 enum{previewStatePreview=0};int previewState=0;
 bool usesSdEditing()const;unsigned effectListRow()const;void configureEffectList();void modSelectedEffect(int16_t);void showTitleBar(const char*);int getParamsPagesCount();int getCurrentPageParams();bool areThereMoreThanOneParamsPage();
 void reloadCurrentEffect(){}void loadModularEffectUI(int){}void refreshEffectList(){assert(effectListRow()<effectListData.length);}
 Params&getCurrentEffectDisplayParams(){params.paramsNumber=currentEffectIdx==editorEffectEQ?9:2;return params;}
};
#include "production.inc"
int main(){
 for(unsigned i=0;i<editorEffectMax;++i)sampleEditorEffect[i]=&effects[i];
 cSampleEditor e;
 const std::vector<unsigned>expected={editorEffectNormalizer,editorEffectCut,editorEffectReverse,editorEffectAmplifier,editorEffectOverdrive,editorEffectBitcrusher,editorEffectEQ,editorEffectLimiter,editorEffectCompresor,editorEffectFadeIn,editorEffectFadeOut,editorEffectStereoMonoSD};
 for(unsigned initial=0;initial<PtfxManager::getTotalEffectListCount();++initial){
  e.selectedEffectListIdx=initial;e.instrument.sample.length=EDIT_BUFF_SIZE;e.configureEffectList();assert(e.sdEffectCount==expected.size());e.currentEffectIdx=e.selectedEffectListIdx;e.currentEffect=sampleEditorEffect[e.currentEffectIdx];
  for(unsigned i=0;i<expected.size();++i){assert(e.sdEffectIds[i]==expected[i]);assert(e.sdEffectNames[i]==effectNamesLabels[expected[i]]);}
  assert(std::find(expected.begin(),expected.end(),e.selectedEffectListIdx)!=expected.end());
  for(int n=0;n<50;++n){unsigned before=e.effectListRow();e.modSelectedEffect(1);assert(e.effectListRow()==std::min(before+1,unsigned(expected.size()-1)));assert(e.currentEffect==sampleEditorEffect[e.currentEffectIdx]);}
  for(int n=0;n<50;++n){unsigned before=e.effectListRow();e.modSelectedEffect(-1);assert(e.effectListRow()==(before?before-1:0));}
  e.modSelectedEffect(5);assert(e.selectedEffectListIdx==expected[5]);e.configureEffectList();assert(e.selectedEffectListIdx==expected[5]);
  e.instrument.sample.length=EDIT_BUFF_SIZE/2;e.configureEffectList();assert(!e.usesSdEditing()&&e.effectListData.length==PtfxManager::getTotalEffectListCount());assert(e.effectListData.data==PtfxManager::getEffectListNames());
  for(unsigned row=0;row<PtfxManager::getTotalEffectListCount();++row){e.selectedEffectListIdx=0;e.modSelectedEffect(0);while(e.selectedEffectListIdx<row)e.modSelectedEffect(1);assert(e.selectedEffectListIdx==row&&e.effectListRow()==row);if(row>=kBuiltInEffectsCount)assert(e.currentModularPluginIdx==row-kBuiltInEffectsCount);}
 }
 std::puts("PASS SD effect list navigation unchanged: all original indices, RAM/SD, plugins, boundaries");
}
