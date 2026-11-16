#pragma once
namespace srcompat {
struct StagePolicy {bool valid,hud,frames,input;};
inline StagePolicy stagePolicy(int stage,bool enabled) {
    return {stage>=0&&stage<=2,enabled&&stage>=1&&stage<=2,enabled&&stage>=1&&stage<=2,enabled&&stage==2};
}
// Build hashes identify prior testing; structural checks decide admission.
inline bool allowHooks(bool candidates) { return candidates; }
}
