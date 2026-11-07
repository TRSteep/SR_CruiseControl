#include "NativeUi.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>

namespace {
unsigned checks=0; int references=0; unsigned emptyCalls=0, removals=0, mouseDisabled=0;
bool missingPanel=false, failText=false, duplicate=false;
double rowY=0; bool wroteTest=false;
void check(bool b) { ++checks; if (!b) { std::fprintf(stderr,"FAIL native UI check %u\n",checks); std::exit(1); } }
nativeui::Value object(unsigned id) {
    nativeui::Value v; v.type=0x4a; v.interfacePointer=0x100; v.object=id; return v;
}
void acquire(nativeui::Value* out,unsigned id) { *out=object(id); ++references; }
bool get(void*,void* parent,const char* name,nativeui::Value* out,bool isDisplay) {
    check(isDisplay);
    const auto id=reinterpret_cast<uintptr_t>(parent);
    if (id==1 && !std::strcmp(name,"rightHudPanelContainer")) { if (missingPanel) return false; acquire(out,2); }
    else if (id==2 && !std::strcmp(name,"brakeBtn")) acquire(out,3);
    else if (id==3 && !std::strcmp(name,"x")) *out=nativeui::number(0);
    else if (id==3 && !std::strcmp(name,"y")) *out=nativeui::number(8);
    else if (id==6 && !std::strcmp(name,"tf")) { if (failText) return false; acquire(out,7); }
    else return false;
    return true;
}
bool set(void*,void* parent,const char* name,const nativeui::Value* value,bool isDisplay) {
    check(isDisplay); const auto id=reinterpret_cast<uintptr_t>(parent);
    if (id==4 && !std::strcmp(name,"y")) check(nativeui::asNumber(*value,rowY));
    if (id==4 && (!std::strcmp(name,"mouseEnabled") || !std::strcmp(name,"mouseChildren"))) {
        check(nativeui::type(*value)==2 && value->object==0); ++mouseDisabled;
    }
    if (id==7 && !std::strcmp(name,"text")) { check(nativeui::type(*value)==6); wroteTest=!std::strcmp(reinterpret_cast<const char*>(value->object),"TEST"); }
    return true;
}
bool invoke(void*,void* parent,nativeui::Value* result,const char* name,const nativeui::Value* args,unsigned count,bool isDisplay) {
    check(isDisplay && count==1); const auto id=reinterpret_cast<uintptr_t>(parent);
    if (id==2 && !std::strcmp(name,"getChildByName")) {
        check(!std::strcmp(reinterpret_cast<const char*>(args->object),"srAutopilotProbe"));
        if (duplicate) acquire(result,4); else result->type=1;
    } else if (id==5 && !std::strcmp(name,"gotoAndStop")) {
        check(!std::strcmp(reinterpret_cast<const char*>(args->object),"none"));
    } else if (id==2 && !std::strcmp(name,"removeChild")) { check(args->object==4); ++removals; acquire(result,4); }
    else return false;
    return true;
}
void release(void* iface,nativeui::Value* value,void* data) {
    check(iface==reinterpret_cast<void*>(0x100) && data==reinterpret_cast<void*>(value->object)); --references; check(references>=0);
}
bool attach(void*,void* parent,nativeui::Value* out,const char* symbol,const char*,int depth,const void* init) {
    check(parent==reinterpret_cast<void*>(4) && depth==-1 && init==nullptr);
    if (!std::strcmp(symbol,"hudBtnBg")) acquire(out,5);
    else if (!std::strcmp(symbol,"hudBtnText")) acquire(out,6);
    else return false;
    return true;
}
bool empty(void*,void* parent,nativeui::Value* out,const char* name,int depth) {
    check(parent==reinterpret_cast<void*>(2) && !std::strcmp(name,"srAutopilotProbe") && depth==-1);
    ++emptyCalls; acquire(out,4); return true;
}
}
int main() {
    nativeui::Api api{get,set,invoke,release,attach,empty};
    check(nativeui::addTestRow(api,object(1))==nativeui::Stage::Created);
    check(references==0 && emptyCalls==1 && removals==0 && mouseDisabled==2 && wroteTest && std::abs(rowY+88)<.001);
    duplicate=true; check(nativeui::addTestRow(api,object(1))==nativeui::Stage::Existing);
    check(references==0 && emptyCalls==1); duplicate=false;
    failText=true; check(nativeui::addTestRow(api,object(1))==nativeui::Stage::BuildFailed);
    check(references==0 && removals==1); failText=false;
    missingPanel=true; check(nativeui::addTestRow(api,object(1))==nativeui::Stage::NoPanel);
    check(references==0 && emptyCalls==2);
    std::printf("PASS: %u native UI ABI/lifetime/rollback checks\n",checks);
}
