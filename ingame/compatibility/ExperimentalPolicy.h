#pragma once
namespace srcompat {
struct StagePolicy {bool valid,hud,frames,input;};
inline StagePolicy stagePolicy(int stage,bool enabled) {
    return {stage>=0&&stage<=2,enabled&&stage>=1&&stage<=2,enabled&&stage>=1&&stage<=2,enabled&&stage==2};
}
inline bool allowHooks(bool reviewed,bool experimental,bool candidates) { return candidates&&(reviewed||experimental); }
}
