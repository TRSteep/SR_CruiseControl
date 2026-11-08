#define SCALEFORM_PROBE_TEST
#include "Probe.cpp"
#include <cstdlib>

unsigned checks = 0;
void check(bool result) { ++checks; if (!result) { std::fprintf(stderr,"FAIL %u\n",checks); std::exit(1); } }
void* expectedOwner; const void* expectedLinkage; unsigned createCalls = 0, attachCalls = 0;
uintptr_t fakeCreate(void* owner, const void* linkage) {
    check(owner == expectedOwner && linkage == expectedLinkage); ++createCalls;
    return 0xabcdef;
}
bool fakeAttach(void* iface, void* parent, probe::ObservedValue* result, const char* symbol,
                const char* instance, int depth, const void* init) {
    check(iface == reinterpret_cast<void*>(0x1234)); check(parent == reinterpret_cast<void*>(0x5678));
    check(!std::strcmp(symbol,"hudBtnBg") && !std::strcmp(instance,"test")); check(depth == -1 && init == nullptr);
    result->type = 0x4a; result->interfacePointer = 0x1234; result->object = 0x8765; ++attachCalls; return true;
}
bool fakeGet(void* iface,void* parent,const char* member,probe::ObservedValue* out,bool display) {
    check(iface==reinterpret_cast<void*>(0x1234) && parent==reinterpret_cast<void*>(0x5678));
    check(!std::strcmp(member,"brakeBtn") && display);
    out->type=0x4a; out->interfacePointer=0x1234; out->object=0x4321; return true;
}
unsigned nativeReads=0;
bool missingPosition(void*,void*,const char*,nativeui::Value*,bool) { ++nativeReads; return false; }
#ifdef SCALEFORM_NATIVE_AUTOPILOT
float fakeAdvance(void* movie,float delta,unsigned count,bool capture){check(movie==reinterpret_cast<void*>(1)&&delta==.125f&&count==2&&capture);return 3.125f;}
uintptr_t fakeDestroy(void* movie,unsigned flags){check(movie==reinterpret_cast<void*>(1)&&flags==1);return reinterpret_cast<uintptr_t>(movie);}
bool showThisFrame=false,nativeFrameVisible=true,customFrameVisible=true;
bool frameGet(void*,void* parent,const char* name,nativeui::Value* out,bool) {
    const auto id=reinterpret_cast<uintptr_t>(parent);
    if(!std::strcmp(name,"brakeBtn")){out->type=10;out->interfacePointer=1;out->object=8;return true;}
    if(!std::strcmp(name,"visible")){*out=nativeui::boolean(id==8?nativeFrameVisible:id==4?customFrameVisible:true);return true;}
    if(!std::strcmp(name,"alpha")){*out=nativeui::number(1);return true;}
    return false;
}
bool frameSet(void*,void* parent,const char* name,const nativeui::Value* value,bool) {
    if(parent==reinterpret_cast<void*>(4)&&!std::strcmp(name,"visible"))customFrameVisible=value->object!=0;
    return true;
}
float frameAdvance(void* movie,float delta,unsigned count,bool capture) {
    check(movie==reinterpret_cast<void*>(1)&&delta==.125f&&count==2&&capture);
    nativeFrameVisible=showThisFrame;return 6.25f;
}
#endif
int main() {
    char name[128]{};
    probe::textAt(reinterpret_cast<const char*>(1),name); check(!*name);
    probe::textAt("brakeBtn",name); check(!std::strcmp(name,"brakeBtn"));
    struct InlineString { uint32_t length = 10, capacity = 24; char text[24] = "defaultHud"; } s;
    probe::gameString(&s,name); check(!std::strcmp(name,"defaultHud"));
    struct HeapString { uint32_t length = 10, capacity = 0; const char* text = "defaultHud"; } h;
    probe::gameString(&h,name); check(!std::strcmp(name,"defaultHud"));
    h.length = 5000; probe::gameString(&h,name); check(!*name);
    alignas(16) unsigned char owner[0x58 + sizeof(probe::ObservedValue)]{};
    auto value = reinterpret_cast<probe::ObservedValue*>(owner + 0x58); value->type = 0x4a; value->object = 0x4321;
    expectedOwner = owner; expectedLinkage = &s; probe::originalCreate = fakeCreate;
    check(probe::createHook(owner,&s) == 0xabcdef); check(createCalls == 1); check(probe::ready[0].load());
    check(probe::events[0].object == 0x4321 && probe::events[0].type == 0x4a);
    probe::ObservedValue output{}; probe::originalAttach = fakeAttach;
    check(probe::attachHook(reinterpret_cast<void*>(0x1234),reinterpret_cast<void*>(0x5678),&output,"hudBtnBg","test",-1,nullptr));
    check(attachCalls == 1 && probe::events[2].success && probe::events[2].object == output.object);
    auto copy = output; probe::snapshot(probe::events[2],&output); check(!std::memcmp(&copy,&output,sizeof(output)));
    probe::originalGet=fakeGet;
    check(probe::getHook(reinterpret_cast<void*>(0x1234),reinterpret_cast<void*>(0x5678),"brakeBtn",&output,true));
    check(probe::events[3].kind==4 && probe::events[3].success && probe::events[3].object==0x4321);
    probe::enableTestRow=true;
    const unsigned before=probe::claimed.load();
    check(probe::createHook(owner,&s)==0xabcdef);
    check(!std::strcmp(probe::events[before+1].symbol,"TEST-await-brake-read") && nativeReads==0);
    probe::ui.get=missingPosition;
    check(probe::getHook(reinterpret_cast<void*>(0x1234),reinterpret_cast<void*>(0x5678),"brakeBtn",&output,true));
#ifdef SCALEFORM_NATIVE_AUTOPILOT
    constexpr unsigned expectedReads=2; // Optional language read, then position.
#else
    constexpr unsigned expectedReads=1;
#endif
    check(nativeReads==expectedReads && !probe::insideUi && output.object==0x4321);
    check(probe::getHook(reinterpret_cast<void*>(0x1234),reinterpret_cast<void*>(0x5678),"brakeBtn",&output,true));
    check(nativeReads==expectedReads); // A failed native attempt must not repeat every frame.
#ifdef SCALEFORM_NATIVE_AUTOPILOT
    check(nativecontrol::label()=="CRUISE CONTROL"&&nativecontrol::keyLabel()=="F6");
    nativecontrol::toggleKey=VK_F7;check(nativecontrol::keyLabel()=="F7");nativecontrol::toggleKey=VK_F6;
    probe::originalAdvance=fakeAdvance;probe::originalDestroy=fakeDestroy;
    check(probe::advanceHook(reinterpret_cast<void*>(1),.125f,2,true)==3.125f);
    check(probe::destroyHook(reinterpret_cast<void*>(1),1)==1);
    // The original Advance changes the native button this frame. Mirroring
    // before it would use the previous visibility and fail both assertions.
    probe::ui={frameGet,frameSet,nullptr,nullptr,nullptr,nullptr};
    nativeui::Value panel;panel.type=10;panel.interfacePointer=1;panel.object=2;
    nativeui::Value row;row.type=10;row.interfacePointer=1;row.object=4;
    probe::sessions.add(reinterpret_cast<void*>(1),panel,row);
    probe::originalAdvance=frameAdvance;
    check(probe::advanceHook(reinterpret_cast<void*>(1),.125f,2,true)==6.25f&&!customFrameVisible);
    showThisFrame=true;
    check(probe::advanceHook(reinterpret_cast<void*>(1),.125f,2,true)==6.25f&&customFrameVisible);
    check(probe::destroyHook(reinterpret_cast<void*>(1),1)==1&&!probe::sessions.has(reinterpret_cast<void*>(1)));
#endif
    std::printf("PASS: %u probe memory/ABI/forwarding checks\n",checks);
}
