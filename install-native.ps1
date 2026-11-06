param([string]$GameBin = 'D:\Games\SnowRunner\en_us\Sources\Bin', [switch]$Uninstall, [ValidateRange(-1,2000)][int]$HudBottom = -1)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$bin = (Resolve-Path -LiteralPath $GameBin).ProviderPath
if (-not (Test-Path -LiteralPath (Join-Path $bin 'SnowRunner.exe'))) { throw "SnowRunner.exe not found in $bin" }
if (Get-Process SnowRunner -ErrorAction SilentlyContinue) { throw 'Close SnowRunner before installing or removing the mod.' }
if (-not $Uninstall -and (Get-Process SnowRunnerCruiseControl -ErrorAction SilentlyContinue)) { throw 'Exit the old tray utility before installing the ASI mod.' }
$package = Join-Path $PSScriptRoot 'dist\native'
$manifestPath = Join-Path $bin 'SnowRunnerAutopilot.install.json'
$previous = @{}
if (Test-Path -LiteralPath $manifestPath) {
    $data = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    foreach ($property in $data.Files.PSObject.Properties) { $previous[$property.Name] = $property.Value }
}
$names = @('dinput8.dll','SnowRunnerAutopilot.asi','SnowRunnerAutopilot.ini')
if ($Uninstall) {
    if ($previous.Count -eq 0) { throw 'Installation manifest missing; no files removed.' }
    foreach ($name in $names) {
        $target = Join-Path $bin $name
        if (-not $previous.ContainsKey($name) -or -not (Test-Path -LiteralPath $target)) { continue }
        if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -eq $previous[$name]) {
            Remove-Item -LiteralPath $target
            Write-Output "Removed: $target"
        } else { Write-Warning "Preserved modified file: $target" }
    }
    Remove-Item -LiteralPath $manifestPath
    return
}
# Validate every destination before copying anything. Never overwrite an
# unrelated proxy loader or a modified binary owned by another mod.
foreach ($name in $names) {
    $source = Join-Path $package $name
    if (-not (Test-Path -LiteralPath $source)) { throw "Build package first; missing $source" }
    $target = Join-Path $bin $name
    if ((Test-Path -LiteralPath $target) -and $name -ne 'SnowRunnerAutopilot.ini') {
        $hash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
        $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        if ($hash -ne $sourceHash -and (-not $previous.ContainsKey($name) -or $hash -ne $previous[$name])) {
            throw "Existing file belongs to another installation or was modified; preserved: $target"
        }
    }
}
$installed = @{}
foreach ($name in $names) {
    $source = Join-Path $package $name
    $target = Join-Path $bin $name
    if ($name -eq 'SnowRunnerAutopilot.ini' -and (Test-Path -LiteralPath $target)) {
        if ($previous.ContainsKey($name)) { $installed[$name] = $previous[$name] }
        Write-Output "Preserved settings: $target"
        continue
    }
    Copy-Item -LiteralPath $source -Destination $target
    $installed[$name] = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    Write-Output "Installed: $target"
}
if ($HudBottom -ge 0) {
    $settingsPath = Join-Path $bin 'SnowRunnerAutopilot.ini'
    $settings = Get-Content -LiteralPath $settingsPath -Raw
    if ($settings -match '(?m)^Bottom=\d+') {
        $settings = $settings -replace '(?m)^Bottom=\d+', "Bottom=$HudBottom"
    } else { throw 'HUD Bottom setting was not found; settings preserved.' }
    Set-Content -LiteralPath $settingsPath -Value $settings -Encoding utf8 -NoNewline
    $installed['SnowRunnerAutopilot.ini'] = (Get-FileHash -LiteralPath $settingsPath -Algorithm SHA256).Hash
    Write-Output "HUD raised: Bottom=$HudBottom"
}
@{ Version = 1; Files = $installed } | ConvertTo-Json | Set-Content -LiteralPath $manifestPath -Encoding utf8
Write-Output 'Native mod installed. Start SnowRunner normally; do not run the old EXE utility.'
