#pragma once
namespace sampleedit {
// Optional instrument-metadata seam. The CE implementation has no external state.
bool metadataReady(unsigned instrument);
void metadataChanged(unsigned instrument,bool structural);
}
