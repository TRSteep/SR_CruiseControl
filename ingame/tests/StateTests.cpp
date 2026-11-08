#include "../src/CruiseState.h"
#include <vector>
#include <stdexcept>
#include <cstdio>

int main() {
    int checks = 0;
    auto check = [&](bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); };
    std::vector<bool> sent;
    bool accept = true;
    autopilot::CruiseState state([&](bool down) { if (accept) sent.push_back(down); return accept; });
    autopilot::KeyboardPolicy keys(state, 0x75);
    auto key = [&](unsigned vk, unsigned scan, unsigned flags = 0, bool focus = true, bool brake = false, bool modifiers = false) {
        return keys.handle(vk, scan, flags, focus, brake, modifiers);
    };
    auto toggle = [&] { key(0x75, 0x40); key(0x75, 0x40, 0x80); };
    check(!key(0x75, 0x40, 0, false) && sent.empty(), "No activation outside game");
    check(!key(0x75, 0x40, 0, true, false, true), "Modified toggle forwarded");
    toggle(); check(state.active && state.ownsThrottle && sent.size() == 1 && sent.back(), "Toggle holds W");
    check(!key(0x57, 0x11, 0x10) && state.active, "Own injected W never cancels");
    check(key(0x57, 0x11, 0x80), "Old physical W-up preserves hold");
    auto count = sent.size();
    check(!key(0x57, 0x11) && !state.active && !state.ownsThrottle, "W takes manual control and reaches game");
    state.retryRelease();
    check(sent.size() == count, "No injected W-up interrupts manual hold");
    check(!key(0x57, 0x11, 0x80), "Manual W-up forwarded");
    toggle(); count = sent.size();
    check(!key(0x4e, 0x31) && !state.active && !state.ownsThrottle, "N cancels and reaches game");
    check(sent.size() == count + 1 && !sent.back(), "N releases synthetic throttle once");
    key(0x4e, 0x31); state.retryRelease();
    check(sent.size() == count + 1, "N repeat never repeats release");
    toggle(); check(!key(0x53, 0x1f) && !state.active, "Brake cancels");
    key(0x75, 0x40, 0, true, true); key(0x75, 0x40, 0x80);
    check(!state.active, "Braking blocks activation");
    toggle();
    for (int i = 0; i < 10; ++i) key(0x75, 0x40);
    check(!state.active, "Next fresh toggle cancels once");
    key(0x75, 0x40, 0x80); toggle();
    check(state.active, "Fresh press enables");
    check(!key(0x41, 0x1e, 0, false) && !state.active, "Focus loss cancels without swallowing input");
    for (unsigned vk : {0x1bu, 0x0du, 0x70u, 0x73u}) {
        toggle(); check(!key(vk, 0) && !state.active, "Menu key cancels");
    }
    toggle(); check(!key(0xd7, 0x11) && !state.active, "Physical W works with another keyboard layout");
    key(0xd7, 0x11, 0x80);
    toggle(); check(!key(0xd8, 0x31) && !state.active, "Physical N works with another keyboard layout");
    accept = false; toggle();
    check(!state.active && state.inputFailed && !state.ownsThrottle, "Rejected down cannot activate");
    accept = true; toggle(); accept = false; state.cancel();
    check(!state.active && state.ownsThrottle && state.inputFailed, "Failed up retains release obligation");
    toggle(); check(!state.active, "Cannot activate with pending release");
    accept = true; state.retryRelease();
    check(!state.ownsThrottle && !state.inputFailed, "Release retried successfully");
    toggle(); accept = false; state.cancel(); key(0x57, 0x11); accept = true; count = sent.size(); state.retryRelease();
    check(!state.ownsThrottle && sent.size() == count, "Manual takeover cancels pending synthetic release");
    state.cancel();
    autopilot::KeyboardPolicy passiveToggle(state, 0);
    autopilot::ToggleEdge edge;
    check(!passiveToggle.handle(0x75, 0x40, 0, true, false, false), "Native hook forwards F6 so async state can update");
    auto sample = [&](bool down, bool focused = true, bool braking = false, bool modifiers = false) {
        return autopilot::pollToggle(state, edge, down, focused, braking, modifiers);
    };
    check(sample(true) && state.active, "Polling enables without a keyboard hook callback");
    count = sent.size();
    for (int i = 0; i < 100; ++i) sample(true);
    check(state.active && sent.size() == count, "Holding polled F6 does not oscillate");
    sample(false);
    check(sample(true) && !state.active && !sent.back(), "Second polled press releases gas");
    sample(false); sample(true, false);
    check(!sample(true) && !state.active, "Refocusing with held F6 never activates");
    sample(false); sample(true, true, false, true); sample(true);
    check(!state.active, "Releasing modifier while holding F6 never activates");
    sample(false); sample(true, true, true); sample(true);
    check(!state.active, "Releasing brake while holding F6 never activates");
    sample(false); sample(true);
    check(state.active, "Fresh press works after blocked activation");
    state.manualThrottle(); sample(false);
    check(!state.active && !state.ownsThrottle, "Window fallback can hand gas to the driver");
    state.cancel();
    autopilot::ToggleEdge controlEdge;
    autopilot::CruiseInput input;
    input.drivingHud = true; input.toggle = true;
    autopilot::updateCruiseInput(state, controlEdge, input);
    check(state.active, "Production input path activates after confirmed HUD");
    input.toggle = false; input.drivingHud = false; count = sent.size();
    for (int i = 0; i < 300; ++i) autopilot::updateCruiseInput(state, controlEdge, input);
    check(state.active && state.ownsThrottle && sent.size() == count,
        "Any length of recognition failure cannot cancel active cruise or inject W-up");
    input.toggle = true; autopilot::updateCruiseInput(state, controlEdge, input);
    check(!state.active && !state.ownsThrottle, "F6 can always disable active cruise without recognized HUD");
    input.toggle = false; autopilot::updateCruiseInput(state, controlEdge, input);
    input.toggle = true; autopilot::updateCruiseInput(state, controlEdge, input);
    check(!state.active, "Missing HUD still blocks new activation");
    for (int reason = 0; reason < 5; ++reason) {
        input = {}; input.drivingHud = true;
        autopilot::updateCruiseInput(state, controlEdge, input);
        input.toggle = true; autopilot::updateCruiseInput(state, controlEdge, input);
        check(state.active, "Fresh activation before explicit cancellation");
        input.toggle = false; input.drivingHud = false;
        if (reason == 0) input.manual = true;
        if (reason == 1) input.brake = true;
        if (reason == 2) input.engine = true;
        if (reason == 3) input.cancel = true;
        if (reason == 4) input.focused = false;
        autopilot::updateCruiseInput(state, controlEdge, input);
        check(!state.active && !state.ownsThrottle, "W/S/N/menu/focus still cancel while HUD is missing");
    }
    // Regression: physical W -> F6 -> W autorepeats -> physical W-up.
    // Model a raw-input release even if the low-level hook swallowed W-up.
    bool gameThrottle=false,acceptDown=true,acceptUp=true;
    unsigned handoffDowns=0,handoffUps=0;
    autopilot::CruiseState handoff([&](bool held) {
        if(held){++handoffDowns;if(!acceptDown)return false;}
        else{++handoffUps;if(!acceptUp)return false;}
        gameThrottle=held;return true;
    });
    autopilot::KeyboardPolicy handoffKeys(handoff,0);
    autopilot::ToggleEdge handoffEdge;
    autopilot::CruiseInput h;h.drivingHud=true;
    auto pollHandoff=[&]{autopilot::updateCruiseInput(handoff,handoffEdge,h);};
    handoffKeys.handle(0x57,0x11,0,true,false,false);gameThrottle=true;
    h.toggle=true;pollHandoff();
    check(handoff.active&&handoff.ownsThrottle&&gameThrottle,"F6 takes over an already-held physical W");
    for(unsigned i=0;i<100;++i) {
        check(!handoffKeys.handle(0x57,0x11,0,true,false,false)&&handoff.active,
            "Autorepeat from the original W hold cannot cancel cruise");
    }
    h.toggle=false;pollHandoff();
    check(handoffKeys.handle(0x57,0x11,0x80,true,false,false),"Physical release is swallowed while cruise owns throttle");
    gameThrottle=false;const auto beforeRestore=handoffDowns;
    h.throttleReleased=true;pollHandoff();h.throttleReleased=false;
    check(handoff.active&&handoff.ownsThrottle&&!handoff.inputFailed&&gameThrottle&&handoffDowns==beforeRestore+1,
        "After releasing W, both actual gas and active HUD state remain enabled");
    for(unsigned i=0;i<100;++i)pollHandoff();
    check(handoffDowns==beforeRestore+1,"An old release does not continually inject W-down");
    check(!handoffKeys.handle(0x57,0x11,0,true,false,false)&&!handoff.active&&!handoff.ownsThrottle,
        "The next distinct W press still takes manual control");
    gameThrottle=true;const auto beforeManualUp=handoffUps;
    h.throttleReleased=true;pollHandoff();h.throttleReleased=false;
    check(gameThrottle&&handoffUps==beforeManualUp,"A queued old release cannot interrupt manual throttle");
    check(!handoffKeys.handle(0x57,0x11,0x80,true,false,false),"Manual release reaches game after takeover");gameThrottle=false;
    h.toggle=true;pollHandoff();
    check(handoff.active&&gameThrottle,"A single fresh F6 enables cruise after manual takeover");
    h.toggle=false;pollHandoff();h.toggle=true;pollHandoff();
    check(!handoff.active&&!gameThrottle,"Next F6 still disables after the handoff");

    // The fallback window route uses the same physical-edge tracker, and a
    // swallowed hook release must reset it before the next hardware press.
    autopilot::PhysicalThrottle windowW;
    check(windowW.press()&&!windowW.press(),"Window route distinguishes fresh W from autorepeat");
    windowW.release();check(windowW.press(),"Hook W-up also allows the next window W-down");
    windowW.reset(true);check(!windowW.press(),"A physical W held when hooking starts is not a fresh press");
    handoffKeys.resetThrottle(true);h={};h.drivingHud=true;pollHandoff();h.toggle=true;pollHandoff();
    check(handoff.active&&!handoffKeys.handle(0x57,0x11,0,true,false,false)&&handoff.active,
        "Starting the hook with W already held does not misclassify its first repeat");
    handoffKeys.handle(0x57,0x11,0x80,true,false,false);
    for(int reason=0;reason<5;++reason) {
        handoff.cancel();h={};h.drivingHud=true;pollHandoff();h.toggle=true;pollHandoff();
        check(handoff.active,"Activate before queued-release cancellation test");
        h.toggle=false;pollHandoff();h.throttleReleased=true;
        if(reason==0)h.brake=true;
        if(reason==1)h.engine=true;
        if(reason==2)h.focused=false;
        if(reason==3)h.manual=true;
        if(reason==4)h.toggle=true;
        const auto downs=handoffDowns;pollHandoff();
        check(!handoff.active&&handoffDowns==downs,"Brake/N/focus/new W/F6 take priority over deferred throttle restore");
    }
    h={};h.drivingHud=true;pollHandoff();h.toggle=true;pollHandoff();h.toggle=false;pollHandoff();
    acceptDown=false;h.throttleReleased=true;pollHandoff();h.throttleReleased=false;
    check(!handoff.active&&!handoff.ownsThrottle&&handoff.inputFailed&&!gameThrottle,
        "Rejected restored W-down clears active state and releases gas instead of leaving a green HUD");
    acceptDown=true;h.toggle=true;pollHandoff();h.toggle=false;pollHandoff();
    acceptDown=acceptUp=false;h.throttleReleased=true;pollHandoff();
    check(!handoff.active&&handoff.ownsThrottle&&handoff.inputFailed,"Failed restore/release retains cleanup obligation");
    acceptDown=acceptUp=true;h.throttleReleased=false;pollHandoff();
    check(!handoff.active&&!handoff.ownsThrottle&&!gameThrottle,"Deferred cleanup never reactivates cruise");
    std::printf("PASS: %d native state/input checks\n", checks);
}
