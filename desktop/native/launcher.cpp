#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <string>

static std::wstring quote(const std::wstring& value){
    std::wstring result=L"\"";size_t slashes=0;
    for(wchar_t c:value){if(c==L'\\'){++slashes;continue;}result.append(slashes*(c==L'\"'?2:1),L'\\');slashes=0;if(c==L'\"')result+=L'\\';result+=c;}
    result.append(slashes*2,L'\\');return result+L'\"';
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    wchar_t path[32768]{};if(!GetModuleFileNameW(nullptr,path,32768))return 1;
    const auto root=std::filesystem::path(path).parent_path();const auto java=root/L"runtime/bin/javaw.exe";
    wchar_t inheritedPath[32768]{};GetEnvironmentVariableW(L"PATH",inheritedPath,32768);
    const auto search=(root/L"native").wstring()+L";"+(root/L"media").wstring()+L";"+inheritedPath;
    SetEnvironmentVariableW(L"PATH",search.c_str());
    if(!std::filesystem::exists(java)){MessageBoxW(nullptr,L"The bundled Java runtime is missing. Extract the complete launcher folder and try again.",L"Mahoyo",MB_OK|MB_ICONERROR);return 1;}
    std::wstring command=quote(java.wstring())+L" -Xmx1536m "
#ifndef WOH_SAVE_SYNC
    L"-XX:+UseZGC -XX:+ZGenerational "
#endif
    +quote(L"-Djava.library.path="+(root/L"native").wstring())+L" "+quote(L"-Dmahoyo.assets="+(root/L"assets").wstring())+L" "+quote(L"-Dmahoyo.package="+root.wstring())+L" "+quote(L"-Dmahoyo.ffmpeg="+(root/L"media/ffmpeg.exe").wstring())+L" -cp "+quote((root/L"app/mahoyo.jar").wstring()+L";"+(root/L"lib/*").wstring())+
#ifdef WOH_SAVE_SYNC
    L" org.mahoyo.port.SaveSyncMain";
#else
    L" org.mahoyo.port.DesktopMain";
#endif
    int count=0;wchar_t** args=CommandLineToArgvW(GetCommandLineW(),&count);if(!args)return 1;
    for(int i=1;i<count;i++)command+=L" "+quote(args[i]);LocalFree(args);
    wchar_t local[32768]{};GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);auto logs=std::filesystem::path(local)/L"MahoyoUnofficial/logs";
    std::error_code ec;std::filesystem::create_directories(logs,ec);SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
    HANDLE log=CreateFileW((logs/
#ifdef WOH_SAVE_SYNC
    L"save-sync.log"
#else
    L"windows.log"
#endif
    ).c_str(),GENERIC_WRITE,FILE_SHARE_READ,&security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;startup.hStdOutput=startup.hStdError=log;startup.hStdInput=GetStdHandle(STD_INPUT_HANDLE);PROCESS_INFORMATION process{};
    BOOL started=CreateProcessW(java.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,root.c_str(),&startup,&process);
    if(log!=INVALID_HANDLE_VALUE)CloseHandle(log);
    if(!started){MessageBoxW(nullptr,L"Cannot start the Windows launcher. Extract the complete folder and try again.",L"Mahoyo",MB_OK|MB_ICONERROR);return 1;}
    CloseHandle(process.hThread);WaitForSingleObject(process.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hProcess);
    if(code!=0)MessageBoxW(nullptr,L"The launcher stopped with an error. Details are in %LOCALAPPDATA%\\MahoyoUnofficial\\logs\\windows.log.",L"Mahoyo",MB_OK|MB_ICONERROR);
    return int(code);
}
