using System;
using System.Runtime.InteropServices;

namespace SnowRunnerCruise
{
    internal static class Native
    {
        internal delegate IntPtr KeyboardProc(int code, IntPtr message, IntPtr data);
        [StructLayout(LayoutKind.Sequential)]
        internal struct KeyboardData
        {
            internal uint vkCode, scanCode, flags, time;
            internal UIntPtr extraInfo;
        }
        [StructLayout(LayoutKind.Sequential)]
        internal struct Input { internal uint type; internal InputUnion data; }
        [StructLayout(LayoutKind.Explicit)]
        internal struct InputUnion
        {
            [FieldOffset(0)] internal KeyboardInput keyboard;
            [FieldOffset(0)] internal MouseInput mouse;
        }
        [StructLayout(LayoutKind.Sequential)]
        internal struct KeyboardInput
        {
            internal ushort vk, scan;
            internal uint flags, time;
            internal UIntPtr extraInfo;
        }
        // Required for the native INPUT union to have the correct size on x64 and x86.
        [StructLayout(LayoutKind.Sequential)]
        internal struct MouseInput
        {
            internal int x, y;
            internal uint mouseData, flags, time;
            internal UIntPtr extraInfo;
        }
        [DllImport("user32.dll", SetLastError = true)]
        internal static extern IntPtr SetWindowsHookEx(int id, KeyboardProc callback, IntPtr module, uint thread);
        [DllImport("user32.dll")] internal static extern bool UnhookWindowsHookEx(IntPtr hook);
        [DllImport("user32.dll")] internal static extern IntPtr CallNextHookEx(IntPtr hook, int code, IntPtr message, IntPtr data);
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode)] internal static extern IntPtr GetModuleHandle(string name);
        [DllImport("user32.dll")] internal static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll")] internal static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
        [DllImport("user32.dll")] internal static extern short GetAsyncKeyState(int key);
        [DllImport("user32.dll", SetLastError = true)] internal static extern uint SendInput(uint count, Input[] inputs, int size);

        internal static bool IsDown(int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; }

        internal static bool SendThrottle(bool down)
        {
            Input input = new Input();
            input.type = 1; // INPUT_KEYBOARD
            input.data.keyboard.scan = 0x11; // Physical W, independent of keyboard layout.
            input.data.keyboard.flags = 0x0008u | (down ? 0u : 0x0002u);
            return SendInput(1, new Input[] { input }, Marshal.SizeOf(typeof(Input))) == 1;
        }
    }
}
