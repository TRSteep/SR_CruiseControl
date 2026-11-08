using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using Microsoft.Win32;

namespace SnowRunnerCC.Setup
{
    internal static class Program
    {
        internal static byte[] Resource(string name)
        {
            using (Stream input = Assembly.GetExecutingAssembly().GetManifestResourceStream("SnowRunnerCC." + name))
            {
                if (input == null) throw new InvalidDataException("Missing installer resource: " + name);
                using (var output = new MemoryStream()) { input.CopyTo(output); return output.ToArray(); }
            }
        }
        internal static Package BundledPackage()
        {
            var package = new Package { Version = BuildInfo.Version, SupportedExe = BuildInfo.SupportedExe };
            foreach (string name in new string[] { Package.Asi, Package.Ini, Package.Loader })
            { package.Files[name] = Resource(name); package.Hashes[name] = BuildInfo.Hashes[name]; }
            package.Verify(); return package;
        }
        [STAThread]
        private static int Main(string[] args)
        {
            try
            {
                BundledPackage();
                if (args.Length == 1 && args[0] == "--verify-package") return 0;
                Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
                string folder = args.Length == 2 && args[0] == "--folder" ? args[1] : "";
                using (var form = new SetupForm(folder)) Application.Run(form);
                return 0;
            }
            catch (Exception error) { MessageBox.Show(error.Message, "SnowRunner CC Setup", MessageBoxButtons.OK, MessageBoxIcon.Error); return 1; }
        }
    }

    internal sealed class SetupForm : Form
    {
        private readonly TextBox folder = new TextBox(), log = new TextBox();
        private readonly Button browse = new Button(), install = new Button(), uninstall = new Button();
        private readonly BackgroundWorker worker = new BackgroundWorker { WorkerReportsProgress = true };
        private static readonly bool Russian = CultureInfo.CurrentUICulture.TwoLetterISOLanguageName == "ru";
        private static string T(string en, string ru) { return Russian ? ru : en; }
        internal SetupForm(string initialFolder)
        {
            Text = "SnowRunner CC — HUD " + BuildInfo.Version + " Setup";
            ClientSize = new Size(690, 485); MinimumSize = Size; MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen; Font = new Font("Segoe UI", 10);
            using (var stream = new MemoryStream(Program.Resource("app.ico")))
            using (var icon = new Icon(stream)) Icon = (Icon)icon.Clone();
            var title = new Label { Text = "SnowRunner Cruise control", Font = new Font(Font.FontFamily, 19, FontStyle.Bold), AutoSize = true, Location = new Point(22, 18) };
            var description = new Label { Text = T("Native HUD " + BuildInfo.Version + " · Windows x64 · Epic Games / SnowRunner 44.1\nInstall outside the game. Afterwards the HUD loads with SnowRunner.",
                "Нативный HUD " + BuildInfo.Version + " · Windows x64 · Epic Games / SnowRunner 44.1\nУстановка вне игры. После неё HUD загружается вместе со SnowRunner."), Location = new Point(24, 62), Size = new Size(645, 54) };
            var hint = new Label { Text = T("Game folder (or the folder containing SnowRunner.exe):", "Папка игры или папка с SnowRunner.exe:"), Location = new Point(24, 130), AutoSize = true };
            folder.SetBounds(24, 158, 532, 29); folder.Text = String.IsNullOrEmpty(initialFolder) ? DetectGame() : initialFolder;
            browse.Text = T("Browse…", "Обзор…"); browse.SetBounds(565, 157, 100, 30);
            browse.Click += delegate
            {
                using (var dialog = new FolderBrowserDialog { Description = T("Select the SnowRunner game folder", "Выберите папку SnowRunner") })
                    if (dialog.ShowDialog(this) == DialogResult.OK) folder.Text = dialog.SelectedPath;
            };
            var policy = new Label { Text = T("The installer checks the EXE, backs up changed files and preserves your INI.\nAn unknown dinput8.dll is never overwritten. Close the game first.",
                "Установщик проверит EXE, создаст резервную копию и сохранит ваш INI.\nНеизвестный dinput8.dll не заменяется. Перед установкой закройте игру."), Location = new Point(24, 201), Size = new Size(645, 53) };
            install.Text = T("Install / update", "Установить / обновить"); install.SetBounds(24, 265, 215, 36);
            uninstall.Text = T("Remove HUD", "Удалить HUD"); uninstall.SetBounds(249, 265, 160, 36);
            var licenses = new Button { Text = T("Help / licenses", "Справка / лицензии"), Location = new Point(419, 265), Size = new Size(246, 36) };
            licenses.Click += delegate { ShowHelp(); };
            log.SetBounds(24, 315, 641, 145); log.Multiline = true; log.ReadOnly = true; log.ScrollBars = ScrollBars.Vertical;
            install.Click += delegate { Begin(false); };
            uninstall.Click += delegate
            {
                if (MessageBox.Show(this, T("Remove only the HUD ASI? Settings and the shared loader will remain.", "Удалить только ASI мода? Настройки и общий загрузчик останутся."), Text, MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes) Begin(true);
            };
            worker.DoWork += delegate(object sender, DoWorkEventArgs e)
            {
                var request = (string[])e.Argument;
                var service = new Installer(Program.BundledPackage(), Installer.GameRunning, delegate(string message) { worker.ReportProgress(0, message); });
                if (request[1] == "remove") service.Uninstall(request[0]); else service.Install(request[0]);
            };
            worker.ProgressChanged += delegate(object sender, ProgressChangedEventArgs e) { log.AppendText((string)e.UserState + Environment.NewLine); };
            worker.RunWorkerCompleted += delegate(object sender, RunWorkerCompletedEventArgs e)
            {
                SetBusy(false);
                if (e.Error == null) { log.AppendText(T("Done.", "Готово.") + Environment.NewLine); return; }
                log.AppendText(e.Error.Message + Environment.NewLine);
                if (e.Error is UnauthorizedAccessException && MessageBox.Show(this, T("Write access denied. Restart this installer as administrator?", "Недостаточно прав записи. Перезапустить установщик от администратора?"), Text, MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
                {
                    try
                    {
                        string bin = Installer.ResolveGameBin(folder.Text);
                        Process.Start(new ProcessStartInfo(Application.ExecutablePath, "--folder \"" + bin.TrimEnd('\\') + "\"") { UseShellExecute = true, Verb = "runas" });
                        Close();
                    }
                    catch (Exception error) { MessageBox.Show(this, error.Message, Text); }
                }
                else MessageBox.Show(this, e.Error.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
            };
            FormClosing += delegate(object sender, FormClosingEventArgs e) { if (worker.IsBusy) e.Cancel = true; };
            Controls.AddRange(new Control[] { title, description, hint, folder, browse, policy, install, uninstall, licenses, log });
        }
        private void SetBusy(bool busy) { install.Enabled = uninstall.Enabled = browse.Enabled = folder.Enabled = !busy; UseWaitCursor = busy; }
        private void Begin(bool remove) { log.Clear(); SetBusy(true); worker.RunWorkerAsync(new string[] { folder.Text, remove ? "remove" : "install" }); }
        private void ShowHelp()
        {
            using (var help = new Form { Text = T("HUD installation and licenses", "Установка HUD и лицензии"), Size = new Size(790, 630), StartPosition = FormStartPosition.CenterParent })
            {
                var text = new TextBox { Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Both, Dock = DockStyle.Fill, Font = new Font("Segoe UI", 10), WordWrap = true };
                text.Text = Encoding.UTF8.GetString(Program.Resource(Russian ? "INSTALL_NATIVE.md" : "MODIO_DESCRIPTION_EN.md")) + "\r\n\r\n--- MinHook ---\r\n" + Encoding.UTF8.GetString(Program.Resource("MinHook.txt")) + "\r\n\r\n--- Ultimate ASI Loader ---\r\n" + Encoding.UTF8.GetString(Program.Resource("Ultimate-ASI-Loader.txt"));
                text.Text = text.Text.Replace("\r\n", "\n").Replace("\n", "\r\n");
                help.Controls.Add(text);
                help.Shown += delegate { text.Select(0, 0); text.ScrollToCaret(); };
                help.ShowDialog(this);
            }
        }
        private static string DetectGame()
        {
            var candidates = new List<string>();
            string manifests = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "Epic", "EpicGamesLauncher", "Data", "Manifests");
            if (Directory.Exists(manifests))
                foreach (string file in Directory.GetFiles(manifests, "*.item"))
                    try
                    {
                        if (new FileInfo(file).Length > 2 * 1024 * 1024) continue;
                        var item = new JavaScriptSerializer().Deserialize<Dictionary<string, object>>(File.ReadAllText(file));
                        object display, path;
                        if (item.TryGetValue("DisplayName", out display) && String.Equals(display as string, "SnowRunner", StringComparison.OrdinalIgnoreCase) && item.TryGetValue("InstallLocation", out path)) candidates.Add(path as string);
                    }
                    catch (Exception error) { if (!(error is IOException || error is UnauthorizedAccessException || error is ArgumentException || error is InvalidOperationException)) throw; }
            string steam = Registry.GetValue(@"HKEY_CURRENT_USER\Software\Valve\Steam", "SteamPath", "") as string;
            if (!String.IsNullOrEmpty(steam))
            {
                candidates.Add(Path.Combine(steam, "steamapps", "common", "SnowRunner"));
                string libraries = Path.Combine(steam, "steamapps", "libraryfolders.vdf");
                if (File.Exists(libraries))
                    try
                    {
                        foreach (Match match in Regex.Matches(File.ReadAllText(libraries), "\"path\"\\s+\"([^\"]+)\""))
                            candidates.Add(Path.Combine(match.Groups[1].Value.Replace("\\\\", "\\"), "steamapps", "common", "SnowRunner"));
                    }
                    catch (IOException) { }
            }
            foreach (string candidate in candidates)
                try { return Installer.ResolveGameBin(candidate); }
                catch (Exception error) { if (!(error is IOException || error is ArgumentException || error is InvalidOperationException || error is UnauthorizedAccessException)) throw; }
            return "";
        }
    }
}
