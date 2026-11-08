#pragma once
#if TRACKER_CE_ENABLE_SD_MONO
#include "mtEffect.h"
class mtEffectStereoMono:public mtEffect {
public:
 void setParameter(float&,int)override;
 bool startProcess()override;
 int32_t processBlock()override;
 bool getProcessState()override{return state_;}
 uint32_t getExpectedProcessLength()override{return confirmed[0].selection.length;}
 void synchronizeOutputChannels();
 void setInputStereo(bool value){stereo_=value;}
private:
 unsigned source_=0;float gain_=1;bool stereo_=true,state_=false;uint32_t position_=0,length_=0;
};
extern mtEffectStereoMono sampleEditorStereoMono;
#endif
