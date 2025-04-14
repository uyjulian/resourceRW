// for v2link entry
#define RESOURCERW_ENTRY EntryResourceRW

// [XXX] direct includes
#include "ResourceRW.cpp"

bool onV2Link()   { return EntryResourceRW(true); }
bool onV2Unlink() { return EntryResourceRW(false); }
