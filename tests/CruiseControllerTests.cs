using System;
using System.Collections.Generic;
using SnowRunnerCruise;

internal static class CruiseControllerTests
{
    private static int checks;
    private static void Check(bool condition, string message)
    {
        checks++;
        if (!condition) throw new Exception(message);
    }

    private static int Main()
    {
        List<bool> events = new List<bool>();
        CruiseController cruise = new CruiseController(delegate(bool down) { events.Add(down); return true; });
        cruise.Toggle(false, false, false);
        cruise.Toggle(true, true, false);
        cruise.Toggle(true, false, true);
        Check(events.Count == 0 && !cruise.Active, "Never enable outside game, while braking or with modifiers");
        cruise.Toggle(true, false, false);
        Check(cruise.Active && cruise.OwnsThrottle && events.Count == 1 && events[0], "Toggle holds throttle");
        Check(cruise.SuppressThrottleUp(), "Releasing physical W preserves cruise");
        cruise.Toggle(true, false, false);
        Check(!cruise.Active && !cruise.OwnsThrottle && events.Count == 2 && !events[1], "Second toggle releases throttle");
        Check(!cruise.SuppressThrottleUp(), "Manual W works when cruise is off");
        cruise.Cancel();
        Check(events.Count == 2, "Idle cancellation never injects W-up into other applications");

        string[] reasons = { "brake", "focus loss", "menu", "shutdown" };
        foreach (string reason in reasons)
        {
            cruise.Toggle(true, false, false);
            int count = events.Count;
            cruise.Cancel();
            Check(!cruise.Active && !cruise.OwnsThrottle && events.Count == count + 1 && !events[events.Count - 1],
                reason + " cancellation releases W");
            cruise.RetryRelease();
            Check(events.Count == count + 1, "Successful release is not repeated");
        }

        bool fail = true;
        CruiseController failures = new CruiseController(delegate(bool down) { return !fail; });
        failures.Toggle(true, false, false);
        Check(!failures.Active && !failures.OwnsThrottle && failures.InputFailed, "Rejected key-down does not enable cruise");
        fail = false;
        failures.Toggle(true, false, false);
        fail = true;
        failures.Cancel();
        Check(!failures.Active && failures.OwnsThrottle && failures.InputFailed, "Failed key-up retains release obligation");
        failures.Toggle(true, false, false);
        Check(!failures.Active, "Cannot reactivate while release is pending");
        fail = false;
        failures.RetryRelease();
        Check(!failures.OwnsThrottle && !failures.InputFailed, "Transient release failure is retried");
        failures.Toggle(true, false, false);
        Check(failures.Active, "Can enable again after successful recovery");
        failures.Cancel();
        TestKeyboard();
        Console.WriteLine("PASS: " + checks + " checks");
        return 0;
    }

    private static void TestKeyboard()
    {
        List<bool> events = new List<bool>();
        CruiseController cruise = new CruiseController(delegate(bool down) { events.Add(down); return true; });
        KeyboardPolicy keys = new KeyboardPolicy(cruise, 0x75); // F6
        Check(!keys.Handle(0x75, 0x40, 0, false, false, false) && events.Count == 0, "F6 outside game is forwarded");
        Check(!keys.Handle(0x75, 0x40, 0, true, false, true), "Modified F6 is forwarded");
        Check(!keys.Handle(0x75, 0x40, 0x10, true, false, false) && events.Count == 0, "Injected F6 is ignored");
        Check(keys.Handle(0x75, 0x40, 0, true, false, false) && cruise.Active, "Game F6 enables and is swallowed");
        for (int i = 0; i < 10; i++) keys.Handle(0x75, 0x40, 0, true, false, false);
        Check(events.Count == 1 && cruise.Active, "Holding F6 enables only once");
        Check(keys.Handle(0x75, 0x40, 0x80, true, false, false), "F6 release is swallowed");
        Check(!keys.Handle(0x57, 0x11, 0x90, true, false, false), "Synthetic W-up is never suppressed");
        Check(keys.Handle(0x57, 0x11, 0x80, true, false, false) && cruise.Active, "Real W-up preserves hold");
        Check(!keys.Handle(0x53, 0x1F, 0, true, true, false) && !cruise.Active, "S cancels and is forwarded");
        Check(events.Count == 2 && !events[1], "Brake emits exactly one W release");
        keys.Handle(0x75, 0x40, 0, true, true, false);
        Check(!cruise.Active && events.Count == 2, "F6 while holding S cannot activate");
        keys.Handle(0x75, 0x40, 0x80, true, false, false);
        keys.Handle(0x75, 0x40, 0, true, false, false);
        keys.Handle(0x75, 0x40, 0x80, true, false, false);
        keys.Handle(0x75, 0x40, 0, true, false, false);
        Check(!cruise.Active && events.Count == 4, "Fresh second press cancels");
        keys.Handle(0x75, 0x40, 0x80, true, false, false);
        keys.Handle(0x75, 0x40, 0, true, false, false);
        Check(!keys.Handle(0x41, 0x1E, 0, false, false, false) && !cruise.Active, "Focus loss cancels without swallowing other input");
        keys.Handle(0x75, 0x40, 0x80, false, false, false);
        keys.Handle(0x75, 0x40, 0, true, false, false);
        keys.Handle(0x75, 0x40, 0x80, true, false, false);
        Check(!keys.Handle(0x1B, 1, 0, true, false, false) && !cruise.Active, "Escape cancels and reaches game");
        Check(!keys.Handle(0x57, 0x11, 0x80, false, false, false), "W-up outside game always reaches its target");

        cruise.Toggle(true, false, false);
        int count = events.Count;
        Check(!keys.Handle(0x57, 0x11, 0x10, true, false, false) && cruise.Active,
            "Synthetic W-down does not cancel cruise");
        Check(!keys.Handle(0x57, 0x11, 0, true, false, false) && !cruise.Active && !cruise.OwnsThrottle,
            "Physical W-down cancels cruise and transfers throttle to the driver");
        cruise.RetryRelease();
        Check(events.Count == count, "Manual takeover never injects W-up during physical W hold");
        Check(!keys.Handle(0x57, 0x11, 0, true, false, false) && events.Count == count,
            "Repeated manual W-down is forwarded without synthetic releases");
        Check(!keys.Handle(0x57, 0x11, 0x80, true, false, false), "Manual W-up reaches game after takeover");

        cruise.Toggle(true, false, false);
        count = events.Count;
        Check(!keys.Handle(0x4E, 0x31, 0x10, true, false, false) && cruise.Active,
            "Synthetic N does not cancel cruise");
        Check(!keys.Handle(0x4E, 0x31, 0, true, false, false) && !cruise.Active && !cruise.OwnsThrottle,
            "N cancels cruise and reaches the game to toggle the engine");
        Check(events.Count == count + 1 && !events[events.Count - 1], "N releases synthetic throttle once");
        Check(!keys.Handle(0x4E, 0x31, 0x80, true, false, false), "N-up reaches game");

        cruise.Toggle(true, false, false);
        Check(!keys.Handle(0xD7, 0x11, 0, true, false, false) && !cruise.Active,
            "Physical W cancels with a different keyboard layout");
        cruise.Toggle(true, false, false);
        Check(!keys.Handle(0xD8, 0x31, 0, true, false, false) && !cruise.Active,
            "Physical N cancels with a different keyboard layout");
    }
}
