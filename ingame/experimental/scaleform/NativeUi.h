#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <initializer_list>

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
    void (*trace)(const char* operation,const char* member,bool success) = nullptr;
};
inline void trace(const Api& api,const char* operation,const char* member,bool success=false) {
    if (api.trace) api.trace(operation,member,success);
}
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
    ~Owned() {
        if (value.type & 0x40) {
            trace(api,"release.begin","");
            api.release(iface(value),&value,data(value));
            trace(api,"release.end","",true);
        }
    }
};
inline bool get(const Api& api,const Value& parent,const char* name,Owned& out) {
    if (!object(parent)) return false;
    trace(api,"get.begin",name);
    const bool okay=api.get(iface(parent),data(parent),name,&out.value,display(parent));
    trace(api,"get.end",name,okay); return okay;
}
inline bool set(const Api& api,const Value& parent,const char* name,const Value& value) {
    if (!object(parent)) return false;
    trace(api,"set.begin",name);
    const bool okay=api.set(iface(parent),data(parent),name,&value,display(parent));
    trace(api,"set.end",name,okay); return okay;
}
inline bool call(const Api& api,const Value& parent,const char* name,const Value* args,unsigned count,Owned& out) {
    if (!object(parent)) return false;
    trace(api,"invoke.begin",name);
    const bool okay=api.invoke(iface(parent),data(parent),&out.value,name,args,count,display(parent));
    trace(api,"invoke.end",name,okay); return okay;
}
inline bool attach(const Api& api,const Value& parent,Owned& out,const char* symbol,const char* instance) {
    trace(api,"attach.begin",symbol);
    const bool okay=api.attach(iface(parent),data(parent),&out.value,symbol,instance,-1,nullptr);
    trace(api,"attach.end",symbol,okay); return okay && object(out.value);
}
inline bool empty(const Api& api,const Value& parent,Owned& out,const char* instance) {
    trace(api,"sprite.begin",instance);
    const bool okay=api.empty(iface(parent),data(parent),&out.value,instance,-1);
    trace(api,"sprite.end",instance,okay); return okay && object(out.value);
}
enum class Stage : unsigned { NoPanel=1,NoBrake,NoPosition,Existing,CreateFailed,BuildFailed,Created };
// Candidate entry point after the game's own brakeBtn read has returned.
// This is not yet validated in a live driving HUD. Never call from creation.
// The display list owns the resulting child; no managed reference escapes.
inline Stage addTestRowToPanel(const Api& api,const Value& panel,const Value& brake) {
    if (!object(panel)) return Stage::NoPanel;
    if (!object(brake)) return Stage::NoBrake;
    // The caller gates this to one attempt per parent. Do not Invoke AS3
    // methods here: getChildByName did not return in the live v5 test.
    Owned x(api),y(api);
    double px=0,py=0;
    if (!get(api,brake,"x",x) || !get(api,brake,"y",y) ||
        !asNumber(x.value,px) || !asNumber(y.value,py)) return Stage::NoPosition;
    // Use the library-backed parent's symbol context. A new built-in Sprite
    // was created successfully in v6, but could not resolve hudBtnBg.
    Owned row(api),text(api),tf(api);
    if (!attach(api,panel,row,"hudBrakeBtn","srAutopilotProbe")) return Stage::CreateFailed;
    const bool built = set(api,row.value,"x",number(px)) && set(api,row.value,"y",number(py-96)) &&
        set(api,row.value,"mouseEnabled",boolean(false)) && set(api,row.value,"mouseChildren",boolean(false)) &&
        set(api,row.value,"tabEnabled",boolean(false)) && set(api,row.value,"label",string("TEST")) &&
        get(api,row.value,"tfCont",text) && object(text.value) &&
        get(api,text.value,"tf",tf) && object(tf.value) && set(api,tf.value,"text",string("TEST"));
    if (!built) {
        // Keep a failed child invisible until the HUD destroys its display
        // list. Avoid removeChild/Invoke on the unverified method-call ABI.
        set(api,row.value,"visible",boolean(false));
        return Stage::BuildFailed;
    }
    // The test row has no handbrake action or key binding. Suppress the
    // inherited brake glyph and key badge instead of suggesting that it does.
    for (const char* child : {"iconContainer","buttonAttach"}) {
        Owned decoration(api);
        if (get(api,row.value,child,decoration) && object(decoration.value))
            set(api,decoration.value,"visible",boolean(false));
    }
    return Stage::Created;
}
}
