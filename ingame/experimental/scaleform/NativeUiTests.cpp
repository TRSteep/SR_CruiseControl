#include "NativeUi.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>

namespace {
unsigned checks=0; int references=0; unsigned emptyCalls=0, removals=0, mouseDisabled=0;
unsigned invokeCalls=0,hiddenRows=0,attachCalls=0,hiddenDecorations=0;
bool missingPanel=false, failText=false, duplicate=false;
double rowY=0; bool wroteTest=false;
unsigned traceBegins=0,traceEnds=0,failedCalls=0;
void trace(const char* operation,const char*,bool success) {
    if (std::strstr(operation,".begin")) ++traceBegins;
    if (std::strstr(operation,".end")) { ++traceEnds; if (!success) ++failedCalls; }
}
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
    else if (id==4 && !std::strcmp(name,"tfCont")) acquire(out,6);
    else if (id==4 && !std::strcmp(name,"iconContainer")) acquire(out,8);
    else if (id==4 && !std::strcmp(name,"buttonAttach")) acquire(out,9);
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
    if (id==4 && !std::strcmp(name,"visible")) { check(nativeui::type(*value)==2 && value->object==0); ++hiddenRows; }
    if ((id==8 || id==9) && !std::strcmp(name,"visible")) { check(nativeui::type(*value)==2 && value->object==0); ++hiddenDecorations; }
    if (id==7 && !std::strcmp(name,"text")) { check(nativeui::type(*value)==6); wroteTest=!std::strcmp(reinterpret_cast<const char*>(value->object),"TEST"); }
    return true;
}
bool invoke(void*,void* parent,nativeui::Value* result,const char* name,const nativeui::Value* args,unsigned count,bool isDisplay) {
    ++invokeCalls;
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
bool attach(void*,void* parent,nativeui::Value* out,const char* symbol,const char* instance,int depth,const void* init) {
    check(parent==reinterpret_cast<void*>(2) && depth==-1 && init==nullptr);
    check(!std::strcmp(symbol,"hudBrakeBtn") && !std::strcmp(instance,"srAutopilotProbe"));
    ++attachCalls; acquire(out,4);
    return true;
}
bool empty(void*,void* parent,nativeui::Value* out,const char* name,int depth) {
    check(parent==reinterpret_cast<void*>(2) && !std::strcmp(name,"srAutopilotProbe") && depth==-1);
    ++emptyCalls; acquire(out,4); return true;
}
}
int main() {
    nativeui::Api api{get,set,invoke,release,attach,empty,trace};
    check(nativeui::addTestRowToPanel(api,object(2),object(3))==nativeui::Stage::Created);
    check(references==0 && emptyCalls==0 && attachCalls==1 && removals==0 && mouseDisabled==2 && hiddenDecorations==2 && wroteTest && std::abs(rowY+88)<.001);
    duplicate=true; check(nativeui::addTestRowToPanel(api,object(2),nativeui::Value{})==nativeui::Stage::NoBrake);
    check(references==0 && emptyCalls==0 && attachCalls==1); duplicate=false;
    failText=true; check(nativeui::addTestRowToPanel(api,object(2),object(3))==nativeui::Stage::BuildFailed);
    check(references==0 && removals==0 && hiddenRows==1); failText=false;
    missingPanel=true; check(nativeui::addTestRowToPanel(api,nativeui::Value{},object(3))==nativeui::Stage::NoPanel);
    check(references==0 && emptyCalls==0 && attachCalls==2);
    check(traceBegins==traceEnds && traceBegins>0 && failedCalls==1);
    check(invokeCalls==0);
    std::printf("PASS: %u native UI ABI/lifetime/rollback checks\n",checks);
}
