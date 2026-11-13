#if TRACKER_CE_ENABLE_WR2
// Keep the isolated module sources unchanged and compile them only for the
// experimental Tracker+ bridge. No archive member is linked in OFF builds.
#include "../../modules/wr2/WeightedRandom.cpp"
#include "../../modules/wr2/AudioAdapter.cpp"
#endif
