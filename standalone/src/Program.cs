using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

namespace SnowRunnerCruise
{
    internal static class Program
    {
        internal const string ProductName = "SnowRunner Cruise control";
        internal const string ShortName = "SnowRunner CC";
        internal static string DisplayName
        {
            get { return ProductName + " — standalone " + typeof(Program).Assembly.GetName().Version.ToString(3); }
        }

        [STAThread]
        private static int Main(string[] args)
        {
            bool created;
            using (Mutex mutex = new Mutex(true, "Local\\SnowRunnerCruiseControl", out created))
            {
                if (!created)
                {
                    MessageBox.Show("Утилита уже запущена. Ищите значок в системном трее.", DisplayName);
                    return 0;
                }
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                try
                {
                    using (CruiseApp app = new CruiseApp())
                    {
                        if (args.Length == 1 && args[0] == "--smoke-test") return 0;
                        Application.Run(app);
                    }
                    return 0;
                }
                catch (Exception ex)
                {
                    MessageBox.Show(ex.Message, DisplayName, MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return 1;
                }
            }
        }
    }

    internal sealed class CruiseApp : ApplicationContext
    {
        private readonly CruiseController cruise = new CruiseController(Native.SendThrottle);
        private readonly Native.KeyboardProc callback;
        private readonly Keys toggleKey;
        private readonly KeyboardPolicy keyboard;
        private readonly NotifyIcon tray;
        private readonly Icon idleIcon = LoadIcon("Off");
        private readonly Icon activeIcon = LoadIcon("On");
        private readonly Icon errorIcon = LoadIcon("Error");
        private readonly ToolStripMenuItem status;
        private readonly System.Windows.Forms.Timer timer;
        private IntPtr hook;
        private bool disposed;
        private string lastStatus;

        internal CruiseApp()
        {
            toggleKey = ReadToggleKey();
            keyboard = new KeyboardPolicy(cruise, (uint)toggleKey);
            callback = KeyboardHook;
            hook = Native.SetWindowsHookEx(13, callback, Native.GetModuleHandle(null), 0);
            if (hook == IntPtr.Zero)
                throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error(), "Не удалось установить обработчик клавиатуры.");
            status = new ToolStripMenuItem { Enabled = false };
            ContextMenuStrip menu = new ContextMenuStrip();
            menu.Items.Add(status);
            menu.Items.Add("Отключить круиз", null, delegate { cruise.Cancel(); UpdateStatus(); });
            menu.Items.Add("Выход", null, delegate { ExitThread(); });
            tray = new NotifyIcon { Icon = idleIcon, ContextMenuStrip = menu, Visible = true };
            timer = new System.Windows.Forms.Timer { Interval = 25 };
            timer.Tick += delegate
            {
                if (cruise.Active && (!GameFocused() || Native.IsDown((int)Keys.S))) cruise.Cancel();
                cruise.RetryRelease();
                UpdateStatus();
            };
            timer.Start();
            Application.ApplicationExit += OnApplicationExit;
            Microsoft.Win32.SystemEvents.SessionEnding += OnSessionEnding;
            UpdateStatus();
            tray.ShowBalloonTip(3500, Program.DisplayName,
                toggleKey + " — включить/выключить; W, S, N — отключить. Работает только в окне игры.", ToolTipIcon.Info);
        }

        private static Icon LoadIcon(string state)
        {
            using (Stream stream = typeof(CruiseApp).Assembly.GetManifestResourceStream("SnowRunnerCruise.Icons." + state + ".ico"))
            {
                if (stream == null) throw new InvalidDataException("Missing embedded application icon: " + state);
                using (Icon icon = new Icon(stream, 32, 32)) return (Icon)icon.Clone();
            }
        }

        private static Keys ReadToggleKey()
        {
            string file = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "CruiseControl.ini");
            if (!File.Exists(file)) return Keys.F6;
            foreach (string raw in File.ReadAllLines(file))
            {
                string line = raw.Trim();
                if (line.Length == 0 || line.StartsWith(";") || line.StartsWith("#") || line.StartsWith("[")) continue;
                string[] pair = line.Split(new char[] { '=' }, 2);
                if (pair.Length != 2 || !pair[0].Trim().Equals("ToggleKey", StringComparison.OrdinalIgnoreCase))
                    throw new InvalidDataException("Неизвестная строка в CruiseControl.ini: " + line);
                Keys key;
                if (!Enum.TryParse<Keys>(pair[1].Trim(), true, out key) || key < Keys.F2 || key > Keys.F12 || key == Keys.F4)
                    throw new InvalidDataException("ToggleKey: укажите F2–F12, кроме F4 (профиль). Рекомендуется F6.");
                return key;
            }
            return Keys.F6;
        }

        private static bool GameFocused()
        {
            uint id;
            IntPtr window = Native.GetForegroundWindow();
            if (window == IntPtr.Zero) return false;
            Native.GetWindowThreadProcessId(window, out id);
            if (id == 0) return false;
            try
            {
                using (Process process = Process.GetProcessById((int)id))
                    return process.ProcessName.Equals("SnowRunner", StringComparison.OrdinalIgnoreCase);
            }
            catch (ArgumentException) { return false; }
            catch (System.ComponentModel.Win32Exception) { return false; }
            catch (InvalidOperationException) { return false; }
        }

        private static bool ModifiersDown()
        {
            return Native.IsDown(0x10) || Native.IsDown(0x11) || Native.IsDown(0x12)
                || Native.IsDown(0x5B) || Native.IsDown(0x5C);
        }

        private IntPtr KeyboardHook(int code, IntPtr message, IntPtr data)
        {
            if (code < 0) return Native.CallNextHookEx(hook, code, message, data);
            Native.KeyboardData key = (Native.KeyboardData)Marshal.PtrToStructure(data, typeof(Native.KeyboardData));
            // Ignore injected input, including our own W events.
            if ((key.flags & 0x10) != 0) return Native.CallNextHookEx(hook, code, message, data);
            if (keyboard.Handle(key.vkCode, key.scanCode, key.flags, GameFocused(),
                Native.IsDown((int)Keys.S), ModifiersDown()))
                return new IntPtr(1);
            return Native.CallNextHookEx(hook, code, message, data);
        }

        private void UpdateStatus()
        {
            string value = cruise.InputFailed ? "Ошибка ввода: проверьте права запуска"
                : (cruise.Active ? "Круиз ВКЛ" : "Круиз ВЫКЛ") + " (" + toggleKey + ")";
            if (value == lastStatus) return;
            lastStatus = value;
            status.Text = value;
            tray.Text = Program.ShortName + " · " + value;
            tray.Icon = cruise.InputFailed ? errorIcon : cruise.Active ? activeIcon : idleIcon;
        }

        private void OnApplicationExit(object sender, EventArgs e) { cruise.Cancel(); }
        private void OnSessionEnding(object sender, Microsoft.Win32.SessionEndingEventArgs e) { cruise.Cancel(); }
        protected override void ExitThreadCore() { cruise.Cancel(); base.ExitThreadCore(); }

        protected override void Dispose(bool disposing)
        {
            if (!disposed)
            {
                disposed = true;
                if (timer != null) timer.Stop();
                cruise.Cancel();
                if (hook != IntPtr.Zero) { Native.UnhookWindowsHookEx(hook); hook = IntPtr.Zero; }
                Application.ApplicationExit -= OnApplicationExit;
                Microsoft.Win32.SystemEvents.SessionEnding -= OnSessionEnding;
                if (tray != null) { tray.Visible = false; tray.ContextMenuStrip.Dispose(); tray.Dispose(); }
                idleIcon.Dispose();
                activeIcon.Dispose();
                errorIcon.Dispose();
                if (timer != null) timer.Dispose();
            }
            base.Dispose(disposing);
        }
    }
}
