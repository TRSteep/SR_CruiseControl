#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <string>
#include <cstdio>
#include "../../src/CruiseState.h"

namespace nativecontrol {
inline std::atomic<int> state{0};
inline std::atomic<bool> owns{false},stop{false},manual{false},cancel{false},hudVisible{false};
inline std::atomic<ULONGLONG> hudStamp{0};
inline unsigned toggleKey=VK_F6;
inline constexpr ULONG_PTR inputTag=0x53524e41;
inline HHOOK hook=nullptr;
inline std::atomic<WNDPROC> previous{nullptr};
inline HWND gameWindow=nullptr;
inline void (*notify)(int,const char*)=nullptr;
inline bool down(unsigned key){return (GetAsyncKeyState(static_cast<int>(key))&0x8000)!=0;}
inline bool focused(){DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);return pid==GetCurrentProcessId();}
inline bool modifiers(){return down(VK_CONTROL)||down(VK_SHIFT)||down(VK_MENU)||down(VK_LWIN)||down(VK_RWIN);}
inline bool available(){const auto now=GetTickCount64(),stamp=hudStamp.load();return hudVisible.load()&&stamp&&now>=stamp&&now-stamp<=500;}
inline bool send(bool held){INPUT input{};input.type=INPUT_KEYBOARD;input.ki.wScan=0x11;input.ki.dwFlags=KEYEVENTF_SCANCODE|(held?0:KEYEVENTF_KEYUP);input.ki.dwExtraInfo=inputTag;return SendInput(1,&input,sizeof(input))==1;}
inline autopilot::CruiseState cruise(send);
inline autopilot::KeyboardPolicy policy(cruise,0);
inline void publish(const char* reason){owns=cruise.ownsThrottle;const int next=cruise.inputFailed?2:cruise.active?1:0;const int old=state.exchange(next);if(old!=next&&notify)notify(next,reason);}
inline LRESULT CALLBACK keyboard(int code,WPARAM message,LPARAM data){
    if(code==HC_ACTION){const auto& key=*reinterpret_cast<KBDLLHOOKSTRUCT*>(data);const bool swallow=policy.handle(key.vkCode,key.scanCode,key.flags,focused(),down(MapVirtualKeyW(0x1f,MAPVK_VSC_TO_VK_EX)),modifiers());publish("physical input");if(swallow)return 1;}
    return CallNextHookEx(hook,code,message,data);
}
inline LRESULT CALLBACK windowProc(HWND window,UINT message,WPARAM vk,LPARAM data){
    if((message==WM_KEYDOWN||message==WM_SYSKEYDOWN)&&static_cast<ULONG_PTR>(GetMessageExtraInfo())!=inputTag){
        const unsigned scan=(static_cast<ULONG_PTR>(data)>>16)&0xff;
        const bool extended=(static_cast<ULONG_PTR>(data)&(1u<<24))!=0;
        if(scan==0x11&&!extended)manual=true;
        if(scan==0x1f||scan==0x31||scan==0x32||vk==VK_ESCAPE||vk==VK_RETURN||vk==VK_F1||vk==VK_F4)cancel=true;
    }
    if((message==WM_KEYDOWN||message==WM_KEYUP||message==WM_SYSKEYDOWN||message==WM_SYSKEYUP)&&vk==toggleKey&&!modifiers()&&(state.load()==1||available()))return 0;
    const auto old=previous.load();return old?CallWindowProcW(old,window,message,vk,data):DefWindowProcW(window,message,vk,data);
}
inline BOOL CALLBACK findWindow(HWND window,LPARAM){DWORD pid=0;GetWindowThreadProcessId(window,&pid);if(pid==GetCurrentProcessId()&&IsWindowVisible(window)&&GetWindow(window,GW_OWNER)==nullptr){gameWindow=window;return FALSE;}return TRUE;}
inline DWORD WINAPI inputThread(void* value){
    const auto module=static_cast<HMODULE>(value);
    hook=SetWindowsHookExW(WH_KEYBOARD_LL,keyboard,module,0);
    autopilot::ToggleEdge edge(down(toggleKey));bool lastToggle=down(toggleKey);MSG message{};ULONGLONG lastWindow=0;
    while(!stop.load()){
        MsgWaitForMultipleObjects(0,nullptr,FALSE,8,QS_ALLINPUT);
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        const auto now=GetTickCount64();
        if(gameWindow&&!IsWindow(gameWindow)){gameWindow=nullptr;previous=nullptr;}
        if(!previous.load()&&now-lastWindow>1000){lastWindow=now;EnumWindows(findWindow,0);if(gameWindow){SetLastError(0);const auto old=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(gameWindow,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(windowProc)));if(old)previous=old;}}
        autopilot::CruiseInput input;input.focused=focused();input.drivingHud=available();
        input.brake=down(MapVirtualKeyW(0x1f,MAPVK_VSC_TO_VK_EX));input.engine=down(MapVirtualKeyW(0x31,MAPVK_VSC_TO_VK_EX));
        input.cancel=cancel.exchange(false)||down(VK_ESCAPE)||down(VK_RETURN)||down(VK_F1)||down(VK_F4);
        input.manual=manual.exchange(false);input.toggle=down(toggleKey);input.modifiers=modifiers();
        const bool pressed=input.toggle&&!lastToggle;lastToggle=input.toggle;const auto oldState=state.load();
        autopilot::updateCruiseInput(cruise,edge,input);publish("poll");
        if(pressed&&input.focused&&oldState==state.load()&&notify)
            notify(state.load(),input.drivingHud?"F-key blocked by brake/modifier/release":"F-key blocked: native HUD unavailable");
    }
    cruise.cancel();publish("shutdown");if(hook)UnhookWindowsHookEx(hook);return 0;
}
inline void shutdown(){stop=true;if(owns.load())send(false);}
inline std::string label(){return "АВТОПИЛОТ";}
inline std::string keyLabel(){char key[16]{};std::snprintf(key,sizeof(key),"F%u",toggleKey-VK_F1+1);return key;}
}
