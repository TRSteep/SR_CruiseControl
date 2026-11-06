$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$vendor = Join-Path $PSScriptRoot 'vendor'
New-Item -ItemType Directory -Path $vendor -Force | Out-Null

# ImGui and MinHook source trees are included in Git. Only the ASI loader
# binary must be retrieved for a fresh checkout; its license is included.
foreach ($required in @('imgui-1.91.6\imgui.cpp', 'minhook-1.3.4\include\MinHook.h')) {
    if (-not (Test-Path -LiteralPath (Join-Path $vendor $required))) {
        throw "Missing vendored source: $required. Restore it from Git."
    }
}
$loaderDir = Join-Path $vendor 'asi-loader-v10.0.2'
$loader = Join-Path $loaderDir 'dinput8.dll'
$binaryHash = '57EC56390C92A18B82F58855E63C93985E254D1E29D5F75CBC28DC08F96C8A12'
if (Test-Path -LiteralPath $loader) {
    if ((Get-FileHash -LiteralPath $loader -Algorithm SHA256).Hash -ne $binaryHash) {
        throw 'Existing ASI loader differs from the pinned official binary; preserved.'
    }
    Write-Output 'Pinned ASI loader already present; SHA256 verified.'
    return
}
$archive = Join-Path $vendor 'Ultimate-ASI-Loader-NoPDB_x64-v10.0.2.zip'
if (-not (Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest -Uri 'https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/v10.0.2/Ultimate-ASI-Loader-NoPDB_x64.zip' -OutFile $archive
}
$archiveHash = 'FE964ADF4DE7F467FF39C440A4059DE02E37D0574A62944EEF2F55A35EDAFA2F'
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $archiveHash) {
    throw 'ASI loader archive SHA256 mismatch; nothing extracted.'
}
Expand-Archive -LiteralPath $archive -DestinationPath $loaderDir -Force
if ((Get-FileHash -LiteralPath $loader -Algorithm SHA256).Hash -ne $binaryHash) {
    throw 'Extracted ASI loader SHA256 mismatch.'
}
Write-Output 'ASI loader v10.0.2 x64 prepared; archive and DLL SHA256 verified.'
