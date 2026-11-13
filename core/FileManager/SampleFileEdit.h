#pragma once
#if TRACKER_CE_ENABLE_SD_MONO || TRACKER_CE_ENABLE_SD_EDITS
#include "sampleProcessing/Conversion.h"
class mtEffect;
namespace samplepreview { struct EffectCopy; }
namespace sampleedit {
void* workBuffer(unsigned bytes);

enum class Edit:unsigned char{None,Crop,Reverse,FadeIn,FadeOut,Normalize,Amplifier,Overdrive,Bitcrusher,Equalizer,Limiter,Compressor};
#if TRACKER_CE_ENABLE_SD_EDITS
bool beginEdit(unsigned slot,Edit edit,unsigned short start,unsigned short end,float parameter=1.0f,float secondary=0.0f,const samplepreview::EffectCopy* nativeSpec=nullptr,mtEffect* nativeSource=nullptr,unsigned effectIndex=0xffffffffu);
#endif
bool begin(unsigned slot,Mode mode=Mode::Mix,float gainDb=0.0f,unsigned short start=0,unsigned short end=65535,unsigned effectIndex=0xffffffffu);
bool beginUndo(unsigned slot,unsigned effectIndex);bool undoAvailable(unsigned slot,unsigned effectIndex);bool undoing();bool undone();void getUndoSelection(unsigned short&start,unsigned short&end);void setUndoContext(unsigned slot);void discardUndo();
bool active();void tick();void cancel();unsigned progress();bool done();bool cancellable();bool succeeded();bool applied();bool needsRecovery();const char* result();bool recover();}
#endif
