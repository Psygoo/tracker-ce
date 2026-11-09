#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#define TRACKER_CE_ENABLE_SD_MONO 1
#define TRACKER_CE_ENABLE_SD_EDITS 1
#define GUI_HANDLE_SYNTH 0
#include "styles.inc"
#define FONT_INDEX_FROM_STYLE (((style>>8)&15)-1)
constexpr unsigned displayFontCount=5,controlsCount=65,labelBitmapsCount=2,displayBitmapsCount=1,SAMPLE_NAME_SIZE=31,SYNTH_INSTRUMENTS_START_IDX=48,MIDI_INSTRUMENTS_START_IDX=96;
constexpr int OPT_CENTERX=1,OPT_CENTERY=2,OPT_RIGHTX=4,RECTS=1,LINES=2,LINE_STRIP=3,BITMAPS=4,FTPOINTS=5,SRC_ALPHA=1,ZERO=0,ONE_MINUS_SRC_ALPHA=2,NEAREST=0,BORDER=0;
struct strFont{uint8_t data[148]{};int16_t height=0,handle=0;}fonts[5];
#include "fonts.inc"
#include "width.inc"
struct strLabelData{struct{unsigned bitmapIndex=0,xValue=0,yValue=0;}bitmaps[2];struct{int x1=0,x2=0,y1=0,y2=0;uint32_t color=0;}line;struct{int count=0,current=0;}dotIndicator;};
struct{unsigned address=0,format=0,width=0,height=0;}bitmaps[1];
struct Command{enum Kind{Text,Rect,Dot}kind;std::string text;int x=0,y=0,w=0,h=0;uint32_t color=0;int font=0;};
static std::vector<Command>commands,frame;static uint32_t color;static int primitive,vx,vy,vertices;
static void API_COLOR(uint32_t c){color=c;}
static void API_BEGIN(int mode){primitive=mode;vertices=0;}
static void API_VERTEX2F(int x,int y){if(vertices++==0){vx=x;vy=y;}else if(primitive==RECTS)commands.push_back({Command::Rect,"",vx,vy,x-vx,y-vy,color,0});else if(primitive==LINES)commands.push_back({Command::Dot,"",vx,vy,x-vx,y-vy,color,0});}
static void API_CMD_TEXT(int x,int y,int font,unsigned options,const char*text,int length=0){std::string s=text?text:"";if(length>0)s.resize(std::min<size_t>(s.size(),length));int width=0;for(unsigned char c:s)width+=c<128?fonts[font].data[c]:0;if(options&OPT_RIGHTX)x-=width;if(options&OPT_CENTERX)x-=width/2;int height=fonts[font].height;if(options&OPT_CENTERY)y-=height/2;commands.push_back({Command::Text,s,x,y,width,height,color,font});}
#define API_BLEND_FUNC(...) ((void)0)
#define API_LINE_WIDTH(...) ((void)0)
#define API_SAVE_CONTEXT(...) ((void)0)
#define API_RESTORE_CONTEXT(...) ((void)0)
#define API_SCISSOR_XY(...) ((void)0)
#define API_SCISSOR_SIZE(...) ((void)0)
#define API_CMD_GRADIENT(...) ((void)0)
#define API_END(...) ((void)0)
#define API_BITMAP_HANDLE(...) ((void)0)
#define API_BITMAP_SIZE(...) ((void)0)
#define API_CMD_LOADIDENTITY(...) ((void)0)
#define API_CMD_TRANSLATE(...) ((void)0)
#define API_CMD_ROTATE(...) ((void)0)
#define API_CMD_SETMATRIX(...) ((void)0)
#define API_CELL(...) ((void)0)
#define API_VERTEX2II(...) ((void)0)
#define API_CMD_SETBITMAP(...) ((void)0)
#define API_POINT_SIZE(...) ((void)0)
#define API_CMD_DLSTART(...) frame.clear()
#define API_CLEAR_COLOR(...) ((void)0)
#define API_CLEAR(...) ((void)0)
#define API_VERTEX_FORMAT(...) ((void)0)
#define API_DISPLAY(...) ((void)0)
#define API_CMD_SWAP(...) ((void)0)
static void API_CMD_APPEND(unsigned,unsigned);
struct strControlProperties{const char*text=nullptr;int value=0;void*data=nullptr;uint32_t style=0;const uint32_t*colors=nullptr;uint16_t x=0,y=0,w=0,h=0;};
struct cLabel{uint32_t style=0,style2=0;int value=0,posX=0,posY=0,width=0,height=0,textStyle1=0,textStyle2=0;const char*text=nullptr,*text2=nullptr;const uint32_t*colors;strLabelData*data=nullptr;strFont*font=&fonts[3],*font2=&fonts[3];bool refresh=true;unsigned ramSize=0;std::vector<Command>cache;
 cLabel(strControlProperties*p):value(p->value),posX(p->x),posY(p->y),width(p->w),height(p->h),text(p->text),colors(p->colors),data(static_cast<strLabelData*>(p->data)){setStyle(p->style);}
 void setStyle(uint32_t);void update();void drawTextUnderline(const char*,const strFont*,int,int,int){}
};using SimpleBox=cLabel;using cDisplayControl=cLabel;using hControl=int;
struct Driver{void beginCoProListNoCheck(){}void endCoProList(){}}driver;
struct cDisplay{struct Theme{uint32_t topBarLabelColors[4]{0xffffff,0,0xffffff,0xffffff};uint32_t batteryIconColors[4]{};uint32_t red=0xff0000;uint32_t backgroundColor=0;}theme;
 cLabel*controlsTable[controlsCount]{};unsigned count=0,memoryUsed=0;int updateStep=0;bool stopAppend=false;unsigned controlsRamStartAddress=0,controlsRamAddressStep=1024;Driver*displayDriver=&driver;Theme*getThemePtr(){return &theme;}Theme&getTheme(){return theme;}
 template<class T>int createControl(strControlProperties*p){assert(count<controlsCount);controlsTable[count]=new T(p);return count++;}
 ~cDisplay(){for(auto*p:controlsTable)delete p;}
 void setControlStyle(int handle,uint32_t style);void setControlShow(int handle);void setControlHide(int handle);void step3();void step1(){}
 void setControlText(int id,const char*s){controlsTable[id]->text=s;}void setControlPosition(int id,int x,int y){controlsTable[id]->posX=x;controlsTable[id]->posY=y;}
 void refreshControl(int id){if(id<0)return;commands.clear();controlsTable[id]->update();controlsTable[id]->cache=commands;controlsTable[id]->ramSize=unsigned(commands.size()*32+8);}
 void render(){step3();}
}display;
static void API_CMD_APPEND(unsigned address,unsigned){auto*p=display.controlsTable[address/1024];frame.insert(frame.end(),p->cache.begin(),p->cache.end());}
struct Instrument{struct{char file_name[32]{"sample"};}sample;};static struct{struct{unsigned lastUsedInstrument=0;}values;Instrument instrument[48];}mtProject;
class TitleBar{public:int titleLabel=-1,rightLabel=-1,battery=-1,performBox=-1,bar=-1;strLabelData barLabelData;bool visible=false;void initControls();void show(bool);void setTitle(const char*);void showCurrentInstrumentOnRightLabel();}nativeTitle;
class cSampleEditor{public:TitleBar*titleBar=&nativeTitle;int sdModeLabel=-1;uint32_t sdModeColors[4]{};strLabelData sdModeData;enum{mainScreen,effectScreen};int screenType=mainScreen;bool sd=false;bool usesSdEditing()const{return sd;}void initSdIndicatorControl();void refreshSdIndicator(const char*);void showTitleBar(const char*);void stop(){display.setControlHide(sdModeLabel);display.refreshControl(sdModeLabel);}};
#include "graphics-native.inc"
static bool overlap(const Command&a,const Command&b){return a.x<b.x+b.w&&b.x<a.x+a.w&&a.y<b.y+b.h&&b.y<a.y+a.h;}
static const Command&textCommand(const std::string&s){auto it=std::find_if(frame.begin(),frame.end(),[&](auto&c){return c.kind==Command::Text&&c.text==s;});assert(it!=frame.end());return *it;}
static void visible(const std::string&s){const auto&c=textCommand(s);assert(c.y>=0&&c.y+c.h<480&&c.x>=0&&c.x+c.w<=800);auto pos=&c-&frame[0];for(size_t i=size_t(pos)+1;i<frame.size();++i)if(frame[i].kind==Command::Rect)assert(!overlap(c,frame[i]));}
static bool has(const std::string&s){return std::any_of(frame.begin(),frame.end(),[&](auto&c){return c.kind==Command::Text&&c.text==s;});}
int main(){for(int i:{3,4}){auto*p=i==3?font3:font4;std::memcpy(fonts[i].data,p,148);fonts[i].height=p[140];fonts[i].handle=int16_t(i);}cSampleEditor e;e.initSdIndicatorControl();nativeTitle.initControls();
 // Emulate opaque editor panels, allocated after the independent indicator just like production.
 strControlProperties p;p.x=400;p.y=29;p.w=400;p.h=394;p.colors=display.theme.topBarLabelColors;p.style=controlStyleBackground;int cover=display.createControl<cLabel>(&p);display.refreshControl(cover);
 const std::string banner="LONG SAMPLE   SD STREAMING MODE";
 for(unsigned slot:{0u,47u})for(bool sd:{false,true})for(int screen:{0,1})for(const char*t:{"Sample Editor 1/2","Sample Editor 2/2","Sample Editor Stereo to Mono","Sample Editor Compressor"})for(bool longName:{false,true}){
  std::string name=longName?std::string(26,'&'):"sample";mtProject.values.lastUsedInstrument=slot;std::snprintf(mtProject.instrument[slot].sample.file_name,32,"%s",name.c_str());std::string shown=std::to_string(slot+1)+". "+name;
  e.sd=sd;e.screenType=screen;e.showTitleBar(t);if(screen)display.setControlShow(cover);else display.setControlHide(cover);e.refreshSdIndicator(t);display.render();
  visible(shown);assert(display.controlsTable[nativeTitle.titleLabel]->style&controlStyleShow);
  // CE clips long titles natively to 270 px. Do not change its text or properties.
  assert(std::string(display.controlsTable[nativeTitle.titleLabel]->text)==t);assert(display.controlsTable[nativeTitle.titleLabel]->width==270);
  std::string drawn=t;int n=getMaxTextLengthThatFitSpecificWidth(&fonts[3],t,270);if(n>0)drawn.resize(n);visible(drawn);
  if(sd){visible(banner);const auto&b=textCommand(banner);assert(std::any_of(frame.begin(),frame.end(),[&](auto&c){return c.kind==Command::Dot&&c.color==0xff0000&&c.y==b.y+b.h/2&&c.x>b.x&&c.x<b.x+b.w;}));assert(b.color==0xff0000&&b.font==4);const int tw=std::min<int>(270,getTextWidth(&fonts[3],t,uint16_t(std::strlen(t))));const bool fits=9+tw+8+b.w+8<=790-textCommand(shown).w;assert(b.y==(fits?5:40));assert(b.x==(fits?9+tw+8:(screen?409:9)));assert(!overlap(b,textCommand(shown)));assert(!overlap(b,textCommand(drawn)));if(screen==0&&b.y>28)assert(b.x==9&&b.y+b.h<75);if(screen==1&&b.y>28)assert(b.x==409&&b.x+b.w<=800);}
  else{assert(!has(banner));auto ramFrame=frame;e.showTitleBar(t);display.render();assert(frame.size()==ramFrame.size());for(size_t i=0;i<frame.size();++i){assert(frame[i].kind==ramFrame[i].kind&&frame[i].text==ramFrame[i].text&&frame[i].x==ramFrame[i].x&&frame[i].y==ramFrame[i].y&&frame[i].font==ramFrame[i].font);}}
  if(slot==47&&std::string(t)==(longName?"Sample Editor Stereo to Mono":"Sample Editor 1/2")&&screen==0){std::printf("TRACE %s %s (native compositor order):\n",sd?"SD":"RAM",longName?"long name":"short name");for(const auto&c:frame)if(c.kind==Command::Text)std::printf(" TEXT [%s] x=%d y=%d w=%d h=%d font=%d color=%06x\n",c.text.c_str(),c.x,c.y,c.w,c.h,c.font,c.color);}
 }
 // Reproduce SDTITLE's exact visibility failure despite valid coordinates.
 e.showTitleBar("Sample Editor 1/2");display.setControlStyle(nativeTitle.titleLabel,controlStyleCenterY|controlStyleLimitTextWidth|controlStyleFont4);display.refreshControl(nativeTitle.titleLabel);display.render();assert(!has("Sample Editor 1/2"));e.showTitleBar("Sample Editor 1/2");display.render();visible("Sample Editor 1/2");
 e.sd=true;e.refreshSdIndicator("Sample Editor 1/2");e.stop();display.render();assert(!has(banner));
 std::puts("PASS actual graphics: native cLabel commands + compositor append order/Show bit; title/name/pages visible, title/fallback indicator not overpainted, RAM command parity, entry/exit; SDTITLE counterexample reproduced");
}
