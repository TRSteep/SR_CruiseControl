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
    std::printf("PASS: %d native state/input checks\n", checks);
}
