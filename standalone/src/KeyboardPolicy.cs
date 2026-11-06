namespace SnowRunnerCruise
{
    // Returns true only for events that must be swallowed by the Windows hook.
    public sealed class KeyboardPolicy
    {
        private readonly CruiseController cruise;
        private readonly uint toggleKey;
        private bool toggleHeld;

        public KeyboardPolicy(CruiseController controller, uint key)
        {
            cruise = controller;
            toggleKey = key;
        }

        public bool Handle(uint vk, uint scan, uint flags, bool game, bool brakeDown, bool modifiers)
        {
            if ((flags & 0x10) != 0) return false;
            bool up = (flags & 0x80) != 0;
            if (!game && cruise.Active) cruise.Cancel();
            if (vk == toggleKey)
            {
                if (up && toggleHeld) { toggleHeld = false; return true; }
                if (!up && toggleHeld) return true;
                if (!up && game && !modifiers)
                {
                    toggleHeld = true;
                    cruise.Toggle(true, brakeDown, false);
                    return true;
                }
            }
            if (!up && scan == 0x11 && (flags & 1) == 0) cruise.TakeManualThrottle();
            if (!up && (scan == 0x1F || scan == 0x31)) cruise.Cancel(); // S and N must still reach the game.
            if (!up && (vk == 0x1B || vk == 0x0D || vk == 0x70 || vk == 0x73 || scan == 0x32))
                cruise.Cancel(); // Esc, Enter, F1, F4, physical M.
            return up && scan == 0x11 && (flags & 1) == 0 && cruise.SuppressThrottleUp();
        }
    }
}
