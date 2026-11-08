#include "NativeSession.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_set>

namespace {
unsigned checks=0,sets=0;int references=0;bool hidden=false,offStage=false,cycle=false,failColor=false;
uintptr_t color=0;bool selected=false;std::string label;
std::unordered_set<nativeui::Value*> registered;
void check(bool b){++checks;if(!b){std::fprintf(stderr,"FAIL session check %u\n",checks);std::exit(1);}}
nativeui::Value object(uintptr_t id){nativeui::Value v;v.type=0x4a;v.interfacePointer=0x100;v.object=id;return v;}
void pin(nativeui::Value* out){check(registered.insert(out).second);out->links[0]=reinterpret_cast<uintptr_t>(out);out->links[1]=0x1234;++references;}
void acquire(nativeui::Value* out,uintptr_t id){*out=object(id);pin(out);}
bool get(void*,void* parent,const char* name,nativeui::Value* out,bool){
    const auto id=reinterpret_cast<uintptr_t>(parent);
    if(!std::strcmp(name,"stage")){if(offStage)out->type=1;else acquire(out,100);return true;}
    if(!std::strcmp(name,"visible")){*out=nativeui::boolean(!hidden);return true;}
    if(!std::strcmp(name,"alpha")){*out=nativeui::number(1);return true;}
    if(!std::strcmp(name,"parent")){if(cycle)acquire(out,id);else out->type=1;return true;}
    if(!std::strcmp(name,"tfCont")){acquire(out,6);return true;}
    if(id==6&&!std::strcmp(name,"tf")){acquire(out,7);return true;}
    return false;
}
bool set(void*,void*,const char* name,const nativeui::Value* value,bool){
    ++sets;
    if(!std::strcmp(name,"selected"))selected=value->object!=0;
    if(!std::strcmp(name,"label"))label=reinterpret_cast<const char*>(value->object);
    if(!std::strcmp(name,"textColor")){color=value->object;return !failColor;}
    return true;
}
void retain(void* iface,nativeui::Value* value,void* data){check(iface==reinterpret_cast<void*>(0x100)&&data==reinterpret_cast<void*>(value->object));pin(value);}
void release(void*,nativeui::Value* value,void*){check(value->links[0]==reinterpret_cast<uintptr_t>(value));check(registered.erase(value)==1);--references;check(references>=0);value->links[0]=value->links[1]=UINT64_MAX;}
}
int main(){
    nativeui::Api api{get,set,nullptr,release,nullptr,nullptr,nullptr,retain};
    nativeui::Session session(api);void* movie=reinterpret_cast<void*>(1);
    session.add(movie,object(2),object(4));check(references==2&&session.has(movie));
    check(session.tick(movie,0,"АВТОПИЛОТ [F6]")&&!selected&&color==0xd2d5d1&&label=="АВТОПИЛОТ [F6]");
    const auto oldSets=sets;check(session.tick(movie,0,"АВТОПИЛОТ [F6]")&&sets==oldSets&&references==2);
    check(session.tick(movie,1,"АВТОПИЛОТ [F6]")&&selected&&color==0xd5e07a&&references==2);
    hidden=true;check(!session.tick(movie,1,"АВТОПИЛОТ [F6]"));hidden=false;
    offStage=true;check(!session.tick(movie,1,"АВТОПИЛОТ [F6]"));offStage=false;
    cycle=true;check(!session.tick(movie,1,"АВТОПИЛОТ [F6]")&&references==2);cycle=false;
    failColor=true;check(!session.tick(movie,2,"АВТОПИЛОТ [F6]"));failColor=false;
    check(session.tick(movie,0,"АВТОПИЛОТ [F6]")&&references==2);
    session.drop(movie);check(references==0&&!session.has(movie));
    const auto afterDrop=sets;check(!session.tick(movie,1,"stale")&&sets==afterDrop);
    session.add(movie,object(2),object(4));session.add(movie,object(2),object(5));check(references==2);
    session.drop(movie);check(references==0);
    for(uintptr_t i=0;i<17;++i)session.add(movie,object(1000+i),object(2000+i));
    check(references==32);session.drop(movie);check(references==0);
    check(registered.empty());
    std::printf("PASS: %u native session ownership/visibility/state checks\n",checks);
}
