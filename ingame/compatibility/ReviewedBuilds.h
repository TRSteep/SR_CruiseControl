#pragma once
#include <string>
namespace srcompat {
inline const char* reviewedProfile(const std::string& hash) {
#define SR_CC_REVIEWED(value,name) if(hash==value)return name;
#include "ReviewedBuilds.inc"
#undef SR_CC_REVIEWED
    return nullptr;
}
inline bool reviewedBuild(const std::string& hash){return reviewedProfile(hash)!=nullptr;}
}
