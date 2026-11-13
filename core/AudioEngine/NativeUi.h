#pragma once
#if TRACKER_CE_ENABLE_WR2
#include "../../modules/wr2/UiModel.h"
struct strInstrument;
namespace wr2ui {
wr2::ui::Editor& editor();
// Called only from foreground lifecycle operations, before native replacement.
void invalidate(strInstrument*);
void resetProject();
}
#endif
