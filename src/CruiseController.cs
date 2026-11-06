using System;

namespace SnowRunnerCruise
{
    // Input-independent state machine; the sender reports whether Windows accepted the event.
    public sealed class CruiseController
    {
        private readonly Func<bool, bool> sendThrottle;
        public bool Active { get; private set; }
        public bool OwnsThrottle { get; private set; }
        public bool InputFailed { get; private set; }

        public CruiseController(Func<bool, bool> sender) { sendThrottle = sender; }

        public void Toggle(bool gameFocused, bool brakeDown, bool modifiersDown)
        {
            if (Active) { Cancel(); return; }
            if (!gameFocused || brakeDown || modifiersDown || OwnsThrottle) return;
            InputFailed = !sendThrottle(true);
            Active = OwnsThrottle = !InputFailed;
        }

        public void Cancel()
        {
            Active = false;
            RetryRelease();
        }

        public void TakeManualThrottle()
        {
            // The physical W-down is forwarded to the game. Its eventual W-up
            // releases the throttle; injecting W-up here would interrupt manual gas.
            Active = OwnsThrottle = InputFailed = false;
        }

        public void RetryRelease()
        {
            if (Active || !OwnsThrottle) return;
            InputFailed = !sendThrottle(false);
            if (!InputFailed) OwnsThrottle = false;
        }

        // A real W-up must not undo the artificial hold while cruise is enabled.
        public bool SuppressThrottleUp() { return Active; }
    }
}
