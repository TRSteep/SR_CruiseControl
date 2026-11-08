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
    check(nativeReads==1 && !probe::insideUi && output.object==0x4321);
    check(probe::getHook(reinterpret_cast<void*>(0x1234),reinterpret_cast<void*>(0x5678),"brakeBtn",&output,true));
    check(nativeReads==1); // A failed native attempt must not repeat every frame.
    std::printf("PASS: %u probe memory/ABI/forwarding checks\n",checks);
}
