#define SCALEFORM_PROBE_TEST
#include "../experimental/scaleform/Probe.cpp"

int wmain(int argc,wchar_t** argv) {
    if(argc!=2)return 1;
    auto bytes=srcompat::readExecutable(argv[1]);
    if(!srcompat::reviewedBuild(srcompat::sha256(bytes))){std::fprintf(stderr,"Binding test requires a reviewed EXE.\n");return 1;}
    const auto file=srcompat::Image::parse(std::move(bytes));const auto resolution=srcompat::resolve(file);
    if(!resolution.candidate())return 1;
    // Private, non-executable copy. Never attach to or change the game.
    std::vector<uint8_t> memory(file.size);
    for(const auto& s:file.sections)std::memcpy(memory.data()+s.rva,file.bytes.data()+s.raw,s.size);
    auto rebase=[&](size_t index,const char* name){const uintptr_t ptr=reinterpret_cast<uintptr_t>(memory.data()+resolution.addresses.at(name));std::memcpy(memory.data()+resolution.vtableRva+index*8,&ptr,8);};
    rebase(0,"destroy");rebase(24,"advance");
    unsigned checks=0;auto check=[&](bool okay){++checks;if(!okay){std::fprintf(stderr,"FAIL resolved binding %u\n",checks);std::exit(1);}};
    check(probe::bindResolvedUi(memory.data(),resolution));
    check(reinterpret_cast<uintptr_t>(probe::ui.get)==reinterpret_cast<uintptr_t>(memory.data()+resolution.addresses.at("get")));
    check(reinterpret_cast<uintptr_t>(probe::ui.retain)==reinterpret_cast<uintptr_t>(memory.data()+resolution.addresses.at("retain")));
    const auto get=resolution.addresses.at("get");memory[get]^=1;
    check(!probe::bindResolvedUi(memory.data(),resolution));memory[get]^=1;
    const uintptr_t wrong=0;std::memcpy(memory.data()+resolution.vtableRva+24*8,&wrong,8);
    check(!probe::bindResolvedUi(memory.data(),resolution));rebase(24,"advance");
    auto missing=resolution;missing.unique=false;check(!probe::bindResolvedUi(memory.data(),missing));
    check(probe::bindResolvedUi(memory.data(),resolution));
    std::printf("PASS: %u private-image relocated binding/patch rejection checks; no hooks or AS3 calls\n",checks);
    return 0;
}
