#pragma once
#include <functional>
#include <atomic>

namespace autopilot {
class PhysicalThrottle {
    std::atomic<bool> held{false};
public:
    void reset(bool down) { held=down; }
    bool press() { return !held.exchange(true); }
    void release() { held=false; }
};

class ToggleEdge {
    bool held = false;
public:
    explicit ToggleEdge(bool initiallyDown = false) : held(initiallyDown) {}
    bool update(bool down) {
        const bool pressed = down && !held;
        held = down;
        return pressed;
    }
};

class CruiseState {
    std::function<bool(bool)> send;
public:
    bool active = false;
    bool ownsThrottle = false;
    bool inputFailed = false;
    explicit CruiseState(std::function<bool(bool)> sender) : send(sender) {}
    void toggle(bool focused, bool braking, bool modifiers) {
        if (active) { cancel(); return; }
        if (!focused || braking || modifiers || ownsThrottle) return;
        inputFailed = !send(true);
        active = ownsThrottle = !inputFailed;
    }
    void cancel() { active = false; retryRelease(); }
    void retryRelease() {
        if (active || !ownsThrottle) return;
        inputFailed = !send(false);
        if (!inputFailed) ownsThrottle = false;
    }
    void manualThrottle() {
        // Physical W-down takes ownership: do not send an artificial W-up.
        active = ownsThrottle = inputFailed = false;
    }
    void restoreAfterPhysicalRelease() {
        // Raw/async input can see the physical W-up despite a swallowed hook
        // event. Reassert only an existing hold, never activate from a release.
        if (!active || !ownsThrottle || send(true)) return;
        active=false;
        retryRelease();
        inputFailed=true;
    }
};

inline bool pollToggle(CruiseState& cruise, ToggleEdge& edge, bool down, bool focused, bool brake, bool modifiers) {
    // Track releases even outside the game: refocusing with a held key must
    // never re-enable throttle, nor may releasing a modifier trigger a toggle.
    if (!edge.update(down) || !focused) return false;
    cruise.toggle(true, brake, modifiers);
    return true;
}

struct CruiseInput {
    bool focused = true, drivingHud = false, brake = false, engine = false;
    bool cancel = false, manual = false, toggle = false, modifiers = false;
    bool throttleReleased = false;
};

inline bool updateCruiseInput(CruiseState& cruise, ToggleEdge& edge, const CruiseInput& input) {
    if (input.manual) cruise.manualThrottle();
    if (!input.focused || input.brake || input.engine || input.cancel) cruise.cancel();
    // Recognition is only an activation guard. A bad/missing picture must
    // neither cancel an active hold nor prevent F6 from turning it off.
    const bool toggled = pollToggle(cruise, edge, input.toggle,
        input.focused && (cruise.active || input.drivingHud),
        input.brake || input.engine || input.cancel, input.modifiers);
    if (input.throttleReleased) cruise.restoreAfterPhysicalRelease();
    cruise.retryRelease();
    return toggled;
}

class KeyboardPolicy {
    CruiseState& cruise;
    unsigned toggleKey;
    bool toggleHeld = false;
    PhysicalThrottle throttle;
public:
    KeyboardPolicy(CruiseState& state, unsigned key) : cruise(state), toggleKey(key) {}
    void resetThrottle(bool down) { throttle.reset(down); }
    bool handle(unsigned vk, unsigned scan, unsigned flags, bool game, bool brake, bool modifiers) {
        if (flags & 0x10) return false; // Ignore injected input, including our own.
        const bool up = (flags & 0x80) != 0;
        if (!game && cruise.active) cruise.cancel();
        if (vk == toggleKey) {
            if (up && toggleHeld) { toggleHeld = false; return true; }
            if (!up && toggleHeld) return true;
            if (!up && game && !modifiers) {
                toggleHeld = true;
                cruise.toggle(true, brake, false);
                return true;
            }
        }
        if (scan == 0x11 && !(flags & 1)) {
            if (up) throttle.release();
            else if (throttle.press()) cruise.manualThrottle();
        }
        if (!up && (scan == 0x1f || scan == 0x31)) cruise.cancel();
        if (!up && (vk == 0x1b || vk == 0x0d || vk == 0x70 || vk == 0x73 || scan == 0x32)) cruise.cancel();
        return up && scan == 0x11 && !(flags & 1) && cruise.active;
    }
};
}
