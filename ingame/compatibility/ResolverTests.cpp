#include "SignatureResolver.h"
#include "ExperimentalPolicy.h"
#include "ReviewedBuilds.h"
#include <cstdio>
#include <cstdlib>

namespace {
unsigned checks=0;
void check(bool okay){++checks;if(!okay){std::fprintf(stderr,"FAIL compatibility check %u\n",checks);std::exit(1);}}
template<class T>void put(std::vector<uint8_t>& b,size_t o,T v){std::memcpy(b.data()+o,&v,sizeof(v));}
std::vector<uint8_t> fixture(unsigned shift=0) {
    std::vector<uint8_t> b(0x3600);constexpr unsigned pe=0x80;constexpr uint64_t base=0x140000000;
    put<uint16_t>(b,0,0x5a4d);put<uint32_t>(b,0x3c,pe);put<uint32_t>(b,pe,0x4550);put<uint16_t>(b,pe+4,0x8664);put<uint16_t>(b,pe+6,3);put<uint16_t>(b,pe+20,240);put<uint16_t>(b,pe+24,0x20b);put<uint64_t>(b,pe+48,base);put<uint32_t>(b,pe+80,0x5000);put<uint32_t>(b,pe+132,16);put<uint32_t>(b,pe+160,0x4000);put<uint32_t>(b,pe+164,10*12);
    for(unsigned i=0;i<3;++i){const auto s=pe+264+i*40;put<uint32_t>(b,s+8,i==0?0x2000:0x1000);put<uint32_t>(b,s+12,i==0?0x1000:i==1?0x3000:0x4000);put<uint32_t>(b,s+16,i==0?0x2000:i==1?0x1000:0x200);put<uint32_t>(b,s+20,i==0?0x400:i==1?0x2400:0x3400);put<uint32_t>(b,s+36,i==0?0x60000020:0x40000040);}
    for(unsigned i=0;i<10;++i) {
        const auto rva=0x1000+shift+i*0x100,raw=0x400+shift+i*0x100;
        auto p=srcompat::pattern(srcompat::symbols[i].pattern);
        for(size_t j=0;j<p.size();++j)b[raw+j]=p[j]<0?0:static_cast<uint8_t>(p[j]);
        put<uint32_t>(b,0x3400+i*12,rva);put<uint32_t>(b,0x3404+i*12,rva+0x100);
    }
    const unsigned ref=0x400+shift+160,rva=0x1000+shift+160;
    b[ref]=0x48;b[ref+1]=0x8d;b[ref+2]=0x0d;put<int32_t>(b,ref+3,0x3300-(rva+7));
    constexpr char anchor[]="Trying call %s.Attach(%s) on ALREADY inited object";std::memcpy(b.data()+0x2700,anchor,sizeof(anchor));
    for(unsigned i=0;i<25;++i)put<uint64_t>(b,0x2500+i*8,base+0x1000+shift);
    put<uint64_t>(b,0x2500,base+0x1900+shift);put<uint64_t>(b,0x2500+24*8,base+0x1800+shift);
    return b;
}
void reject(const std::vector<uint8_t>& bytes){bool failed=false;try{srcompat::Image::parse(bytes);}catch(const std::runtime_error&){failed=true;}check(failed);}
}
int main() {
    check(srcompat::reviewedBuild("a3cb442ec138eb2eb7e335e5c80e376766550740e49e233ac9eace035e182285"));
    check(srcompat::reviewedBuild("13ea28ab4b7e85770bcee97d35d50156516d3d3248f2428b27a5bbe512d831d0"));
    check(!srcompat::reviewedBuild(std::string(64,'0')));
    check(!srcompat::reviewedBuild(""));
    for(bool candidates:{false,true}) check(srcompat::allowHooks(candidates)==candidates);
    check(!srcompat::stagePolicy(-1,true).valid&&!srcompat::stagePolicy(3,true).valid);
    auto stage=srcompat::stagePolicy(0,true);check(stage.valid&&!stage.hud&&!stage.frames&&!stage.input);
    stage=srcompat::stagePolicy(1,true);check(stage.valid&&stage.hud&&stage.frames&&!stage.input);
    stage=srcompat::stagePolicy(2,true);check(stage.valid&&stage.hud&&stage.frames&&stage.input);
    stage=srcompat::stagePolicy(2,false);check(stage.valid&&!stage.hud&&!stage.frames&&!stage.input);
    for(unsigned shift:{0u,0x30u,0x80u}) {
        auto image=srcompat::Image::parse(fixture(shift));auto r=srcompat::resolve(image);
        check(r.candidate());check(r.addresses.at("get")==0x1200+shift);check(r.addresses.at("destroy")==0x1900+shift);check(r.vtableRva==0x3100);
    }
    auto b=fixture();b[0x400+0x200]^=1;check(!srcompat::resolve(srcompat::Image::parse(b)).candidate());
    b=fixture();b[0x2700]^=1;auto r=srcompat::resolve(srcompat::Image::parse(b));check(r.unique&&!r.factoryAnchor&&!r.candidate());
    b=fixture();put<uint64_t>(b,0x2500+24*8,0x140001000);check(!srcompat::resolve(srcompat::Image::parse(b)).candidate());
    b=fixture();std::memcpy(b.data()+0x2a00,b.data()+0x2500,25*8);check(!srcompat::resolve(srcompat::Image::parse(b)).candidate());
    b=fixture();put<uint32_t>(b,0x3400+2*12,0x1201);check(!srcompat::resolve(srcompat::Image::parse(b)).candidate());
    b=fixture();std::memcpy(b.data()+0x1e00,b.data()+0x600,80);put<uint32_t>(b,0x80+164,11*12);put<uint32_t>(b,0x3400+10*12,0x2a00);put<uint32_t>(b,0x3404+10*12,0x2b00);
    r=srcompat::resolve(srcompat::Image::parse(b));check(!r.candidate()&&r.matches.at("get").rvas.size()==2);
    b=fixture();auto p=srcompat::pattern(srcompat::symbols[0].pattern);for(size_t i=0;i<p.size();++i)if(p[i]<0)b[0x400+i]=0xa5;check(srcompat::resolve(srcompat::Image::parse(b)).candidate());
    // Generic deleting destructors can share bytes; only one vtable link
    // must pick the destructor paired with Movie::Advance at slot 24.
    b=fixture();std::memcpy(b.data()+0x1e00,b.data()+0xd00,80);put<uint32_t>(b,0x80+164,11*12);put<uint32_t>(b,0x3400+10*12,0x2a00);put<uint32_t>(b,0x3404+10*12,0x2b00);
    r=srcompat::resolve(srcompat::Image::parse(b));check(r.candidate()&&r.matches.at("destroy").rawMatches==2&&r.addresses.at("destroy")==0x1900);
    for(size_t length:{size_t(0),size_t(2),size_t(63),size_t(200),size_t(500)}){b=fixture();b.resize(length);reject(b);}
    b=fixture();put<uint16_t>(b,0x84,0x14c);reject(b);
    b=fixture();put<uint32_t>(b,0x3c,0xfffffff0);reject(b);
    b=fixture();put<uint32_t>(b,0x80+164,13);reject(b);
    b=fixture();put<uint32_t>(b,0x80+264+16,0xffffffff);reject(b);
    b=fixture();put<uint32_t>(b,0x80+264+40+12,0x1100);reject(b);
    b=fixture();put<uint64_t>(b,0x80+48,UINT64_MAX);reject(b);
    b=fixture();std::memcpy(b.data()+0x2c00,b.data()+0x600,80);check(srcompat::resolve(srcompat::Image::parse(b)).candidate());
    for(const char* text:{"","?? ?? ?? ??","GG 00 11 22","00 1 22 33"}){bool bad=false;try{srcompat::find(srcompat::Image::parse(fixture()),srcompat::pattern(text));}catch(const std::runtime_error&){bad=true;}check(bad);}
    std::printf("PASS: %u compatibility relocation/ambiguity/PE bounds checks\n",checks);
}
