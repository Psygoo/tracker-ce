#if TRACKER_CE_ENABLE_SD_MONO || TRACKER_CE_ENABLE_SD_EDITS
#include "SampleEditNotifications.h"
namespace sampleedit {
bool metadataReady(unsigned){return true;}
void metadataChanged(unsigned,bool){}
}
#endif
