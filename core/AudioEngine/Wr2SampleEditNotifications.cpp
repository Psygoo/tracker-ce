#if TRACKER_CE_ENABLE_WR2 && (TRACKER_CE_ENABLE_SD_MONO || TRACKER_CE_ENABLE_SD_EDITS)
#include "SampleEditNotifications.h"
#include "NativeBridge.h"
#include <cstdint>
namespace sampleedit {
bool metadataReady(unsigned slot){
 auto&row=wr2native::publication().instrument[slot];wr2::audio::Snapshot before;
 return row.writable()&&(!row.read(before)||before.topology!=UINT32_MAX);
}
void metadataChanged(unsigned slot,bool structural){
 auto&row=wr2native::publication().instrument[slot];wr2::audio::Snapshot snapshot;
 if(row.read(snapshot)){
  if(structural)snapshot.state.reset(snapshot.state.sliceCount<=48?snapshot.state.sliceCount:0);
  if(row.publish(snapshot.state,snapshot.topology+1)!=wr2::audio::PublishResult::Published)row.retire();
 }
 wr2native::sampleChanged(slot);
}
}
#endif
