#pragma once
#include "SymbolPatterns.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace srcompat {
struct Section { uint32_t rva,raw,size,virtualSize,flags; };
struct Function { uint32_t begin,end; };
struct Image {
    std::vector<uint8_t> bytes;
    std::vector<Section> sections;
    std::vector<Function> functions;
    uint64_t base=0;
    uint32_t size=0,timestamp=0;
    template<class T> T at(size_t offset)const {
        if(offset>bytes.size()||sizeof(T)>bytes.size()-offset)throw std::runtime_error("truncated PE");
        T result;std::memcpy(&result,bytes.data()+offset,sizeof(T));return result;
    }
    size_t offset(uint32_t rva,size_t length=1)const {
        for(const auto& section:sections)if(rva>=section.rva&&rva-section.rva<=section.size&&length<=section.size-(rva-section.rva))return section.raw+(rva-section.rva);
        throw std::runtime_error("RVA outside file-backed section");
    }
    bool executable(uint32_t rva,size_t length=1)const {
        for(const auto& s:sections)if((s.flags&0x20000000)&&rva>=s.rva&&rva-s.rva<=s.size&&length<=s.size-(rva-s.rva))return true;
        return false;
    }
    const Function* function(uint32_t start)const {
        const auto i=std::lower_bound(functions.begin(),functions.end(),start,[](const Function& f,uint32_t r){return f.begin<r;});
        return i!=functions.end()&&i->begin==start?&*i:nullptr;
    }
    static Image parse(std::vector<uint8_t> data) {
        Image image;image.bytes=std::move(data);
        if(image.at<uint16_t>(0)!=0x5a4d)throw std::runtime_error("not an MZ executable");
        const size_t pe=image.at<uint32_t>(0x3c);
        if(image.at<uint32_t>(pe)!=0x4550||image.at<uint16_t>(pe+4)!=0x8664)throw std::runtime_error("not an x64 PE executable");
        const unsigned count=image.at<uint16_t>(pe+6),optional=image.at<uint16_t>(pe+20);
        if(!count||count>96||optional<240||image.at<uint16_t>(pe+24)!=0x20b)throw std::runtime_error("unsupported PE headers");
        image.timestamp=image.at<uint32_t>(pe+8);image.base=image.at<uint64_t>(pe+48);image.size=image.at<uint32_t>(pe+80);
        if(!image.size||image.size>0x80000000u||image.base>UINT64_MAX-image.size||image.at<uint32_t>(pe+132)<4)throw std::runtime_error("invalid image directories");
        for(unsigned i=0;i<count;++i) {
            const size_t s=pe+24+optional+i*40;
            Section section{image.at<uint32_t>(s+12),image.at<uint32_t>(s+20),image.at<uint32_t>(s+16),image.at<uint32_t>(s+8),image.at<uint32_t>(s+36)};
            if(section.raw>image.bytes.size()||section.size>image.bytes.size()-section.raw||section.rva>image.size||std::max(section.size,section.virtualSize)>image.size-section.rva)throw std::runtime_error("invalid PE section bounds");
            for(const auto& old:image.sections) {
                if(section.size&&old.size&&uint64_t(section.raw)<uint64_t(old.raw)+old.size&&uint64_t(old.raw)<uint64_t(section.raw)+section.size)throw std::runtime_error("overlapping raw sections");
                if(uint64_t(section.rva)<uint64_t(old.rva)+std::max(old.size,old.virtualSize)&&uint64_t(old.rva)<uint64_t(section.rva)+std::max(section.size,section.virtualSize))throw std::runtime_error("overlapping virtual sections");
            }
            image.sections.push_back(section);
        }
        const auto pdata=image.at<uint32_t>(pe+160),length=image.at<uint32_t>(pe+164);
        if(!length||length%12||length/12>1000000)throw std::runtime_error("invalid exception directory");
        const auto start=image.offset(pdata,length);
        for(size_t i=0;i<length;i+=12) {
            Function f{image.at<uint32_t>(start+i),image.at<uint32_t>(start+i+4)};
            if(!f.begin||f.end<=f.begin||!image.executable(f.begin,f.end-f.begin)||(!image.functions.empty()&&f.begin<=image.functions.back().begin))throw std::runtime_error("invalid runtime function table");
            image.functions.push_back(f);
        }
        return image;
    }
};
inline std::vector<int> pattern(const std::string& text) {
    std::vector<int> result;
    for(size_t i=0;i<text.size();) {
        if(text[i]==' '){++i;continue;}
        if(i+2>text.size()||(i+2<text.size()&&text[i+2]!=' '))throw std::runtime_error("invalid signature token");
        auto hex=[](char c){return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;};
        if(text.substr(i,2)=="??")result.push_back(-1);
        else{const int a=hex(text[i]),b=hex(text[i+1]);if(a<0||b<0)throw std::runtime_error("invalid signature hex");result.push_back(a*16+b);}
        i+=2;
    }
    if(result.empty())throw std::runtime_error("empty signature");
    return result;
}
struct Match {std::vector<uint32_t> rvas;unsigned rejectedEntries=0,rawMatches=0;};
inline Match find(const Image& image,const std::vector<int>& signature) {
    size_t anchorStart=0,anchorLength=0,run=0;
    for(size_t i=0;i<signature.size();++i){run=signature[i]>=0?run+1:0;if(run>anchorLength){anchorLength=run;anchorStart=i+1-run;}}
    if(anchorLength<4)throw std::runtime_error("signature anchor too short");
    std::vector<uint8_t> anchor;for(size_t i=0;i<anchorLength;++i)anchor.push_back(static_cast<uint8_t>(signature[anchorStart+i]));
    Match result;
    for(const auto& section:image.sections) {
        if(!(section.flags&0x20000000)||section.size<signature.size())continue;
        auto begin=image.bytes.begin()+section.raw,end=begin+section.size,cursor=begin;
        while(cursor!=end) {
            const auto hit=std::search(cursor,end,anchor.begin(),anchor.end());if(hit==end)break;cursor=hit+1;
            const size_t position=static_cast<size_t>(hit-begin);
            if(position<anchorStart)continue;
            const size_t candidate=position-anchorStart;if(signature.size()>section.size-candidate)continue;
            bool same=true;for(size_t i=0;i<signature.size();++i)if(signature[i]>=0&&signature[i]!=begin[candidate+i]){same=false;break;}
            if(!same)continue;
            const auto rva=section.rva+static_cast<uint32_t>(candidate);const auto f=image.function(rva);
            if(!f||signature.size()>f->end-f->begin){++result.rejectedEntries;continue;}
            result.rvas.push_back(rva);
        }
    }
    result.rawMatches=static_cast<unsigned>(result.rvas.size());
    return result;
}
struct Resolution {
    std::map<std::string,Match> matches;
    std::map<std::string,uint32_t> addresses;
    bool unique=false,factoryAnchor=false,movieVtable=false;
    uint32_t vtableRva=0;
    bool candidate()const{return unique&&factoryAnchor&&movieVtable;}
};
inline Resolution resolve(const Image& image) {
    Resolution result;result.unique=true;
    for(const auto& symbol:symbols) {
        auto m=find(image,pattern(symbol.pattern));
        if(m.rvas.size()!=1||m.rejectedEntries)result.unique=false;
        else result.addresses[symbol.name]=m.rvas[0];
        result.matches[symbol.name]=std::move(m);
    }
    if(result.matches.at("advance").rvas.size()!=1)return result;
    unsigned tables=0;uint32_t destructor=0;
    for(const auto& s:image.sections) {
        if((s.flags&0xa0000000)||!(s.flags&0x40000000)||s.size<25*8)continue;
        for(size_t i=0;i+25*8<=s.size;i+=8) {
            if(image.at<uint64_t>(s.raw+i+24*8)!=image.base+result.addresses.at("advance"))continue;
            const auto destroy=image.at<uint64_t>(s.raw+i);
            if(destroy<image.base||destroy-image.base>UINT32_MAX||std::find(result.matches.at("destroy").rvas.begin(),result.matches.at("destroy").rvas.end(),static_cast<uint32_t>(destroy-image.base))==result.matches.at("destroy").rvas.end())continue;
            bool methods=true;
            for(size_t j=0;j<25;++j){const auto ptr=image.at<uint64_t>(s.raw+i+j*8);if(ptr<image.base||ptr-image.base>UINT32_MAX||!image.executable(static_cast<uint32_t>(ptr-image.base))){methods=false;break;}}
            if(methods){++tables;result.vtableRva=s.rva+static_cast<uint32_t>(i);destructor=static_cast<uint32_t>(destroy-image.base);}
        }
    }
    result.movieVtable=tables==1;
    if(result.movieVtable){result.matches["destroy"].rvas={destructor};result.addresses["destroy"]=destructor;}
    result.unique=true;
    for(const auto& item:result.matches)if(item.second.rvas.size()!=1||item.second.rejectedEntries)result.unique=false;
    if(!result.unique)return result;
    const auto create=result.addresses.at("create");const auto function=image.function(create);
    const auto start=image.offset(create,function->end-create);
    constexpr char anchor[]="Trying call %s.Attach(%s) on ALREADY inited object";
    for(size_t i=0;i+7<=function->end-create;++i) {
        const auto p=start+i;
        if((image.bytes[p]&0xf8)!=0x48||image.bytes[p+1]!=0x8d||(image.bytes[p+2]&0xc7)!=5)continue;
        const int64_t target=int64_t(create)+static_cast<int64_t>(i)+7+image.at<int32_t>(p+3);
        if(target<0||target>UINT32_MAX)continue;
        try{const auto offset=image.offset(static_cast<uint32_t>(target),sizeof(anchor)-1);if(!std::memcmp(image.bytes.data()+offset,anchor,sizeof(anchor)-1))result.factoryAnchor=true;}catch(const std::runtime_error&){}
    }
    return result;
}
}
