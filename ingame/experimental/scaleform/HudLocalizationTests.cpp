#include "HudLocalization.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
unsigned checks=0;
void check(bool b){++checks;if(!b){std::fprintf(stderr,"FAIL localisation check %u\n",checks);std::exit(1);}}
struct Region {uintptr_t start;size_t size;};
std::vector<Region> regions;
bool read(const void* from,void* to,size_t size) {
    const auto address=reinterpret_cast<uintptr_t>(from);
    for(const auto& region:regions)
        if(address>=region.start&&address-region.start<=region.size&&size<=region.size-(address-region.start)) {
            std::memcpy(to,from,size);return true;
        }
    return false;
}
template<class T> void allow(const T* p,size_t count=1){regions.push_back({reinterpret_cast<uintptr_t>(p),sizeof(T)*count});}
}
int main() {
    using namespace hudlocale;
    check(detect("РУЧНОЙ ТОРМОЗ")==Language::Russian);
    check(detect("Ручной тормоз")==Language::Russian);
    check(detect("FREIN À MAIN")==Language::French);
    check(detect("Frein à main")==Language::French);
    check(detect("HANDBRAKE")==Language::English);
    check(detect("handbrake")==Language::English);
    check(detect("HANDBREMSE")==Language::Unknown);
    check(std::string(label(Language::Russian))=="КРУИЗ КОНТРОЛЬ");
    check(std::string(label(Language::French))=="RÉGULATEUR DE VITESSE");
    check(std::string(label(Language::Unknown))=="CRUISE CONTROL");
    for(const char* text:{"РУЧНОЙ ТОРМОЗ","FREIN À MAIN","HANDBRAKE"}) {
        regions.clear();allow(text,std::strlen(text)+1);
        auto value=nativeui::string(text);
        check(copyString(value,read)==text);
        const char* pointer=text;allow(&pointer);
        value.type=0x46;value.object=reinterpret_cast<uintptr_t>(&pointer);
        check(copyString(value,read)==text);
        regions.pop_back();check(copyString(value,read).empty());
    }
    const char16_t wide[]=u"РУЧНОЙ ТОРМОЗ";
    regions.clear();allow(wide,sizeof(wide)/sizeof(*wide));
    nativeui::Value value;value.type=7;value.object=reinterpret_cast<uintptr_t>(wide);
    check(copyString(value,read)=="РУЧНОЙ ТОРМОЗ");
    const char16_t* pointer=wide;allow(&pointer);
    value.type=0x47;value.object=reinterpret_cast<uintptr_t>(&pointer);
    check(copyString(value,read)=="РУЧНОЙ ТОРМОЗ");
    value.object=1;check(copyString(value,read).empty());
    value=nativeui::boolean(true);check(copyString(value,read).empty());
    check(copyString(nativeui::string("x"),nullptr).empty());
    char longText[128];std::memset(longText,'A',sizeof(longText));regions.clear();allow(longText,sizeof(longText));
    check(copyString(nativeui::string(longText),read).empty());
    const char16_t invalid[]={0xd800,0};allow(invalid,2);value.type=7;value.object=reinterpret_cast<uintptr_t>(invalid);
    check(copyString(value,read).empty());
    value.object=UINTPTR_MAX;check(copyString(value,read).empty());
    std::printf("PASS: %u HUD language/string safety checks\n",checks);
}
