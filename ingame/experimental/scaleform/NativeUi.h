#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace nativeui {
// Exact observed ABI of the supported SnowRunner EXE, not a generic SDK type.
struct Value {
    uint64_t cache[2] = {UINT64_MAX, UINT64_MAX};
    uintptr_t interfacePointer = 0;
    uint32_t type = 0, padding = 0;
    uintptr_t object = 0, auxiliary = 0;
};
static_assert(sizeof(Value) == 48 && offsetof(Value,type) == 0x18);
using Get = bool(*)(void*,void*,const char*,Value*,bool);
using Set = bool(*)(void*,void*,const char*,const Value*,bool);
using Invoke = bool(*)(void*,void*,Value*,const char*,const Value*,unsigned,bool);
using Release = void(*)(void*,Value*,void*);
using Attach = bool(*)(void*,void*,Value*,const char*,const char*,int,const void*);
using Empty = bool(*)(void*,void*,Value*,const char*,int);
struct Api {
    Get get; Set set; Invoke invoke; Release release; Attach attach; Empty empty;
};
inline unsigned type(const Value& value) { return value.type & 0x8f; }
inline bool object(const Value& value) { return value.interfacePointer && value.object && (type(value) == 8 || type(value) == 10); }
inline void* iface(const Value& v) { return reinterpret_cast<void*>(v.interfacePointer); }
inline void* data(const Value& v) { return reinterpret_cast<void*>(v.object); }
inline bool display(const Value& v) { return type(v) == 10; }
inline Value number(double n) { Value v; v.type = 5; std::memcpy(&v.object,&n,sizeof(n)); return v; }
inline Value string(const char* s) { Value v; v.type = 6; v.object = reinterpret_cast<uintptr_t>(s); return v; }
inline Value boolean(bool b) { Value v; v.type = 2; v.object = b; return v; }
inline bool asNumber(const Value& v,double& n) {
    switch (type(v)) {
        case 3:n=static_cast<int32_t>(v.object);return true;
        case 4:n=static_cast<uint32_t>(v.object);return true;
        case 5:std::memcpy(&n,&v.object,sizeof(n));return true;
        default:return false;
    }
}
class Owned {
    const Api& api;
public:
    Value value;
    explicit Owned(const Api& a):api(a){}
    Owned(const Owned&) = delete;
    Owned& operator=(const Owned&) = delete;
    ~Owned() { if (value.type & 0x40) api.release(iface(value),&value,data(value)); }
};
inline bool get(const Api& api,const Value& parent,const char* name,Owned& out) {
    return object(parent) && api.get(iface(parent),data(parent),name,&out.value,display(parent));
}
inline bool set(const Api& api,const Value& parent,const char* name,const Value& value) {
    return object(parent) && api.set(iface(parent),data(parent),name,&value,display(parent));
}
inline bool call(const Api& api,const Value& parent,const char* name,const Value* args,unsigned count,Owned& out) {
    return object(parent) && api.invoke(iface(parent),data(parent),&out.value,name,args,count,display(parent));
}
enum class Stage : unsigned { NoPanel=1,NoBrake,NoPosition,Existing,CreateFailed,BuildFailed,Created };
// Must be called synchronously inside the game's HUD creation callback.
// The display list owns the resulting child; no managed reference escapes.
inline Stage addTestRow(const Api& api,const Value& hud) {
    Owned panel(api),brake(api),x(api),y(api),existing(api);
    if (!get(api,hud,"rightHudPanelContainer",panel) || !object(panel.value)) return Stage::NoPanel;
    if (!get(api,panel.value,"brakeBtn",brake) || !object(brake.value)) return Stage::NoBrake;
    double px=0,py=0;
    if (!get(api,brake.value,"x",x) || !get(api,brake.value,"y",y) ||
        !asNumber(x.value,px) || !asNumber(y.value,py)) return Stage::NoPosition;
    const Value name = string("srAutopilotProbe");
    if (!call(api,panel.value,"getChildByName",&name,1,existing)) return Stage::CreateFailed;
    if (object(existing.value)) return Stage::Existing;
    Owned row(api),bg(api),text(api),tf(api),stopped(api);
    if (!api.empty(iface(panel.value),data(panel.value),&row.value,"srAutopilotProbe",-1) || !object(row.value)) return Stage::CreateFailed;
    const Value off = string("none");
    const bool built = set(api,row.value,"x",number(px)) && set(api,row.value,"y",number(py-96)) &&
        set(api,row.value,"mouseEnabled",boolean(false)) && set(api,row.value,"mouseChildren",boolean(false)) &&
        api.attach(iface(row.value),data(row.value),&bg.value,"hudBtnBg","srProbeBg",-1,nullptr) && object(bg.value) &&
        call(api,bg.value,"gotoAndStop",&off,1,stopped) &&
        api.attach(iface(row.value),data(row.value),&text.value,"hudBtnText","srProbeText",-1,nullptr) && object(text.value) &&
        set(api,text.value,"x",number(15)) && set(api,text.value,"y",number(6)) &&
        get(api,text.value,"tf",tf) && object(tf.value) && set(api,tf.value,"text",string("TEST"));
    if (!built) {
        Owned removed(api);
        call(api,panel.value,"removeChild",&row.value,1,removed);
        return Stage::BuildFailed;
    }
    return Stage::Created;
}
}
