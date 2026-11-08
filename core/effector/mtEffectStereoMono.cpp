#if TRACKER_CE_ENABLE_SD_MONO
#include "mtEffectStereoMono.h"
#include "sampleProcessing/PcmEffects.h"
#include <cmath>
#include <algorithm>
#include <cstring>
mtEffectStereoMono sampleEditorStereoMono;
void mtEffectStereoMono::setParameter(float&value,int index){if(index==0)source_=static_cast<unsigned>(value);else if(index==1)gain_=value==0.0f?1.0f:std::pow(10.0f,value/20.0f);}
bool mtEffectStereoMono::startProcess(){position_=0;length_=confirmed[0].selection.length;state_=length_!=0;return state_;}
int32_t mtEffectStereoMono::processBlock(){
 if(!state_)return -1;
 const auto count=std::min<uint32_t>(8192,length_-position_);
 for(uint32_t i=0;i<count;++i){const auto at=position_+i;const auto value=ce_pcm_mono(confirmed[0].selection.addr[at],kChannels>1?confirmed[kChannels-1].selection.addr[at]:confirmed[0].selection.addr[at],source_,gain_,stereo_&&kChannels>1);for(int c=0;c<kChannels;++c)processed[c].selection.addr[at]=value;}
 position_+=count;if(position_==length_){state_=false;for(int c=0;c<kChannels;++c){processed[c].selection.length=length_;processed[c].area.length=confirmed[c].area.length;}}
 return count;
}
void mtEffectStereoMono::synchronizeOutputChannels(){for(int c=1;c<kChannels;++c)if(confirmed[c].area.addr!=confirmed[0].area.addr)std::memcpy(confirmed[c].area.addr,confirmed[0].area.addr,confirmed[0].area.length*sizeof(int16_t));}
#endif
