using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Web.Script.Serialization;

namespace SnowRunnerCC.Setup
{
    internal sealed class Package
    {
        internal const string Asi = "SnowRunnerNativeAutopilot.asi";
        internal const string Ini = "SnowRunnerNativeAutopilot.ini";
        internal const string Loader = "dinput8.dll";
        internal const string Receipt = "SnowRunnerCC.install.json";
        internal string Version, SupportedExe;
        internal readonly Dictionary<string, byte[]> Files = new Dictionary<string, byte[]>();
        internal readonly Dictionary<string, string> Hashes = new Dictionary<string, string>();
        internal static string Hash(byte[] bytes)
        {
            using (SHA256 sha = SHA256.Create()) return Hex(sha.ComputeHash(bytes));
        }
        internal static string FileHash(string path)
        {
            using (Stream input = File.OpenRead(path))
            using (SHA256 sha = SHA256.Create()) return Hex(sha.ComputeHash(input));
        }
        private static string Hex(byte[] bytes) { return BitConverter.ToString(bytes).Replace("-", "").ToLowerInvariant(); }
        internal void Verify()
        {
            foreach (string name in new string[] { Asi, Ini, Loader })
                if (!Files.ContainsKey(name) || !Hashes.ContainsKey(name) || Hash(Files[name]) != Hashes[name])
                    throw new InvalidDataException("Damaged installer payload: " + name);
        }
    }

    internal sealed class Installer
    {
        private readonly Package package;
        private readonly Func<bool> gameRunning;
        private readonly Action<string> notify;
        // Used by tests to simulate an interrupted installation.
        internal Action<string> AfterWrite { get; set; }
        internal Installer(Package value, Func<bool> busy, Action<string> progress)
        { package = value; gameRunning = busy; notify = progress ?? delegate { }; }

        internal static string ResolveGameBin(string input)
        {
            if (String.IsNullOrWhiteSpace(input)) throw new InvalidOperationException("Select the SnowRunner folder.");
            string root = Path.GetFullPath(input.Trim().Trim('"'));
            if (File.Exists(root) && Path.GetFileName(root).Equals("SnowRunner.exe", StringComparison.OrdinalIgnoreCase)) root = Path.GetDirectoryName(root);
            foreach (string path in new string[] { root, Path.Combine(root, "en_us", "Sources", "Bin"), Path.Combine(root, "Sources", "Bin") })
                if (File.Exists(Path.Combine(path, "SnowRunner.exe"))) return Path.GetFullPath(path);
            throw new InvalidOperationException("SnowRunner.exe was not found. Select the game folder or Sources\\Bin.");
        }
        private void RequireClosed()
        {
            if (gameRunning()) throw new InvalidOperationException("Close SnowRunner before installing or removing the HUD.");
        }
        internal static bool GameRunning()
        {
            Process[] processes = Process.GetProcessesByName("SnowRunner");
            bool result = processes.Length != 0;
            foreach (Process process in processes) process.Dispose();
            return result;
        }
        private static void WriteAtomic(string path, byte[] bytes)
        {
            string temporary = path + ".cc-" + Guid.NewGuid().ToString("N") + ".tmp";
            try
            {
                File.WriteAllBytes(temporary, bytes);
                if (File.Exists(path)) File.Replace(temporary, path, null);
                else File.Move(temporary, path);
            }
            finally { if (File.Exists(temporary)) File.Delete(temporary); }
        }

        internal string Install(string folder)
        {
            RequireClosed(); package.Verify();
            string bin = ResolveGameBin(folder);
            notify("Checking SnowRunner.exe SHA256...");
            if (Package.FileHash(Path.Combine(bin, "SnowRunner.exe")) != package.SupportedExe)
                throw new InvalidOperationException("Unsupported SnowRunner.exe. This HUD build supports the tested Epic Games 44.1 executable only. No game files were changed.");
            string loader = Path.Combine(bin, Package.Loader);
            if (File.Exists(loader) && Package.FileHash(loader) != package.Hashes[Package.Loader])
                throw new InvalidOperationException("An unrecognised dinput8.dll is already installed. It was preserved. Use the manual installation instructions with your existing compatible x64 ASI loader.");
            RequireClosed();
            string id = DateTime.UtcNow.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N");
            string backup = Path.Combine(bin, "SnowRunnerCC_Backups", id);
            Directory.CreateDirectory(backup);
            string[] names = { Package.Asi, Package.Ini, Package.Loader, Package.Receipt, "SnowRunnerAutopilot.asi", "SnowRunnerScaleformProbe.asi" };
            var existed = new Dictionary<string, bool>();
            foreach (string name in names)
            {
                string target = Path.Combine(bin, name);
                existed[name] = File.Exists(target);
                if (existed[name]) File.Copy(target, Path.Combine(backup, name));
            }
            var written = new List<string>();
            var disabled = new List<string>();
            try
            {
                foreach (string name in new string[] { Package.Asi, Package.Ini, Package.Loader })
                {
                    if (name != Package.Asi && existed[name]) { notify("Preserved: " + name); continue; }
                    notify("Installing: " + name);
                    WriteAtomic(Path.Combine(bin, name), package.Files[name]);
                    written.Add(name);
                    if (Package.FileHash(Path.Combine(bin, name)) != package.Hashes[name]) throw new IOException("Installed file checksum mismatch: " + name);
                    if (AfterWrite != null) AfterWrite(name);
                }
                foreach (string name in new string[] { "SnowRunnerAutopilot.asi", "SnowRunnerScaleformProbe.asi" })
                    if (existed[name])
                    {
                        File.Move(Path.Combine(bin, name), Path.Combine(bin, name + ".disabled.cc-" + id));
                        disabled.Add(name); notify("Disabled previous HUD: " + name);
                        if (AfterWrite != null) AfterWrite(name);
                    }
                var receipt = new Dictionary<string, object> {
                    { "Product", "SnowRunner CC HUD" }, { "Schema", 1 }, { "Version", package.Version },
                    { "AsiSha256", package.Hashes[Package.Asi] }, { "Backup", backup }
                };
                WriteAtomic(Path.Combine(bin, Package.Receipt), Encoding.UTF8.GetBytes(new JavaScriptSerializer().Serialize(receipt)));
                written.Add(Package.Receipt);
                if (AfterWrite != null) AfterWrite(Package.Receipt);
                notify("Installed HUD " + package.Version + ". Start SnowRunner normally.");
                notify("Backup: " + backup);
                return backup;
            }
            catch (Exception failure)
            {
                var errors = new List<string>();
                for (int i = disabled.Count - 1; i >= 0; --i)
                {
                    string name = disabled[i];
                    try { File.Move(Path.Combine(bin, name + ".disabled.cc-" + id), Path.Combine(bin, name)); }
                    catch (Exception error) { errors.Add(error.Message); }
                }
                for (int i = written.Count - 1; i >= 0; --i)
                {
                    string name = written[i], target = Path.Combine(bin, name);
                    try
                    {
                        if (existed[name]) WriteAtomic(target, File.ReadAllBytes(Path.Combine(backup, name)));
                        else if (File.Exists(target)) File.Delete(target);
                    }
                    catch (Exception error) { errors.Add(error.Message); }
                }
                if (errors.Count != 0) throw new IOException("Installation failed; rollback needs attention. Backup: " + backup + "\n" + String.Join("\n", errors), failure);
                notify("Installation rolled back. Backup: " + backup);
                throw;
            }
        }

        internal void Uninstall(string folder)
        {
            RequireClosed(); package.Verify();
            string bin = ResolveGameBin(folder), asi = Path.Combine(bin, Package.Asi), receiptPath = Path.Combine(bin, Package.Receipt);
            if (!File.Exists(asi)) { notify("The HUD ASI is already absent. Settings and loader were preserved."); return; }
            string ownedHash = package.Hashes[Package.Asi];
            if (File.Exists(receiptPath))
            {
                if (new FileInfo(receiptPath).Length > 65536) throw new InvalidDataException("Invalid installation receipt; files preserved.");
                var receipt = new JavaScriptSerializer().Deserialize<Dictionary<string, object>>(File.ReadAllText(receiptPath));
                if (!receipt.ContainsKey("Product") || !Object.Equals(receipt["Product"], "SnowRunner CC HUD") || !receipt.ContainsKey("AsiSha256"))
                    throw new InvalidDataException("Invalid installation receipt; files preserved.");
                ownedHash = receipt["AsiSha256"] as string;
            }
            if (Package.FileHash(asi) != ownedHash) throw new InvalidOperationException("The HUD ASI was modified after installation. It was preserved; remove it manually if intended.");
            string backup = Path.Combine(bin, "SnowRunnerCC_Backups", "uninstall-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(backup);
            File.Move(asi, Path.Combine(backup, Package.Asi));
            notify("HUD removed. ASI backup: " + backup);
            // The loader may be shared. Never remove it, settings or other mods.
            notify("INI, ASI loader and previous backups were preserved. Previous HUDs remain disabled.");
        }
    }
}
