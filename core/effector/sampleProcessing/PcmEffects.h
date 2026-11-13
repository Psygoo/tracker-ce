#pragma once
#include <stdint.h>
// PCM16 operations shared by RAM, plugins and fixed-block file processing.
static inline int ce_pcm_peak(int16_t value){return value<0?-(int32_t)value:value;}
static inline int16_t ce_pcm_scale(int16_t value,float gain){
    if(!(gain>=0.0f)||gain>32767.0f)return 0;
    int32_t result=(int32_t)(value*gain);
    if(result>32767)result=32767;
    if(result< -32768)result=-32768;
    return (int16_t)result;
}
static inline float ce_pcm_normalize_gain(int level,int peak){return peak?((float)((level*32767)/100)/peak):0.0f;}
static inline int16_t ce_pcm_mono(int16_t left,int16_t right,unsigned source,float gain,int stereo){
    int32_t sample=left;
    if(stereo){switch(source){case 0:sample=((int32_t)left+right)/2;break;case 1:break;case 2:sample=right;break;case 3:sample=((int32_t)left-right)/2;break;}}
    return gain==1.0f?(int16_t)sample:ce_pcm_scale((int16_t)sample,gain);
}
