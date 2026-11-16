#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include "SignatureResolver.h"
#include "ReviewedBuilds.h"

namespace srcompat {
inline std::vector<uint8_t> readExecutable(const wchar_t* path) {
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open executable for reading");
    LARGE_INTEGER size{};
    if(!GetFileSizeEx(file,&size)||size.QuadPart<256||size.QuadPart>512*1024*1024){CloseHandle(file);throw std::runtime_error("Unsupported executable size (maximum 512 MiB)");}
    std::vector<uint8_t> bytes;
    try{bytes.resize(static_cast<size_t>(size.QuadPart));}catch(...){CloseHandle(file);throw;}
    size_t done=0;
    while(done<bytes.size()) {
        DWORD received=0;const auto block=static_cast<DWORD>(std::min<size_t>(65536,bytes.size()-done));
        if(!ReadFile(file,bytes.data()+done,block,&received,nullptr)||!received){CloseHandle(file);throw std::runtime_error("Executable read failed");}
        done+=received;
    }
    CloseHandle(file);return bytes;
}
inline std::string sha256(const std::vector<uint8_t>& bytes) {
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    bool okay=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0&&BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
    for(size_t p=0;okay&&p<bytes.size();p+=65536)okay=BCryptHashData(hash,const_cast<PUCHAR>(bytes.data()+p),static_cast<ULONG>(std::min<size_t>(65536,bytes.size()-p)),0)>=0;
    unsigned char digest[32]{};if(okay)okay=BCryptFinishHash(hash,digest,sizeof(digest),0)>=0;
    if(hash)BCryptDestroyHash(hash);
    if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
    if(!okay)throw std::runtime_error("SHA256 calculation failed");
    char hex[65]{};for(unsigned i=0;i<32;++i)std::snprintf(hex+i*2,3,"%02x",digest[i]);return hex;
}
inline constexpr char reviewedHash[]="a3cb442ec138eb2eb7e335e5c80e376766550740e49e233ac9eace035e182285";
inline void writeReport(FILE* log,const Image& image,const Resolution& result,const std::string& hash) {
    std::fprintf(log,"SnowRunner CC compatibility diagnostic 1.0 / signature revision 1\n");
    std::fprintf(log,"Mode=READ_ONLY; hooks=NONE; AS3 calls=NONE; keyboard input=NONE\n");
    std::fprintf(log,"SHA256=%s\nFileBytes=%zu ImageSize=%u PETimestamp=%u\n",hash.c_str(),image.bytes.size(),image.size,image.timestamp);
    std::fprintf(log,"ReviewedBuild=%s\n",reviewedProfile(hash)?reviewedProfile(hash):"UNKNOWN (Steam/build/store cannot be inferred from candidates)");
    for(const auto& symbol:symbols) {
        const auto& match=result.matches.at(symbol.name);
        std::fprintf(log,"Symbol=%s PatternMatches=%u Matches=%zu RejectedFunctionEntries=%u",symbol.name,match.rawMatches,match.rvas.size(),match.rejectedEntries);
        for(size_t i=0;i<std::min<size_t>(match.rvas.size(),8);++i)std::fprintf(log," RVA=0x%08x",match.rvas[i]);
        std::fprintf(log,"\n");
    }
    std::fprintf(log,"UniqueFunctions=%u FactoryStringAnchor=%u MovieVtableLink=%u MovieVtableRVA=0x%08x\n",result.unique,result.factoryAnchor,result.movieVtable,result.vtableRva);
    std::fprintf(log,"Result=%s\n",result.candidate()?"STRUCTURAL_CANDIDATES_FOUND":"INCOMPLETE_OR_AMBIGUOUS");
    std::fprintf(log,"Unknown-build ABI is NOT verified. This report does not enable or certify Steam support.\n");
    std::fprintf(log,"ReportComplete=1\n");std::fflush(log);
}
inline Resolution scanFile(const wchar_t* path,FILE* log) {
    auto bytes=readExecutable(path);const auto hash=sha256(bytes);
    std::fprintf(log,"ScannedSHA256=%s\n",hash.c_str());std::fflush(log);
    const auto image=Image::parse(std::move(bytes));const auto result=resolve(image);
    writeReport(log,image,result,hash);return result;
}
}
