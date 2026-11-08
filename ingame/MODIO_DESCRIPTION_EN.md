# SnowRunner Cruise control (SnowRunner CC) — Native HUD

**PC ONLY — MANUAL INSTALLATION. Subscribing in the in-game Mod Browser does not install this ASI plugin.**

Hold full throttle without holding W. Toggle with F6; press W to take over manually, or S/N to cancel. This is a throttle latch, not a target-speed controller or route autopilot.

The control appears inside SnowRunner's native Scaleform HUD above the handbrake row. It follows the original HUD animation and visibility. The speedometer icon turns green when enabled; the label remains light and the F6 hint uses a separate native key badge.

HUD labels follow the game's handbrake language: Russian, English and French are included; other languages use English. Keyboard layout and Windows language do not select the HUD language. The new language switching has automated coverage; switching all three languages in a live game has not yet been verified.

If W is already held, press F6 and then release W: cruise takes over and keeps the throttle held. Autorepeat from the original W hold is ignored. The next distinct W press takes manual control; other cancellation keys retain their behaviour.

## Compatibility

- Windows x64, .NET Framework 4.x for the installer.
- Supported executable: the tested **Epic Games / SnowRunner 44.1** build. Both the installer and plugin check the exact EXE SHA256. Steam, Game Pass and other builds are not confirmed.
- Use physical W for throttle and S for brake. Gamepad/wheel input and switching the engine off with a mouse are not tracked.
- Changing focus or pressing Esc/Enter/M/F1/F4 cancels throttle. Returning to the game does not re-enable it.

## Installation

1. Close SnowRunner.
2. Download and extract `SnowRunnerCC-HUD-1.1.4-Setup.zip` from this page's manual files, if available, or obtain the published installer from GitHub Releases.
3. Run `SnowRunnerCC-HUD-1.1.4-Setup.exe` and select the game root or the folder containing `SnowRunner.exe`.
4. Click **Install / update**. Administrator access is requested only if the selected directory requires it.
5. Launch SnowRunner normally. No separate launcher is needed afterwards.

The installer preserves your INI, backs up replaced files, disables the previous overlay/test ASI, and refuses to overwrite an unknown `dinput8.dll`. If you already use another compatible ASI loader, follow the manual instructions instead. It does not edit saves, PAK files or the game's EXE.

## Updating and removing

Run the installer again to update. Your INI is preserved. **Remove HUD** removes only the installed ASI after verifying its hash; INI, the shared loader and backups remain. Previous overlay/test versions stay disabled. The installer is a portable setup tool and does not add an entry to Windows Installed Apps.

Version: **HUD 1.1.4**. The installer is unsigned. Installation/update/removal/rollback and payload checks are automated; a full interactive installer run on every supported Windows configuration is not claimed.

[Source code](https://github.com/TRSteep/SR_CruiseControl) · [HUD 1.1.4 release](https://github.com/TRSteep/SR_CruiseControl/releases/tag/ingame-v1.1.4)

Report issues with the game store/build, HUD version and `SnowRunnerNativeAutopilot.log`. Do not include account tokens or save files.

MinHook and Ultimate ASI Loader licenses are included. No game assets are redistributed. This is an unofficial community mod.
