#pragma once
#if TRACKER_CE_ENABLE_WR2
namespace wr2persist {
bool beginSave();bool saveReady();bool finishSave();
bool markInstrumentReplacement(unsigned);void resetWorkspace();
void copyInstrument(unsigned source,unsigned destination);
void importInstrument(unsigned destination,const char*folder,const char*name);
void stageStartupOpen();void stageOpen();void finishOpen();void abort();
}
#endif
