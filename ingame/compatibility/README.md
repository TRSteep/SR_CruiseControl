# SnowRunner CC — compatibility diagnostic 1.0.0

This is a **read-only diagnostic**, not a universal HUD release. It does not install or launch SnowRunner, attach to its process, install hooks, call Scaleform or send keyboard input. Steam support is **not certified** by a successful scan.

## For Steam testers

1. Extract `SnowRunnerCC-Compatibility-1.0.0-win-x64.zip` into a writable folder.
2. Double-click `SnowRunnerCC-Compatibility.exe` and select your actual `SnowRunner.exe` using the file picker. Steam's **Browse local files** command helps locate it. Do not select the launcher or the mod installer.
3. Wait for the completion message. A `SnowRunnerCC-Compatibility-<timestamp>.log` file is created next to the tool. No mod installation or ASI loader is required; the game can stay closed.
4. Send the report together with the game version, store (Steam/Epic) and whether it is the normal or test branch. The report contains the executable hash, PE metadata, signature match counts and relative function addresses; it does not include save data, account details or your full game path. The executable itself is not required for this first step.

CLI:

```text
SnowRunnerCC-Compatibility.exe "D:\SteamLibrary\steamapps\common\SnowRunner\Sources\Bin\SnowRunner.exe" --output "D:\Reports\Steam.log"
```

The output file must not already exist. Existing files are never overwritten. Exit code 0 means structural candidates were found; 2 means missing/ambiguous candidates; 1 means a file, format or reporting error. All outcomes remain diagnostic only. Logs may identify a previously reviewed hash; this is an ABI profile, not automatic identification of the installed storefront.

## Для тестирования Steam

Распакуйте ZIP в доступную для записи папку, запустите EXE и выберите файл `SnowRunner.exe` вашей игры. Устанавливать ASI и запускать игру не нужно. Пришлите полученный журнал, версию игры, магазин и ветку (обычная/тестовая). Отчёт не содержит сохранений или данных аккаунта. Совпадения сигнатур ещё не подтверждают правильность ABI и не включают управление/HUD.

## Development

From the repository root, Windows x64 and MSYS2 UCRT64 GCC:

```powershell
.\ingame\compatibility\build.ps1 -Package -ReferenceExe 'D:\Games\SnowRunner\en_us\Sources\Bin\SnowRunner.exe'
```

Outputs are under `ingame/build/compatibility`. The tester ZIP contains only the EXE, this README and build metadata. The optional `SnowRunnerCCCompatibility.asi` uses the same file scanner from inside the process and writes `SnowRunnerCCCompatibility.log`; its build excludes the hook-installation path entirely. It is not bundled with the tester ZIP, because the EXE avoids loader conflicts and manual ASI installation.

Opt-in experimental HUD:

```powershell
.\ingame\experimental\scaleform\build.ps1 -Autopilot -ResolveSignatures -Package
```

This builds into a **separate** `ingame/build/compatibility/native-hud` directory. Like normal HUD 1.2.1 it resolves ten functions from signatures and verifies loaded bytes and the Movie vtable before hooks. There is no hash allowlist: known hashes identify prior testing only. Unknown builds are allowed after structural checks, with an unreviewed-ABI warning. This is experimental compatibility, not proof of ABI safety. The normal installer embeds this read-only scanner and requires a successful structural scan before changing game files. `-AllowUnreviewed` retains separate names and staged observation/control for debugging.

The scanner validates x64 PE section bounds and exception entries, searches executable sections, rejects missing or ambiguous entries, verifies a factory string reference and the Movie destructor/Advance vtable relationship (slots 0/24). Generic deleting destructors are disambiguated by that unique vtable. It does not select the first byte match. Changing RVAs, relative calls and RIP displacements is modelled by tests; malformed/truncated images and duplicate candidates are rejected.

`generate_signatures.py` regenerates the checked-in masked patterns from the reviewed Epic EXE, using pefile 2024.8.26 and Capstone 5.x. Game-dependent RIP displacements and relative branch/call operands are masked. The generated patterns are small code signatures, not redistributed game resources. Signatures are intentionally conservative and may fail on a different compiler build.

## Next step

Review Steam reports, refine the discovery rules if needed, then validate call signatures, object layouts and reference lifetimes with a separate UI observer build. After a successful live HUD/control test, add a reviewed Steam ABI profile and update the installer. Structural matching alone cannot establish a new build's ABI, rather than treating structural matches as ABI certification.

HUD 1.2.1 is an experimental compatibility release without an EXE hash allowlist. It does not claim universal Steam compatibility. These build commands do not replace any installed game files.

Local validation: all ten reviewed Epic functions and structural links were resolved; 47 scanner/policy/profile tests and 7 private-image binding checks passed. The existing HUD tests also passed. The installed game EXE and HUD ASI hashes remained unchanged. The diagnostic import audit found no keyboard-input, process-write or hook-installation imports; MinGW CRT can still import VirtualProtect for its own runtime startup.

## Explicit unreviewed-build experiment

```powershell
.\ingame\compatibility\build.ps1 -Package
.\ingame\experimental\scaleform\build.ps1 -Autopilot -ResolveSignatures -AllowUnreviewed -Package
```

The second command writes `build/compatibility/unreviewed-hud/SnowRunnerCC-HUD-unreviewed-experimental.zip`. It uses distinct ASI/INI/log names. Its hash allowlist is disabled; signature uniqueness, PE bounds, vtable relationships and loaded-byte checks remain mandatory. Stage 0 observes loading with no authored HUD or keyboard control; Stage 1 adds the HUD/lifecycle; Stage 2 adds throttle input. Close the game before changing Stage. UI checkpoints are flushed immediately. This is an explicit experimental run on an unverified ABI, not certified Steam support. See [experimental testing instructions](EXPERIMENTAL_RU.md).
