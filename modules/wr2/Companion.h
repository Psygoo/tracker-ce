#pragma once
#include "Persistence.h"
namespace wr2 { namespace persist {
constexpr unsigned kCompanionBytes=288;
// CE v2: explicit <PTI>.w2i binds by PTI SHA + live metadata; rooted A/B remains compatible.
std::uint64_t readReplacements(IO&);
bool markReplacement(IO&,unsigned);
bool clearReplacements(IO&);
bool exportProject(IO&,Model&,Storage&);
bool importInstrument(IO&,Model&,Storage&,unsigned destination,const char*ptiPath);
bool copyInstrument(Model&,unsigned source,unsigned destination);
} }
