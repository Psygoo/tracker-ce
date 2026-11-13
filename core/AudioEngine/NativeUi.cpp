#if TRACKER_CE_ENABLE_WR2
#include "mtStructs.h"
#include "NativeBridge.h"
#include "NativeUi.h"
#include "../../modules/wr2/UiModel.cpp"
extern strMtProject mtProject;
namespace wr2ui {
namespace {wr2::ui::Editor instance(wr2native::publication());}
wr2::ui::Editor&editor(){return instance;}
void invalidate(strInstrument*p){
    // Equality scan also accepts pointers to temporary native structs safely.
    for(unsigned i=0;i<wr2::kMaxInstruments;++i)if(p==&mtProject.instrument[i]){instance.invalidate(i);break;}
}
void resetProject(){instance.invalidateAll();}
}
#endif
