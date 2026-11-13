#define _USE_MATH_DEFINES
#define _CRT_SECURE_NO_WARNINGS
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>
#define TRACKER_CE_ENABLE_SD_MONO 1
#include "../../core/effector/sampleProcessing/NativeDrive.h"
#define __attribute__(x)
#include "../../editor-fx-sdk/tracker_plugin.h"
#include "../../core/effector/sampleProcessing/PcmEffects.h"
static unsigned checks=0;
#define CHECK(v) do{++checks;if(!(v)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#v);std::abort();}}while(0)
constexpr int MAX_SIGNED_16BIT=32767,MIN_SIGNED_16BIT=-32768;
template<class T>T map(T x,T a,T b,T c,T d){return (x-a)*(d-c)/(b-a)+c;}
class mtEffect{public:
 static constexpr int kChannels=2;struct Part{int16_t*addr;uint32_t length;};struct Buffer{Part area,selection;}confirmed[2]{},processed[2]{};
 virtual ~mtEffect()=default;virtual void setParameter(float&,int)=0;virtual bool startProcess()=0;virtual int32_t processBlock()=0;virtual bool getProcessState()=0;virtual uint32_t getExpectedProcessLength()=0;
};
#pragma warning(push)
#pragma warning(disable:4244 4100)
namespace baseline{
#define TRACKER_CE_ENABLE_SD_EDITS 0
#include "original.inc"
#include "original-plugin.inc"
}
#undef TRACKER_CE_ENABLE_SD_EDITS
#define TRACKER_CE_ENABLE_SD_EDITS 1
namespace current{
#include "current.inc"
#include "current-plugin.inc"
#include "mono.inc"
}
#pragma warning(pop)
#define strcasecmp _stricmp
#include "dedup.inc"
static int16_t sample(unsigned channel,unsigned at){return int16_t((at*(channel?37u:109u)+511u)&65535);}
static void plugins(){
 PtfxHostApi host{};host.powf=::powf;int16_t left[35],right[35],a[35],b[35],c[35],d[35];for(unsigned n=0;n<35;++n){left[n]=n%2?-32768:32767;right[n]=n%3?int16_t(n*983):32767;}
 const int16_t*input[2]{left,right};int16_t*outA[2]{a,b},*outB[2]{c,d};
 for(unsigned channels:{1u,2u})for(unsigned source=0;source<4;++source)for(float db:{-12.0f,-3.5f,0.0f,6.0f}){
  baseline::plugin_init(nullptr,0,&host);current::plugin_init(nullptr,0,&host);baseline::plugin_set_param(0,float(source));baseline::plugin_set_param(1,db);current::plugin_set_param(0,float(source));current::plugin_set_param(1,db);
  CHECK(baseline::plugin_process_block(input,outA,35,uint8_t(channels))==35);CHECK(current::plugin_process_block(input,outB,35,uint8_t(channels))==35);CHECK(!std::memcmp(a,c,sizeof(a)));if(channels==2)CHECK(!std::memcmp(b,d,sizeof(b)));
 }
 PtfxHeader h{};h.flags=PTFX_FLAG_CONVERT_TO_MONO;std::strcpy(h.display_name,"Stereo to Mono");CHECK(bundledMono(h,"renamed.ptfx"));std::strcpy(h.display_name,"My Custom Mono");CHECK(!bundledMono(h,"custom.ptfx"));CHECK(bundledMono(h,"stereo_to_mono.ptfx"));h.flags=0;CHECK(!bundledMono(h,"stereo_to_mono.ptfx"));
 std::puts("PASS original plugin parity: four Sources, mono/stereo, Gain -12..+6 dB, extrema; canonical plugin deduplicated, custom effects retained");
}
template<class A,class B>void gainCompare(bool normalize){
 for(unsigned length:{1u,2u,1023u,8192u,8193u})for(float parameter:normalize?std::vector<float>{0,50,100}:std::vector<float>{.01f,.5f,1,2,5}){
  std::vector<int16_t>input[2],old[2],now[2];A original;B updated;
  for(unsigned c=0;c<2;++c){input[c].resize(length);old[c].resize(length);now[c].resize(length);for(unsigned n=0;n<length;++n)input[c][n]=sample(c,n);original.confirmed[c]={{input[c].data(),length},{input[c].data(),length}};original.processed[c]={{old[c].data(),length},{old[c].data(),length}};updated.confirmed[c]=original.confirmed[c];updated.processed[c]={{now[c].data(),length},{now[c].data(),length}};}
  original.setParameter(parameter,0);updated.setParameter(parameter,0);CHECK(original.startProcess()&&updated.startProcess());while(original.getProcessState())original.processBlock();while(updated.getProcessState())updated.processBlock();for(unsigned c=0;c<2;++c)CHECK(old[c]==now[c]);
 }
}
static void ramMono(){
 for(unsigned length:{1u,1023u,8192u,8193u})for(bool stereo:{false,true})for(unsigned source=0;source<4;++source)for(float db:{-12.0f,0.0f,6.0f}){
  std::vector<int16_t>input[2],output[2],expected[2];current::mtEffectStereoMono effect;const auto start=length>2?1u:0u,end=length>2?length-1:length;
  for(unsigned c=0;c<2;++c){input[c].resize(length);output[c].resize(length);expected[c].resize(length);for(unsigned i=0;i<length;++i)input[c][i]=sample(c,i);output[c]=input[c];effect.confirmed[c]={{input[c].data(),length},{input[c].data()+start,end-start}};effect.processed[c]={{output[c].data(),length},{output[c].data()+start,end-start}};}
  effect.setInputStereo(stereo);float selected=float(source);effect.setParameter(selected,0);effect.setParameter(db,1);CHECK(effect.startProcess());while(effect.getProcessState())effect.processBlock();
  PtfxHostApi host{};host.powf=::powf;baseline::plugin_init(nullptr,0,&host);baseline::plugin_set_param(0,float(source));baseline::plugin_set_param(1,db);const int16_t*in[2]{input[0].data()+start,input[1].data()+start};int16_t*out[2]{expected[0].data()+start,expected[1].data()+start};baseline::plugin_process_block(in,out,end-start,stereo?2:1);for(unsigned i=start;i<end;++i)CHECK(output[0][i]==expected[0][i]);
  for(unsigned c=0;c<2;++c)effect.confirmed[c].area={output[c].data(),length};effect.synchronizeOutputChannels();CHECK(output[0]==output[1]);for(unsigned i=0;i<start;++i)CHECK(output[0][i]==input[0][i]);for(unsigned i=end;i<length;++i)CHECK(output[0][i]==input[0][i]);
 }
 std::puts("PASS native RAM Mono vs original plugin: Source/Gain, partial region, mono input, full mono channel synchronization");
}


template<class A,class B>static void driveCompare(bool crusher){
 for(unsigned length:{1u,1023u,1024u,1025u,8191u,8192u,8193u,16387u})for(unsigned mode=0;mode<(crusher?4u:3u);++mode)for(float secondary:crusher?std::vector<float>{500,6300,8800,14700,44100}:std::vector<float>{.01f,.5f,1,5}){
  A original;B current;const float primary=crusher?float(1+mode*5):secondary;float setting=primary,other=crusher?secondary:float(mode);original.setParameter(setting,0);current.setParameter(setting,0);original.setParameter(other,1);current.setParameter(other,1);
  std::vector<int16_t> input[2],a[2],b[2],stream[2];
  for(unsigned c=0;c<2;++c){input[c].resize(length);a[c].resize(length);b[c].resize(length);stream[c].resize(length);for(unsigned n=0;n<length;++n)input[c][n]=sample(c,n);original.confirmed[c]={{input[c].data(),length},{input[c].data(),length}};original.processed[c]={{a[c].data(),length},{a[c].data(),length}};current.confirmed[c]=original.confirmed[c];current.processed[c]={{b[c].data(),length},{b[c].data(),length}};}
  CHECK(original.startProcess()&&current.startProcess());while(original.getProcessState())original.processBlock();while(current.getProcessState())current.processBlock();
  for(unsigned c=0;c<2;++c){sampleedit::CrushState hold;for(unsigned pos=0;pos<length;pos+=1024)for(unsigned i=pos;i<std::min(length,pos+1024);++i)stream[c][i]=crusher?sampleedit::crushSample(input[c][i],unsigned(primary),sampleedit::crusherStep(unsigned(other)),i,hold):sampleedit::overdriveSample(input[c][i],sampleedit::overdriveGain(primary,mode),mode);CHECK(a[c]==b[c]&&a[c]==stream[c]);}
 }
}
static void everyHoldStep(){
 for(unsigned step=1;step<=64;++step)for(unsigned bits:{1u,8u,16u}){
  constexpr unsigned length=16387;baseline::mtEffectBitcrusher original;std::vector<int16_t> input(length),a(length),b(length);for(unsigned n=0;n<length;++n)input[n]=sample(0,n);float depth=float(bits),rate=float(std::min(44100u,unsigned(44117.64706/step)));original.setParameter(depth,0);original.setParameter(rate,1);CHECK(sampleedit::crusherStep(unsigned(rate))==step);for(unsigned c=0;c<2;++c){original.confirmed[c]={{input.data(),length},{input.data(),length}};original.processed[c]={{a.data(),length},{a.data(),length}};}CHECK(original.startProcess());while(original.getProcessState())original.processBlock();sampleedit::CrushState hold;for(unsigned at=0;at<length;++at)b[at]=sampleedit::crushSample(input[at],bits,step,at,hold);CHECK(a==b);
 }
 std::puts("PASS native Bitcrusher: all 64 hold steps, 1/8/16 bits, 1024/8192 boundaries and two consecutive native blocks");
}
namespace ui {
#define PLATFORM_IMX 1
constexpr int editorEffectModular=19,PTFX_FLAG_CONVERT_TO_MONO=8;
struct mtEffect{enum Channel{kLeft,kRight};int16_t*data[2]{};uint32_t length=0;bool workspaceConflict=false;bool hasPreviewWorkspaceConflict()const{return workspaceConflict;}int16_t*getAddresToPolyphonicPlay(Channel c){return data[c];}uint32_t getLengthToPolyphonicPlay(){return length;}};
struct Input;
struct cSampleEditor{struct Instrument{struct{uint32_t length=1;}sample;bool isActive=true;}instrument;Instrument*editorInstrument=&instrument;Input*ip=nullptr;int label[8]{};enum{previewStatePreview,previewStatePlay,previewStateStop};int previewState=previewStatePreview;bool processingInProgress=false,playingInProgress=false;unsigned processingProgress=0;struct{uint16_t startPoint=0,endPoint=65535;}selection;void hideProgressPopup(){}void restoreFunctions(){}void showPopupProcessing(){}bool applyAllowed=true;bool canApplyCurrentEffect()const{return applyAllowed;}bool canPreviewCurrentEffect()const;bool directApply=false;bool isParametersScreen(){return directApply;}bool canUseMainAction();enum{mainScreen,effectScreen};int screenType=mainScreen;unsigned switches=0;void switchScreen(int screen){screenType=screen;++switches;}void refreshMainActionColor();void setPreviewFunction();void setPlayFunction();void showPreviewLabel();void showPlayLabel();void registerParamsPreview();bool convertedToMono=false,confirmedDataIsChanged=true;mtEffect*currentEffect=nullptr;int currentEffectIdx=0;bool isCurrentSampleStereo();};
static struct{unsigned flags=0;unsigned getFlags(){return flags;}}sampleEditorModular;
static struct{struct{unsigned lastUsedInstrument=0;}values;}mtProject;
static struct{bool stereo=true;bool isInstrumentStereo(unsigned){return stereo;}}newFileManager;
#include "stereo-ui.inc"
constexpr uint32_t EDIT_BUFF_SIZE=1048576;constexpr int interfaceButton6=6,buttonPress=1;
static unsigned nativePreviewCalls=0;static void functPreview();static void functPlay();static unsigned stops=0;static void functStop(){++stops;}
struct Input{void(*handler)()=nullptr;void clearAll(){handler=nullptr;}void clearButton(int){handler=nullptr;}void setButtonObj(int,int,void(*h)()){handler=h;}void press(){if(handler)handler();}};
static struct{struct{int activeLabelsColors=1,inactiveLabelsColors=2,activeButtonLabelsColors=3;}theme;int color=0;std::string text;auto&getTheme(){return theme;}void setControlText(int,const char*t){text=t;}void setControlColors(int,int c){color=c;}void refreshControl(int){}}display;
static cSampleEditor*SE;
static std::string lifetime;static unsigned begins=0,popups=0;constexpr int INTERFACE_BUTTON_PREVIEW=200;
namespace samplepreview{static bool enabled=true,live=false,prepared=false,fail=false;bool supported(unsigned){return enabled;}bool ready(){return prepared;}bool valid(){return true;}bool active(){return live;}void release(){lifetime+="release;";live=prepared=false;}const char*error(){return "Preview error";}bool begin(unsigned,unsigned,const mtEffect&,uint16_t,uint16_t){++begins;if(fail)return false;live=true;prepared=false;return true;}}
static struct{void stopInstrument(int){lifetime+="stop;";}}mtPadBoard;
static void monoMessage(const char*){++popups;}
static unsigned nativeApplyCalls=0,fileApplyCalls=0;static void functSave();static void monoBegin(){++fileApplyCalls;}
static bool isSdMonoEffect(unsigned id,uint32_t length){return id==17&&length>EDIT_BUFF_SIZE/2;}
static bool isSdRegionEffect(unsigned id,uint32_t length){return length>EDIT_BUFF_SIZE/2&&(id==0||id==3||id==4||id==6);}
#include "preview-ui.inc"
#include "settings-ui.inc"
static void settingsTest(){cSampleEditor e;e.confirmedDataIsChanged=false;SE=&e;Input input;e.ip=&input;mtEffect effect;e.currentEffect=&effect;e.instrument.sample.length=20000003;e.applyAllowed=false;samplepreview::enabled=true;
 for(int id:{9,10,11}){e.currentEffectIdx=id;e.screenType=cSampleEditor::mainScreen;CHECK(e.canUseMainAction()&&!e.canApplyCurrentEffect());e.refreshMainActionColor();CHECK(display.color==3);const auto count=e.switches;functApply();CHECK(e.screenType==cSampleEditor::effectScreen&&e.switches==count+1);CHECK(e.canPreviewCurrentEffect());functSave();CHECK(!nativeApplyCalls&&!fileApplyCalls);
  effect.workspaceConflict=true;e.selection.endPoint=e.selection.startPoint;CHECK(e.canUseMainAction()&&!e.canPreviewCurrentEffect());effect.workspaceConflict=false;e.selection.endPoint=65535;}
 e.currentEffectIdx=3;e.applyAllowed=true;functSave();CHECK(fileApplyCalls==1&&!nativeApplyCalls);e.directApply=true;e.applyAllowed=false;CHECK(!e.canUseMainAction());const auto count=e.switches;functApply();CHECK(e.switches==count);
 e.directApply=false;e.instrument.sample.length=1025;e.applyAllowed=false;CHECK(!e.canUseMainAction());e.refreshMainActionColor();CHECK(display.color==2);e.applyAllowed=true;CHECK(e.canUseMainAction());functApply();CHECK(e.switches==count+1);SE=nullptr;std::puts("PASS real Settings entry: EQ/Limiter/Compressor Select active, parameters accessible, Preview separate from Apply; RAM/direct Apply preserved");}
static void previewTest(){cSampleEditor e;e.confirmedDataIsChanged=false;SE=&e;Input input;e.ip=&input;mtEffect effect;e.currentEffect=&effect;decltype(e.instrument) instrument;e.editorInstrument=&instrument;
 for(unsigned length:{1u,524288u,1025u}){instrument.sample.length=length;CHECK(e.canPreviewCurrentEffect());e.setPreviewFunction();CHECK(display.text=="Preview"&&display.color==1&&input.handler);const auto before=nativePreviewCalls;input.press();CHECK(nativePreviewCalls==before+1);}
 instrument.sample.length=20000003;e.applyAllowed=false;CHECK(e.canPreviewCurrentEffect());e.setPreviewFunction();const auto old=begins;input.press();CHECK(begins==old+1&&samplepreview::live&&e.processingInProgress&&display.text=="Cancel");lifetime.clear();input.press();CHECK(!samplepreview::live&&!e.processingInProgress&&lifetime=="stop;release;");
 e.setPlayFunction();functPlay();CHECK(samplepreview::live);cancelLargePreview();
 effect.workspaceConflict=true;e.setPreviewFunction();CHECK(!input.handler&&display.color==2);effect.workspaceConflict=false;samplepreview::enabled=false;e.setPreviewFunction();CHECK(!input.handler&&display.color==2);const auto messages=popups;functPreview();functPlay();CHECK(popups==messages&&!samplepreview::live);
 e.previewState=cSampleEditor::previewStateStop;e.registerParamsPreview();CHECK(input.handler);input.press();CHECK(stops==1);
 samplepreview::enabled=true;samplepreview::fail=true;e.setPreviewFunction();input.press();CHECK(popups==messages+1&&!samplepreview::live);samplepreview::fail=false;
 instrument.sample.length=1;e.applyAllowed=true;e.setPreviewFunction();CHECK(input.handler&&display.color==1);SE=nullptr;std::puts("PASS actual Preview UI: RAM unchanged, large Preview starts/cancels, stop-before-release, unsupported disabled, failure and return to RAM");}
static void test(){int16_t left[]{100,200},right[]{100,200};mtEffect effect;effect.data[0]=left;effect.data[1]=right;effect.length=2;cSampleEditor e;e.currentEffect=&effect;
 CHECK(e.isCurrentSampleStereo());e.convertedToMono=true;CHECK(!e.isCurrentSampleStereo());right[1]=201;CHECK(e.isCurrentSampleStereo());right[1]=200;e.convertedToMono=false;CHECK(e.isCurrentSampleStereo());newFileManager.stereo=false;CHECK(!e.isCurrentSampleStereo());right[1]=201;CHECK(e.isCurrentSampleStereo());e.confirmedDataIsChanged=false;CHECK(!e.isCurrentSampleStereo());e.confirmedDataIsChanged=true;e.currentEffectIdx=editorEffectModular;sampleEditorModular.flags=PTFX_FLAG_CONVERT_TO_MONO;CHECK(!e.isCurrentSampleStereo());
 std::puts("PASS actual channel metadata UI body: native stereo/mono, converted mono, later stereo effect, restored original state and modular mono");}
}
int main(){ui::settingsTest();ui::previewTest();ui::test();driveCompare<baseline::mtEffectOverdrive,current::mtEffectOverdrive>(false);driveCompare<baseline::mtEffectBitcrusher,current::mtEffectBitcrusher>(true);everyHoldStep();std::puts("PASS original RAM/current RAM/SD PCM equivalence: 3 Overdrive types, min/max gain, Bitcrusher depth/rate extremes, 1024/8192 boundaries");plugins();gainCompare<baseline::mtEffectNormalizer,current::mtEffectNormalizer>(true);gainCompare<baseline::mtEffectAmplifier,current::mtEffectAmplifier>(false);ramMono();
 int16_t zeros[16]{},out[16]{};current::mtEffectNormalizer effect;for(int c=0;c<2;++c){effect.confirmed[c]={{zeros,16},{zeros,16}};effect.processed[c]={{out,16},{out,16}};}float level=100;effect.setParameter(level,0);CHECK(effect.startProcess());while(effect.getProcessState())effect.processBlock();for(auto value:out)CHECK(value==0);CHECK(ce_pcm_peak(-32768)==32768&&ce_pcm_normalize_gain(100,0)==0);
 std::printf("PASS CE PCM/RAM: %u checks, original non-silent gain effects bit-identical, normalization silence safe\n",checks);
}
