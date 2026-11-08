#pragma once
#include "NativeUi.h"
#include <cmath>
#include <utility>

namespace nativeui {
inline uint32_t cruiseIconColor(int state) { return state==1?0xd5e07a:state==2?0xef744e:0xd2d5d1; }
inline Value rgb(uint32_t color) { Value result;result.type=4;result.object=color;return result; }
inline bool graphicsCall(const Api& api,const Value& target,const char* method,std::initializer_list<Value> args) {
    // Drawing arguments are primitive, unmanaged Values; their contiguous
    // array stays at one address for the synchronous call.
    Owned result(api);
    return call(api,target,method,args.begin(),static_cast<uint64_t>(args.size()),result);
}
inline bool drawCruiseIcon(const Api& api,const Value& row,int state) {
    Owned icon(api),graphics(api);
    const Value name=string("srAutopilotIcon");
    if(!call(api,row,"getChildByName",&name,1,icon)||!object(icon.value)||
       !get(api,icon.value,"graphics",graphics)||!object(graphics.value)||
       !set(api,icon.value,"visible",boolean(false)))return false;
    const auto& g=graphics.value;const Value color=rgb(cruiseIconColor(state));
    if(!graphicsCall(api,g,"clear",{})||
       !graphicsCall(api,g,"lineStyle",{number(2.25),color,number(1)}))return false;
    constexpr double pi=3.14159265358979323846,center=16,radius=13.5;
    auto radians=[=](double degrees){return degrees*pi/180;};
    auto point=[&](double angle,double r){return std::pair<double,double>{center+r*std::cos(radians(angle)),center+r*std::sin(radians(angle))};};
    const auto start=point(135,radius);
    if(!graphicsCall(api,g,"moveTo",{number(start.first),number(start.second)}))return false;
    for(unsigned i=0;i<6;++i) {
        const double angle=135+i*45;
        const auto control=point(angle+22.5,radius/std::cos(radians(22.5)));
        const auto end=point(angle+45,radius);
        if(!graphicsCall(api,g,"curveTo",{number(control.first),number(control.second),number(end.first),number(end.second)}))return false;
    }
    for(unsigned i=0;i<7;++i) {
        const double angle=135+i*45;const auto inner=point(angle,10.5),outer=point(angle,radius);
        if(!graphicsCall(api,g,"moveTo",{number(inner.first),number(inner.second)})||
           !graphicsCall(api,g,"lineTo",{number(outer.first),number(outer.second)}))return false;
    }
    return graphicsCall(api,g,"moveTo",{number(center),number(center)})&&
        graphicsCall(api,g,"lineTo",{number(24),number(8)})&&
        graphicsCall(api,g,"lineStyle",{number(0),color,number(0)})&&
        graphicsCall(api,g,"beginFill",{color,number(1)})&&
        graphicsCall(api,g,"drawCircle",{number(center),number(center),number(2.2)})&&
        graphicsCall(api,g,"endFill",{})&&set(api,icon.value,"visible",boolean(true));
}
}
