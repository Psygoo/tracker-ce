#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <vector>
#include <utility>
#include "../../../modules/wr2/UiModel.h"
#include "Platform.h"
#include "../../../core/source/mtPadsBacklight.h"
using std::uint8_t;using std::uint32_t;using namespace wr2;using namespace wr2::audio;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::abort();}}while(0)
static PublishedBank bank;
namespace wr2ui {static ui::Editor instance(bank);ui::Editor&editor(){return instance;}}
#include "buttons.inc"
constexpr int MAX_SLICE_NUMBER=48,kSampleInstrumentsCount=48,playModeSingleShot=0,playModeSlice=4,playModeBeatSlice=5,playModeWavetable=6,playModeGranular=7;
struct {bool invertValForList=false;}interfaceGlobals;
struct Instrument{unsigned isActive=1,playMode=5,sliceNumber=4,selectedSlice=0;uint16_t slices[48]{};};
struct {struct{unsigned lastUsedInstrument=0;}values;Instrument instrument[48];}mtProject;
struct {struct{uint8_t padsLightFront=31,padsLightBack=5;}values;}mtConfig;
struct {unsigned errors=0,hides=0;bool visible=false;void showInfoPopup(const char*,const char*){++errors;visible=true;}bool isNoteKeyboardPopupActive(){return false;}void toggleNoteKeyboardPopup(){}void hideInfoPopup(){++hides;visible=false;}}mtPopups;
struct Display {
 struct{uint32_t sliceColors[3]{0xabcdef,0x123456,0xfedcba},activeButtonLabelsColors[4]{0xffffff,0x123456,0x654321,0x111111};}theme;
 unsigned frameSelection=0;void setControlValue(int control,unsigned value){if(control==9)frameSelection=value;}void setControlShow(int){}
 std::string text[8];const uint32_t*palettes[8]{};
 auto&getTheme(){return theme;}
 void setControlText2(int i,const char*s){text[i]=s;}
 void setControlColors(int i,const uint32_t*c){palettes[i]=c;}
 void refreshControl(int){}
}display;
constexpr unsigned mtSampleTypeWaveFile=0,eventSwitchModule=1;
static struct Sequencer{enum{midiInVoiceMode_default=0};unsigned stops=0;void stopManualNotes(){++stops;}void setMidiInVoiceMode(unsigned){}}sequencer;
class cSamplePlayback{
public:
 bool moduleRefresh=true,wr2ThirdPage=true;uint8_t wr2TrigPlace=0;bool wr2Notice=false;uint8_t wr2NoticeSlice=255;
 int frameControl=9;struct{unsigned placesCount=8;}frameData;
 Instrument*editorInstrument=&mtProject.instrument[0];
 struct Points{const uint32_t*wr2Colors=nullptr;bool isSliceZerocrossed[48]{};uint8_t sliceCount=0,selected=0;int16_t slice[48]{};}slicePoints;
 uint32_t wr2MarkerColors[48]{},wr2TrigColors[4]{};char wr2WeightText[6]{};
 int label[8]{0,1,2,3,4,5,6,7},slicePointsControl=8;unsigned selectedPlace=0;
 bool wr2Page()const;void wr2Finish(bool resetPage=true);void wr2DismissNotice();void wr2Refresh();bool wr2Button(unsigned,uint8_t);
 bool sliceScreenMode2=false;unsigned refreshPoints=0,refreshSlicePoints=0;
 struct{unsigned selected=0;}points;
 unsigned loadedInstrumentType=0,lastDestination=255;void start(uint32_t);void stop();
 void eventFunct(unsigned,cSamplePlayback* page,const uint8_t* button,unsigned){lastDestination=*button;page->stop();}
 void showDefaultScreen(){}
 void lightUpPadBoard(){wr2Refresh();}
 void cancelMultiFrame(){}void clearAllNodes(){}
 void activateLabelsBorder();
};
static cSamplePlayback*SP=nullptr;
static unsigned nativeEncoderFallback=0;
static void modSliceSelect(int16_t value){SP->editorInstrument->selectedSlice=static_cast<unsigned>(std::clamp<int>(static_cast<int>(SP->editorInstrument->selectedSlice)+value,0,static_cast<int>(SP->editorInstrument->sliceNumber)-1));SP->wr2Refresh();}
static void changePlayModeSelection(int16_t){SP->wr2Finish();}
#include "ui.inc"
#include "encoder.inc"
#include "arrows.inc"
#include "lifecycle.inc"
static uint32_t drawColor=0;
static std::vector<uint32_t> drawColors;
static std::vector<std::pair<int,int>> drawVertices;
enum{LINES=1,LINE_STRIP=2};
static void API_COLOR(uint32_t c){drawColor=c;}
static void API_LINE_WIDTH(int){}
static void API_BEGIN(int primitive){if(primitive==LINES)drawColors.push_back(drawColor);}
static void API_VERTEX2F(int x,int y){drawVertices.emplace_back(x,y);}
static void API_END(){}
class cSlicePoints{public:cSamplePlayback::Points*points=nullptr;const uint32_t*colors=nullptr;int posX=0,posY=50,height=100;void update();};
#pragma warning(push)
#pragma warning(disable:4244)
#include "markers.inc"
#pragma warning(pop)
static void markerTests(){
 cSamplePlayback::Points p;p.sliceCount=3;p.selected=1;for(unsigned i=0;i<3;++i)p.slice[i]=static_cast<int16_t>(i*10);
 uint32_t stock[3]{0xabcdef,0x123456,0xfedcba},roles[3]{0xff0000,0xff8000,0x556677};cSlicePoints renderer;renderer.points=&p;renderer.colors=stock;
 drawVertices.clear();renderer.update();auto original=drawVertices;drawColors.clear();drawVertices.clear();p.wr2Colors=roles;renderer.update();
 CHECK(drawVertices==original&&drawVertices.size()==18);CHECK(drawColors.size()==3&&drawColors[0]==roles[0]&&drawColors[1]==roles[1]&&drawColors[2]==roles[2]);
}

static void uiTests(){
 Leds leds;padsBacklight.init(100,&leds);cSamplePlayback page;SP=&page;Snapshot s;
 for(unsigned i=0;i<48;++i)CHECK(!bank.instrument[i].read(s));
 page.wr2Refresh();CHECK(!bank.instrument[0].read(s));CHECK(leds.level[0]==5&&leds.level[4]==0);
 CHECK(page.wr2Button(1,buttonRelease));CHECK(!bank.instrument[0].read(s));
 CHECK(page.wr2Button(1,buttonPress));CHECK(bank.instrument[0].read(s)&&s.state.owner[0]==0&&s.state.weight[0]==100);
 mtConfig.values.padsLightFront=10;mtConfig.values.padsLightBack=31;page.wr2Refresh();CHECK(leds.level[0]==10);mtConfig.values.padsLightFront=31;mtConfig.values.padsLightBack=5;page.wr2Refresh();
 CHECK(wr2ui::editor().editing()&&display.palettes[1][0]==0xcd1818&&display.palettes[1][1]==0xcd1818);
 page.editorInstrument->selectedSlice=1;CHECK(page.wr2Button(2,buttonPress));
 CHECK(bank.instrument[0].read(s)&&s.state.weight[0]==90&&s.state.weight[1]==10);
 page.editorInstrument->selectedSlice=2;CHECK(page.wr2Button(2,buttonPress));
 CHECK(bank.instrument[0].read(s)&&s.state.weight[0]==81&&s.state.weight[1]==9&&s.state.weight[2]==10);
 CHECK(page.wr2Button(4,buttonPress)&&page.selectedPlace==4&&display.text[4]=="10%");
 encoderUnderTest(3);CHECK(display.text[4]=="13%");encoderUnderTest(-3);CHECK(display.text[4]=="10%");
 CHECK(wr2ui::editor().weight(2,-100)==Result::Applied);page.wr2Refresh();CHECK(display.text[4]=="0%");
 CHECK(wr2ui::editor().weight(2,1000)==Result::Applied);page.wr2Refresh();CHECK(display.text[4]=="100%");
 CHECK(bank.instrument[0].read(s)&&valid(s.state)&&s.state.weight[0]==0&&s.state.weight[1]==0);
 auto epoch=s.topology;auto selected=page.editorInstrument->selectedSlice;
 // Move only cuts: no invalidator; selection and authority remain unchanged.
 page.wr2Refresh();CHECK(bank.instrument[0].read(s)&&s.topology==epoch&&page.editorInstrument->selectedSlice==selected);
 CHECK(page.wr2Button(3,buttonPress));CHECK(bank.instrument[0].read(s)&&s.state.owner[2]==kFree&&valid(s.state));
 auto remaining=s.state;CHECK(page.wr2Button(3,buttonRelease));CHECK(bank.instrument[0].read(s)&&std::memcmp(&remaining,&s.state,sizeof(remaining))==0);
 // Group session remains attached to original master on free-slice navigation.
 page.editorInstrument->selectedSlice=3;page.wr2Refresh();CHECK(wr2ui::editor().master()==0&&display.text[4].empty());
 CHECK(page.wr2Button(1,buttonPress)&&!wr2ui::editor().editing());
 CHECK(page.wr2Button(1,buttonPress));CHECK(wr2ui::editor().master()==3);
 CHECK(page.wr2MarkerColors[0]==0x7f0000&&page.wr2MarkerColors[1]==0x7f4000&&page.wr2MarkerColors[3]==0xff0000);
 CHECK(leds.level[0]==10&&leds.level[1]==10&&leds.level[3]==31);
 // Foreign group edit rejected without altering either group.
 page.editorInstrument->selectedSlice=1;CHECK(page.wr2Button(2,buttonPress));CHECK(mtPopups.errors==1);
 page.editorInstrument->selectedSlice=3;CHECK(page.wr2Button(3,buttonPress));CHECK(!wr2ui::editor().editing());
 CHECK(bank.instrument[0].read(s)&&s.state.owner[3]==kFree&&s.state.owner[0]==0);
 // Enter group through Member, remove Master deletes all members.
 page.editorInstrument->selectedSlice=1;CHECK(page.wr2Button(1,buttonPress));CHECK(wr2ui::editor().master()==0);
 page.editorInstrument->selectedSlice=0;CHECK(page.wr2Button(3,buttonPress));
 CHECK(bank.instrument[0].read(s)&&s.state.owner[0]==kFree&&s.state.owner[1]==kFree);
 CHECK(page.wr2Button(1,buttonPress));CHECK(wr2ui::editor().editing());
 mtProject.values.lastUsedInstrument=1;CHECK(!page.wr2Button(1,buttonPress));CHECK(!bank.instrument[1].read(s));page.editorInstrument=&mtProject.instrument[1];page.wr2Refresh();CHECK(!wr2ui::editor().editing());CHECK(!bank.instrument[1].read(s));
 CHECK(page.wr2Button(1,buttonPress));CHECK(bank.instrument[1].read(s));
 CHECK(wr2ui::editor().invalidateAll());CHECK(bank.instrument[0].read(s)&&s.state.sliceCount==0);CHECK(bank.instrument[1].read(s)&&s.state.sliceCount==0);
 page.wr2Finish();CHECK(!page.wr2Page()&&!page.slicePoints.wr2Colors);CHECK(!page.wr2Button(1,buttonPress));
 // Real MORE callback cycles two native pages and page 3 without changing slice.
 page.editorInstrument->selectedSlice=2;functButton5();CHECK(page.sliceScreenMode2&&!page.wr2ThirdPage);
 functButton5();CHECK(!page.sliceScreenMode2&&page.wr2Page());CHECK(page.slicePoints.wr2Colors!=nullptr);CHECK(page.editorInstrument->selectedSlice==2);
 functButton5();CHECK(!page.wr2ThirdPage&&!page.sliceScreenMode2&&page.editorInstrument->selectedSlice==2);
 encoderUnderTest(1);CHECK(nativeEncoderFallback==1);
 std::printf("PASS production UI callbacks/refresh: %u checks; RAM only, display/inputs mocked\n",checks);
}

static void interactionRegressions(){
 wr2ui::editor().finish();bank.~PublishedBank();new(&bank)PublishedBank;mtProject.values.lastUsedInstrument=0;mtProject.instrument[0]=Instrument{};mtProject.instrument[1]=Instrument{};mtPopups={};
 Leds leds;padsBacklight.init(100,&leds);cSamplePlayback page;SP=&page;Snapshot before,after;
 CHECK(page.wr2TrigPlace==0);CHECK(page.wr2Button(4,buttonPress));CHECK(page.selectedPlace==0&&display.frameSelection==0&&!wr2ui::editor().editing());functRight();CHECK(page.selectedPlace==0&&display.frameSelection==0);
 CHECK(page.wr2Button(2,buttonPress)&&mtPopups.visible);page.wr2Refresh();CHECK(mtPopups.visible);CHECK(page.wr2Button(0,buttonPress)&&!mtPopups.visible);
 CHECK(page.wr2Button(1,buttonPress));CHECK(page.selectedPlace==0);page.editorInstrument->selectedSlice=1;CHECK(page.wr2Button(2,buttonPress));
 CHECK(page.wr2Button(4,buttonPress));CHECK(page.selectedPlace==4&&page.wr2TrigPlace==4&&display.frameSelection==4);CHECK(bank.instrument[0].read(before));
 CHECK(page.wr2Button(1,buttonPress));CHECK(!wr2ui::editor().editing()&&page.selectedPlace==0&&page.wr2TrigPlace==4);CHECK(page.wr2Button(0,buttonPress));CHECK(page.wr2TrigPlace==4);CHECK(page.wr2Button(4,buttonPress)&&page.selectedPlace==0&&display.frameSelection==0);
 CHECK(page.wr2Button(1,buttonPress));CHECK(wr2ui::editor().editing()&&page.selectedPlace==4&&display.frameSelection==4);CHECK(bank.instrument[0].read(after)&&!std::memcmp(&before.state,&after.state,sizeof(before.state))&&before.topology==after.topology);
 functLeft();CHECK(page.selectedPlace==0&&page.wr2TrigPlace==0);CHECK(page.wr2Button(1,buttonPress));CHECK(page.wr2Button(1,buttonPress)&&page.selectedPlace==0);functRight();CHECK(page.selectedPlace==4&&page.wr2TrigPlace==4);
 // Remove a Master ends Trig and immediately clears a stale Weight frame.
 page.editorInstrument->selectedSlice=0;CHECK(page.wr2Button(3,buttonPress));CHECK(!wr2ui::editor().editing()&&page.selectedPlace==0&&display.frameSelection==0&&page.wr2TrigPlace==4);
 CHECK(page.wr2Button(1,buttonPress)&&page.selectedPlace==4);page.editorInstrument->selectedSlice=1;CHECK(page.wr2Button(2,buttonPress));CHECK(page.wr2Button(1,buttonPress));
 page.editorInstrument->selectedSlice=2;CHECK(page.wr2Button(1,buttonPress));CHECK(page.wr2Button(1,buttonPress));page.editorInstrument->selectedSlice=0;CHECK(page.wr2Button(1,buttonPress));CHECK(wr2ui::editor().master()==0);
 CHECK(bank.instrument[0].read(before));page.editorInstrument->selectedSlice=2;CHECK(page.wr2Button(2,buttonPress));CHECK(mtPopups.visible&&page.wr2Notice);page.wr2Refresh();CHECK(mtPopups.visible&&page.wr2Notice);
 CHECK(bank.instrument[0].read(after)&&!std::memcmp(&before.state,&after.state,sizeof(before.state)));CHECK(page.wr2Button(4,buttonPress));CHECK(!mtPopups.visible&&!page.wr2Notice);
 CHECK(page.wr2Button(2,buttonPress)&&mtPopups.visible);page.editorInstrument->selectedSlice=1;page.wr2Refresh();CHECK(!mtPopups.visible&&!page.wr2Notice&&!display.text[4].empty());
 page.editorInstrument->selectedSlice=2;CHECK(page.wr2Button(2,buttonPress)&&mtPopups.visible);encoderUnderTest(1);CHECK(!mtPopups.visible&&!page.wr2Notice);CHECK(bank.instrument[0].read(after)&&!std::memcmp(&before.state,&after.state,sizeof(before.state)));
 CHECK(page.wr2Button(2,buttonPress)&&mtPopups.visible);functLeft();CHECK(!mtPopups.visible&&page.selectedPlace==0);encoderUnderTest(-1);CHECK(page.editorInstrument->selectedSlice==1);
 mtPopups.showInfoPopup("Other", "Native popup");const auto hides=mtPopups.hides;page.wr2Refresh();CHECK(mtPopups.visible&&mtPopups.hides==hides);page.wr2Button(0,buttonPress);CHECK(mtPopups.visible&&mtPopups.hides==hides);mtPopups.hideInfoPopup();
 page.wr2Button(4,buttonPress);page.editorInstrument->selectedSlice=2;page.wr2Button(2,buttonPress);CHECK(mtPopups.visible);page.wr2Finish();CHECK(!mtPopups.visible&&!page.wr2Notice&&page.wr2TrigPlace==4);
 page.wr2ThirdPage=true;page.editorInstrument->selectedSlice=1;page.wr2Button(1,buttonPress);CHECK(page.selectedPlace==4);page.wr2Finish();
 // The preference is UI-wide, not an instrument parameter; native pages keep their focus.
 mtProject.values.lastUsedInstrument=1;page.editorInstrument=&mtProject.instrument[1];page.wr2ThirdPage=true;page.wr2Button(1,buttonPress);CHECK(page.selectedPlace==4);page.wr2Finish();page.selectedPlace=4;page.wr2Refresh();CHECK(page.selectedPlace==4);
 wr2ui::editor().invalidateAll();std::puts("PASS three UI regressions: Trig Slice/Weight memory, unavailable Weight frame, owned notice dismissal/recovery; group data unchanged by focus/navigation");
}
static void ledsTests(){
 Leds leds;cMtPadsBacklight manager{};manager.init(100,&leds);
 manager.setBackLayer(1,4,0);manager.setFrontLayer(1,7,0);manager.setBackLayer(1,8,1);
 uint8_t levels[48]{};levels[0]=31;levels[1]=18;manager.setWr2Overlay(levels);auto calls=leds.calls;
 manager.setWr2Overlay(levels);CHECK(leds.calls==calls);
 manager.setFrontLayer(1,12,0);manager.setBackLayer(1,6,1);CHECK(leds.calls==calls&&leds.level[0]==31);
 manager.clearWr2Overlay();CHECK(leds.level[0]==12&&leds.level[1]==6);
 calls=leds.calls;manager.clearWr2Overlay();CHECK(leds.calls==calls);
 InstrumentState s;s.reset(5);create(s,0);add(s,0,1);create(s,2);add(s,2,3);
 CHECK(ui::Editor::led(s,0,kFree,31)==31&&ui::Editor::led(s,1,kFree,31)==18&&ui::Editor::led(s,4,kFree,31)==5&&ui::Editor::led(s,5,kFree,31)==0);
 CHECK(ui::Editor::led(s,2,0,31)==10);
 for(unsigned ceil=0;ceil<32;++ceil)for(unsigned slice=0;slice<48;++slice)CHECK(ui::Editor::led(s,slice,0,ceil)<=ceil);
 CHECK(ui::Editor::marker(s,0,kFree,0)==0xff0000&&ui::Editor::marker(s,1,kFree,0)==0xff8000);
 CHECK(ui::Editor::marker(s,4,0,0xabcdef)==0x556677);
 std::printf("PASS native LED overlay/gamma/marker regression: %u total checks\n",checks);
}
using strInstrument=Instrument;
namespace wr2ui {
#include "native-invalidate.inc"
}
constexpr int fmReloadSamples=1;
struct HeaderFixture{
 int operation=fmReloadSamples;
 int getCurrentOperation()const{return operation;}
 void loadValidatedHeader(strInstrument*instrument){
#include "header-invalidation.inc"
 }
};
static void reloadTests(){
 wr2ui::editor().bind(42,3,true);CHECK(wr2ui::editor().trig(0)==Result::Applied);
 Snapshot before,after;CHECK(bank.instrument[42].read(before));HeaderFixture loader;
 loader.loadValidatedHeader(&mtProject.instrument[42]);CHECK(bank.instrument[42].read(after));
 CHECK(after.topology==before.topology&&after.state.owner[0]==0);
 loader.operation=0;loader.loadValidatedHeader(&mtProject.instrument[42]);
 CHECK(bank.instrument[42].read(after)&&after.state.sliceCount==0&&after.topology!=before.topology);
}

struct ValidatedPti {struct {strInstrument instrument;}instrumentDataAndHeader;};
struct ImportPolicy{
 int getCurrentOperation()const{return 0;}
 bool loadPti(strInstrument*instrument,ValidatedPti*instrumentFile){
#include "pti-tail.inc"
 }
 void setDefaultActiveInstrument(strInstrument*){CHECK(false);} // must not run for active WAV slot
 void loadWavActiveGuard(int instrIdx){
    auto&newFileManager=*this;
#include "wav-active-guard.inc"
 }
};
static void importPolicyTests(){
 constexpr unsigned slot=41;auto&instrument=mtProject.instrument[slot];instrument.isActive=1;instrument.sliceNumber=4;instrument.selectedSlice=2;instrument.slices[0]=0;instrument.slices[1]=9000;instrument.slices[2]=22000;instrument.slices[3]=40000;
 wr2ui::editor().bind(slot,4,true);CHECK(wr2ui::editor().trig(0)==Result::Applied&&wr2ui::editor().addMember(1)==Result::Applied);
 Snapshot before,after;CHECK(bank.instrument[slot].read(before));const auto nativeBefore=instrument;
 ImportPolicy importer;importer.loadWavActiveGuard(slot);CHECK(bank.instrument[slot].read(after));
 CHECK(after.topology==before.topology&&std::memcmp(&after.state,&before.state,sizeof(before.state))==0);
 CHECK(std::memcmp(&nativeBefore,&instrument,sizeof(instrument))==0);
 std::puts("PASS LOAD WAV metadata: native active-slot guard preserves slices, Master, Members, weights and context");
 ValidatedPti pti;pti.instrumentDataAndHeader.instrument.sliceNumber=7;pti.instrumentDataAndHeader.instrument.selectedSlice=6;pti.instrumentDataAndHeader.instrument.slices[1]=7000;
 CHECK(importer.loadPti(&instrument,&pti));CHECK(instrument.sliceNumber==7&&instrument.selectedSlice==6&&instrument.slices[1]==7000);
 CHECK(bank.instrument[slot].read(after)&&after.state.sliceCount==0&&after.topology!=before.topology);
 pti.instrumentDataAndHeader.instrument.sliceNumber=255;pti.instrumentDataAndHeader.instrument.selectedSlice=255;
 CHECK(importer.loadPti(&instrument,&pti));CHECK(instrument.sliceNumber==MAX_SLICE_NUMBER&&instrument.selectedSlice==MAX_SLICE_NUMBER-1);
 pti.instrumentDataAndHeader.instrument.sliceNumber=0;pti.instrumentDataAndHeader.instrument.selectedSlice=255;
 CHECK(importer.loadPti(&instrument,&pti));CHECK(instrument.sliceNumber==0&&instrument.selectedSlice==0);
 std::puts("PASS LOAD PTI metadata: native validated tail clears previous groups then copies replacement slices; file validation/IO not emulated");
}
static void boundsTests(){
 for(unsigned i=0;i<48;++i){
  wr2ui::editor().bind(i,48,true);CHECK(wr2ui::editor().trig(i)==Result::Applied);
  auto member=(i+1)%48;CHECK(wr2ui::editor().addMember(member)==Result::Applied);
  CHECK(wr2ui::editor().weight(i,-10000)==Result::Applied&&wr2ui::editor().weight(i)==0);
  CHECK(wr2ui::editor().weight(i,10000)==Result::Applied&&wr2ui::editor().weight(i)==100);
  CHECK(wr2ui::editor().removeSlice(i)==Result::Applied);
  CHECK(wr2ui::editor().invalidate(i));
 }
 Snapshot snapshot;for(unsigned i=0;i<48;++i)CHECK(bank.instrument[i].read(snapshot)&&snapshot.state.sliceCount==0);
 InstrumentState state;state.reset(3);create(state,0);
 CHECK(bank.instrument[47].publish(state,0xffffffffu)==PublishResult::Published);
 CHECK(!wr2ui::editor().invalidate(47));CHECK(!bank.instrument[47].read(snapshot)&&bank.instrument[47].context()==0);
 CHECK(bank.instrument[47].publish(state,1)==PublishResult::Exhausted);
 wr2ui::editor().bind(47,3,true);CHECK(wr2ui::editor().trig(0)==Result::Invalid);
}
static void pageMemoryTests(){
 Leds leds;padsBacklight.init(100,&leds);
 wr2ui::editor().invalidateAll();mtProject.values.lastUsedInstrument=0;mtProject.instrument[0]={};mtProject.instrument[1]={};mtProject.instrument[1].playMode=playModeSingleShot;
 cSamplePlayback page;SP=&page;page.wr2ThirdPage=false;page.sliceScreenMode2=false;
 // All menu departures share the actual module-switch event and stop lifecycle.
 const char* destinations[]={"Pattern","Sample Loader","Sample Editor","File","Config","Song"};
 for(unsigned cycle=0;cycle<20;++cycle)for(unsigned expected=0;expected<3;++expected){
  page.start(0);while((page.wr2ThirdPage?2u:page.sliceScreenMode2?1u:0u)!=expected)functButton5();
  for(unsigned menu=0;menu<6;++menu){
   const auto beforeStops=sequencer.stops;functSwitchModule(static_cast<uint8_t>(menu));
   CHECK(!page.moduleRefresh&&sequencer.stops==beforeStops+1&&page.lastDestination==menu);
   CHECK(!wr2ui::editor().editing()&&page.slicePoints.wr2Colors==nullptr);
   page.start(menu+1);page.wr2Refresh();
   CHECK(page.moduleRefresh&&(page.wr2ThirdPage?2u:page.sliceScreenMode2?1u:0u)==expected);
   CHECK(page.wr2Page()==(expected==2));
  }
 }
 for(const char* name:destinations)std::printf("PASS page memory: pages 1/2/3 via %s, repeated transitions\n",name);
 // Navigation keeps page preference, while explicit MORE still cycles 3 -> 1.
 page.wr2ThirdPage=true;page.sliceScreenMode2=false;functButton5();CHECK(!page.wr2ThirdPage&&!page.sliceScreenMode2);functButton5();CHECK(page.sliceScreenMode2&&!page.wr2ThirdPage);functButton5();CHECK(page.wr2ThirdPage&&!page.sliceScreenMode2);
 // Active Trig/error/overlay are cleaned exactly as before; groups remain intact.
 page.editorInstrument->selectedSlice=0;page.wr2Button(1,buttonPress);Snapshot before,after;CHECK(bank.instrument[0].read(before));page.wr2Notice=true;mtPopups.visible=true;page.stop();CHECK(!wr2ui::editor().editing()&&!mtPopups.visible&&!page.wr2Notice);page.start(0);page.wr2Refresh();CHECK(page.wr2Page()&&bank.instrument[0].read(after)&&!std::memcmp(&before.state,&after.state,sizeof(before.state)));
 // Existing native reset when entering a different playback mode remains shared.
 mtProject.values.lastUsedInstrument=1;page.stop();page.start(0);CHECK(!page.wr2ThirdPage&&!page.sliceScreenMode2&&!page.wr2Page());mtProject.values.lastUsedInstrument=0;page.stop();page.start(0);CHECK(!page.wr2ThirdPage&&!page.sliceScreenMode2);wr2ui::editor().invalidateAll();
}
int main(){uiTests();pageMemoryTests();interactionRegressions();ledsTests();markerTests();reloadTests();importPolicyTests();boundsTests();std::printf("PASS UI final: %u checks, native MORE/jog branch/marker geometry\n",checks);}
