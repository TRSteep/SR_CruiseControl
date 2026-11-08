#include "CruiseIcon.h"
#include <cstdio>
#include <cstdlib>
#include <unordered_set>
#include <type_traits>

namespace {
unsigned checks=0,curves=0,lines=0,circles=0;uint32_t expectedColor=0;
bool visible=false,failCurve=false;std::unordered_set<nativeui::Value*> registered;
void check(bool b){++checks;if(!b){std::fprintf(stderr,"FAIL icon check %u\n",checks);std::exit(1);}}
nativeui::Value object(uintptr_t id,uint32_t type=0x4a){nativeui::Value v;v.type=type;v.interfacePointer=0x100;v.object=id;return v;}
void acquire(nativeui::Value* out,uintptr_t id,uint32_t type=0x4a){*out=object(id,type);check(registered.insert(out).second);}
bool get(void*,void* parent,const char* name,nativeui::Value* out,bool display){check(parent==reinterpret_cast<void*>(2)&&display&&!std::strcmp(name,"graphics"));acquire(out,3,0x48);return true;}
bool set(void*,void* parent,const char* name,const nativeui::Value* value,bool display){check(parent==reinterpret_cast<void*>(2)&&display&&!std::strcmp(name,"visible")&&nativeui::type(*value)==2);visible=value->object!=0;return true;}
void release(void*,nativeui::Value* value,void*){check(registered.erase(value)==1);}
bool invoke(void*,void* parent,nativeui::Value* result,const char* name,const nativeui::Value* args,uint64_t count,bool display){
    if(!std::strcmp(name,"abiProbe")){check(count==0x100000003ULL);return true;}
    if(!std::strcmp(name,"getChildByName")){
        check(parent==reinterpret_cast<void*>(1)&&display&&count==1&&nativeui::type(args[0])==6);
        check(!std::strcmp(reinterpret_cast<const char*>(args[0].object),"srAutopilotIcon"));acquire(result,2);return true;
    }
    check(parent==reinterpret_cast<void*>(3)&&!display);
    if(!std::strcmp(name,"clear")||!std::strcmp(name,"endFill")){check(count==0);return true;}
    if(!std::strcmp(name,"lineStyle")){check(count==3&&nativeui::type(args[1])==4&&args[1].object==expectedColor);return true;}
    if(!std::strcmp(name,"beginFill")){check(count==2&&nativeui::type(args[0])==4&&args[0].object==expectedColor);return true;}
    for(uint64_t i=0;i<count;++i){double n=0;check(nativeui::asNumber(args[i],n)&&std::isfinite(n)&&n>=0&&n<=32);}
    if(!std::strcmp(name,"curveTo")){check(count==4);++curves;return !failCurve;}
    if(!std::strcmp(name,"moveTo")){check(count==2);return true;}
    if(!std::strcmp(name,"lineTo")){check(count==2);++lines;return true;}
    if(!std::strcmp(name,"drawCircle")){check(count==3);++circles;return true;}
    check(false);return false;
}
static_assert(std::is_same<nativeui::Invoke,decltype(&invoke)>::value,"Invoke must use the EXE's 64-bit argument count");
}
int main(){
    nativeui::Api api{get,set,invoke,release,nullptr,nullptr};
    for(int state=0;state<3;++state){
        curves=lines=circles=0;expectedColor=state==1?0xd5e07a:state==2?0xef744e:0xd2d5d1;
        check(nativeui::drawCruiseIcon(api,object(1),state));
        check(curves==6&&lines==8&&circles==1&&visible&&registered.empty());
    }
    failCurve=true;expectedColor=0xd2d5d1;check(!nativeui::drawCruiseIcon(api,object(1),0));check(!visible&&registered.empty());
    nativeui::Owned result(api);check(nativeui::call(api,object(1),"abiProbe",nullptr,0x100000003ULL,result));
    std::printf("PASS: %u vector icon geometry/color/ABI/lifetime checks\n",checks);
}
