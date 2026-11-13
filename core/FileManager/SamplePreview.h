#pragma once
#if TRACKER_CE_ENABLE_SD_MONO || TRACKER_CE_ENABLE_SD_EDITS
#include <cstdint>
class mtEffect;
namespace samplepreview
{
// C5 native file Preview uses the clock-corrected 44100-frame/s file rate.
constexpr uint32_t kMaximumFrames = 10u * 44100u;
bool supported(unsigned effect);
// Foreground only; caller hard-stops any native Preview voice before begin.
bool begin(unsigned slot, unsigned effect, const mtEffect& original, uint16_t first, uint16_t last);
void tick();
bool active();
bool busy();
bool ready();
bool valid();
unsigned progress();
const char* error();
int16_t* data(unsigned channel);
uint32_t frames();
// Foreground only. Caller must hard-stop the native Preview voice before release.
void release();
}
#endif
