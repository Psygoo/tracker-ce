#include <limits>
#include <type_traits>
#define AUDIO_SAMPLE_RATE_EXACT 44117.64706f
using std::clamp;
constexpr int MAX_SIGNED_16BIT=32767,MIN_SIGNED_16BIT=-32768;
template<class T>T map(T x,T a,T b,T c,T d){return (x-a)*(d-c)/(b-a)+c;}
class mtEffect {public:
 static constexpr int kChannels=2;struct Part{int16_t*addr;uint32_t length;};struct Buffer{Part area,selection;}confirmed[2]{},processed[2]{};
 virtual void discardForFileOperation(){}
 ~mtEffect()=default;struct{bool isEnable=false;}undoParams;struct{bool isLoadedData=false;struct{bool state=false;}processParams;}processing;struct{struct{bool state=false;}processingParams;}loadingParams;bool hasPreviewWorkspaceConflict()const;virtual void setParameter(float&,int)=0;virtual bool startProcess()=0;virtual int32_t processBlock()=0;virtual bool getProcessState()=0;virtual uint32_t getExpectedProcessLength()=0;
 void bindPreviewBuffers(int16_t*const[2],int16_t*const[2],uint32_t);
};
#include "preview-bind.inc"
#include "preview-conflict.inc"
