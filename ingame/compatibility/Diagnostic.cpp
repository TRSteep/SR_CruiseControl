#include "CompatibilityReport.h"
#include <commdlg.h>
#include <io.h>
#include <fcntl.h>

int wmain(int argc,wchar_t** argv) {
    wchar_t chosen[32768]{},self[32768]{};
    std::wstring input,output;bool dialog=argc==1;
    for(int i=1;i<argc;++i) {
        const std::wstring arg=argv[i];
        if(arg==L"--help"){std::puts("SnowRunnerCC-Compatibility.exe SnowRunner.exe [--output report.log]\nRead-only: does not install, hook or launch the game.");return 0;}
        if(arg==L"--output"&&i+1<argc)output=argv[++i];
        else if(input.empty()&&arg.rfind(L"--",0)!=0)input=arg;
        else{std::fprintf(stderr,"Invalid arguments. Use --help.\n");return 1;}
    }
    if(dialog) {
        OPENFILENAMEW pick{};pick.lStructSize=sizeof(pick);pick.lpstrFilter=L"SnowRunner.exe\0SnowRunner.exe\0Executable\0*.exe\0";pick.lpstrFile=chosen;pick.nMaxFile=32768;
        pick.lpstrTitle=L"Select SnowRunner.exe (read-only compatibility check)";pick.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
        if(!GetOpenFileNameW(&pick))return 3;
        input=chosen;
    }
    if(input.empty()){std::fprintf(stderr,"Select an executable. Use --help.\n");return 1;}
    if(output.empty()) {
        GetModuleFileNameW(nullptr,self,32768);output=self;output.resize(output.find_last_of(L"\\/")+1);
        SYSTEMTIME time{};GetLocalTime(&time);wchar_t name[120]{};
        std::swprintf(name,120,L"SnowRunnerCC-Compatibility-%04u%02u%02u-%02u%02u%02u.log",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond);output+=name;
    }
    // Never overwrite an existing file, even if an incorrect --output path
    // points to the game executable or a previous report.
    HANDLE report=CreateFileW(output.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(report==INVALID_HANDLE_VALUE){std::fprintf(stderr,"Cannot create a new report. Existing files are never overwritten.\n");return 1;}
    const int descriptor=_open_osfhandle(reinterpret_cast<intptr_t>(report),_O_BINARY);
    if(descriptor==-1){CloseHandle(report);return 1;}
    FILE* log=_fdopen(descriptor,"wb");
    if(!log){_close(descriptor);std::fprintf(stderr,"Cannot create report stream.\n");return 1;}
    try {
        const auto result=srcompat::scanFile(input.c_str(),log);std::fclose(log);
        std::puts(result.candidate()?"Scan completed: structural candidates found; ABI is not verified.":"Scan completed: missing or ambiguous candidates; see report.");
        if(dialog)MessageBoxW(nullptr,(L"Report saved:\n"+output+L"\n\nNo hooks or game changes were made. Send this log with the game version and store.").c_str(),L"SnowRunner CC compatibility diagnostic",MB_OK|MB_ICONINFORMATION);
        return result.candidate()?0:2;
    }catch(const std::exception& error) {
        std::fprintf(log,"Result=ERROR\nReason=%s\nNo hooks or game changes.\n",error.what());std::fclose(log);
        std::fprintf(stderr,"Diagnostic failed: %s\n",error.what());
        if(dialog)MessageBoxW(nullptr,(L"The scan failed. Report:\n"+output).c_str(),L"SnowRunner CC compatibility diagnostic",MB_OK|MB_ICONERROR);
        return 1;
    }
}
