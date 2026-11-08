using System;
using System.IO;
using System.Text;

namespace SnowRunnerCC.Setup
{
    internal static class InstallerTests
    {
        private static int checks;
        private static string root;
        private static void Check(bool okay) { ++checks; if (!okay) throw new Exception("Installer check failed: " + checks); }
        private static byte[] Bytes(string s) { return Encoding.UTF8.GetBytes(s); }
        private static Package Fixture()
        {
            var p = new Package { Version = "1.1.3", SupportedExe = Package.Hash(Bytes("supported game")) };
            foreach (string name in new string[] { Package.Asi, Package.Ini, Package.Loader })
            { p.Files[name] = Bytes("new " + name); p.Hashes[name] = Package.Hash(p.Files[name]); }
            return p;
        }
        private static string Game(string name)
        {
            string bin = Path.Combine(root, name, "en_us", "Sources", "Bin");
            Directory.CreateDirectory(bin); File.WriteAllBytes(Path.Combine(bin, "SnowRunner.exe"), Bytes("supported game"));
            return bin;
        }
        private static Installer Service(Package p, bool busy = false) { return new Installer(p, delegate { return busy; }, null); }
        private static void Reject(Action action)
        {
            bool rejected = false;
            try { action(); } catch (InvalidOperationException) { rejected = true; } catch (InvalidDataException) { rejected = true; } catch (IOException) { rejected = true; }
            Check(rejected);
        }
        public static int Main(string[] args)
        {
            root = Path.GetFullPath(args[0]); Directory.CreateDirectory(root);
            Package package = Fixture();
            string bin = Game("fresh"), gameRoot = Path.GetFullPath(Path.Combine(bin, "..", "..", ".."));
            Check(Installer.ResolveGameBin(gameRoot) == bin);
            Check(Installer.ResolveGameBin(Path.Combine(bin, "SnowRunner.exe")) == bin);
            Reject(delegate { Installer.ResolveGameBin(root); });
            string backup = Service(package).Install(gameRoot);
            Check(Directory.Exists(backup));
            foreach (string name in new string[] { Package.Asi, Package.Ini, Package.Loader }) Check(Package.FileHash(Path.Combine(bin, name)) == package.Hashes[name]);
            Check(File.ReadAllText(Path.Combine(bin, Package.Receipt)).Contains("SnowRunner CC HUD"));
            File.WriteAllText(Path.Combine(bin, Package.Ini), "custom key F7");
            File.WriteAllText(Path.Combine(bin, Package.Asi), "old ASI");
            File.WriteAllText(Path.Combine(bin, "SnowRunnerAutopilot.asi"), "overlay");
            File.WriteAllText(Path.Combine(bin, "SnowRunnerScaleformProbe.asi"), "probe");
            backup = Service(package).Install(bin);
            Check(File.ReadAllText(Path.Combine(bin, Package.Ini)) == "custom key F7");
            Check(File.ReadAllText(Path.Combine(backup, Package.Asi)) == "old ASI");
            Check(!File.Exists(Path.Combine(bin, "SnowRunnerAutopilot.asi")) && !File.Exists(Path.Combine(bin, "SnowRunnerScaleformProbe.asi")));
            Check(Directory.GetFiles(bin, "SnowRunnerAutopilot.asi.disabled.cc-*").Length == 1);
            Service(package).Uninstall(bin);
            Check(!File.Exists(Path.Combine(bin, Package.Asi)));
            Check(File.Exists(Path.Combine(bin, Package.Loader)) && File.ReadAllText(Path.Combine(bin, Package.Ini)) == "custom key F7");
            Service(package).Uninstall(bin); // Idempotent; no extra deletion.
            Service(package).Install(bin); File.WriteAllText(Path.Combine(bin, Package.Asi), "user modified");
            Reject(delegate { Service(package).Uninstall(bin); });
            Check(File.ReadAllText(Path.Combine(bin, Package.Asi)) == "user modified");
            foreach (string scenario in new string[] { "unknown-loader", "unsupported", "busy", "corrupt-payload" })
            {
                bin = Game(scenario); Package p = Fixture();
                if (scenario == "unknown-loader") File.WriteAllText(Path.Combine(bin, Package.Loader), "another mod's loader");
                if (scenario == "unsupported") File.WriteAllText(Path.Combine(bin, "SnowRunner.exe"), "unsupported exe");
                if (scenario == "corrupt-payload") p.Files[Package.Asi] = Bytes("damaged");
                string target = bin;
                Reject(delegate { Service(p, scenario == "busy").Install(target); });
                Check(!File.Exists(Path.Combine(bin, Package.Asi)) && !File.Exists(Path.Combine(bin, Package.Receipt)));
                Check(!Directory.Exists(Path.Combine(bin, "SnowRunnerCC_Backups")));
                if (scenario == "unknown-loader") Check(File.ReadAllText(Path.Combine(bin, Package.Loader)) == "another mod's loader");
            }
            foreach (string failurePoint in new string[] { Package.Asi, Package.Ini, Package.Loader, "SnowRunnerAutopilot.asi", Package.Receipt })
            {
                bin = Game("rollback-" + failurePoint); File.WriteAllText(Path.Combine(bin, Package.Asi), "previous ASI");
                File.WriteAllText(Path.Combine(bin, "SnowRunnerAutopilot.asi"), "previous overlay");
                File.WriteAllText(Path.Combine(bin, Package.Receipt), "previous receipt");
                Installer service = Service(package);
                service.AfterWrite = delegate(string name) { if (name == failurePoint) throw new IOException("Simulated interruption"); };
                string target = bin; Reject(delegate { service.Install(target); });
                Check(File.ReadAllText(Path.Combine(bin, Package.Asi)) == "previous ASI");
                Check(File.ReadAllText(Path.Combine(bin, "SnowRunnerAutopilot.asi")) == "previous overlay");
                Check(File.ReadAllText(Path.Combine(bin, Package.Receipt)) == "previous receipt");
                Check(!File.Exists(Path.Combine(bin, Package.Ini)) && !File.Exists(Path.Combine(bin, Package.Loader)));
                Check(Directory.GetFiles(bin, "*.tmp").Length == 0);
                Check(Directory.GetFiles(bin, "*.disabled.cc-*").Length == 0);
            }
            // Removal must still work after the game itself updates.
            bin = Game("game-updated"); Service(package).Install(bin);
            File.WriteAllText(Path.Combine(bin, "SnowRunner.exe"), "new game version");
            Service(package).Uninstall(bin); Check(!File.Exists(Path.Combine(bin, Package.Asi)));
            Console.WriteLine("PASS: " + checks + " installer installation/update/removal/rollback checks");
            return 0;
        }
    }
}
