param([string]$Toolchain = 'C:\msys64\ucrt64\bin', [switch]$Package, [switch]$Autopilot, [switch]$ResolveSignatures, [string]$ReferenceExe, [switch]$AllowUnreviewed)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = $PSScriptRoot
$out = Join-Path $root '..\..\build\scaleform-probe'
if($ResolveSignatures) {
    if(!$Autopilot){throw '-ResolveSignatures requires -Autopilot.'}
    $out=Join-Path $root '..\..\build\compatibility\native-hud'
}
if($AllowUnreviewed) {
    if(!$ResolveSignatures -or !$Autopilot){throw '-AllowUnreviewed requires -ResolveSignatures -Autopilot.'}
    $out=Join-Path $root '..\..\build\compatibility\unreviewed-hud'
}
$minhook = Join-Path $root '..\..\vendor\minhook-1.3.4'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$version = [IO.File]::ReadAllText((Join-Path $root '..\..\VERSION')).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw 'Invalid HUD VERSION.' }
[IO.File]::WriteAllText((Join-Path $out 'SnowRunnerCCVersion.h'), ('#define SNOWRUNNER_CC_VERSION "' + $version + '"'), (New-Object Text.UTF8Encoding($false)))
$oldPath = $env:PATH
try {
    $env:PATH = "$Toolchain;$oldPath"
    $objects = @()
    foreach ($file in @('buffer.c','hook.c','trampoline.c','hde\hde64.c')) {
        $target = Join-Path $out (([IO.Path]::GetFileNameWithoutExtension($file)) + '.o')
        & "$Toolchain\gcc.exe" -O2 -c (Join-Path $minhook "src\$file") "-I$minhook\include" -o $target
        if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $file" }
        $objects += $target
    }
    $flags = @('-std=c++17','-O2','-Wall','-Wextra','-Werror','-static','-pthread',"-I$minhook\include", "-I$out")
    if($Autopilot){$flags+='-DSCALEFORM_NATIVE_AUTOPILOT'}
    if($Autopilot){$flags+='-DSR_CC_SIGNATURE_RESOLVER'}
    if($AllowUnreviewed){$flags+='-DSR_CC_ALLOW_UNREVIEWED'}
    & "$Toolchain\g++.exe" @flags (Join-Path $root 'ProbeTests.cpp') $objects -lbcrypt -o (Join-Path $out 'ProbeTests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Probe test compilation failed.' }
    & (Join-Path $out 'ProbeTests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Probe tests failed.' }
    & "$Toolchain\g++.exe" @flags (Join-Path $root 'NativeUiTests.cpp') -o (Join-Path $out 'NativeUiTests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Native UI test compilation failed.' }
    & (Join-Path $out 'NativeUiTests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Native UI tests failed.' }
    & "$Toolchain\g++.exe" @flags (Join-Path $root 'NativeSessionTests.cpp') -o (Join-Path $out 'NativeSessionTests.exe')
    if($LASTEXITCODE -ne 0){throw 'Session test compilation failed.'}
    & (Join-Path $out 'NativeSessionTests.exe')
    if($LASTEXITCODE -ne 0){throw 'Session tests failed.'}
    & "$Toolchain\g++.exe" @flags (Join-Path $root 'CruiseIconTests.cpp') -o (Join-Path $out 'CruiseIconTests.exe')
    if($LASTEXITCODE -ne 0){throw 'Cruise icon test compilation failed.'}
    & (Join-Path $out 'CruiseIconTests.exe')
    if($LASTEXITCODE -ne 0){throw 'Cruise icon tests failed.'}
    & "$Toolchain\g++.exe" @flags (Join-Path $root 'HudLocalizationTests.cpp') -o (Join-Path $out 'HudLocalizationTests.exe')
    if($LASTEXITCODE -ne 0){throw 'HUD localisation test compilation failed.'}
    & (Join-Path $out 'HudLocalizationTests.exe')
    if($LASTEXITCODE -ne 0){throw 'HUD localisation tests failed.'}
    if($Autopilot){
        & "$Toolchain\g++.exe" @flags (Join-Path $root '..\..\tests\StateTests.cpp') -o (Join-Path $out 'StateTests.exe')
        if($LASTEXITCODE -ne 0){throw 'Control test compilation failed.'}
        & (Join-Path $out 'StateTests.exe')
        if($LASTEXITCODE -ne 0){throw 'Control tests failed.'}
    }
    $binary=if($Autopilot){'SnowRunnerNativeAutopilot.asi'}else{'SnowRunnerScaleformProbe.asi'}
    if($AllowUnreviewed){$binary='SnowRunnerCCExperimental.asi'}
    & "$Toolchain\g++.exe" @flags -shared '-Wl,--exclude-all-symbols' (Join-Path $root 'Probe.cpp') $objects -lbcrypt -o (Join-Path $out $binary)
    if ($LASTEXITCODE -ne 0) { throw 'Probe compilation failed.' }
    if($Autopilot -and $ReferenceExe) {
        & "$Toolchain\g++.exe" @flags -municode (Join-Path $root '..\..\compatibility\ResolvedBindingTests.cpp') $objects -lbcrypt -o (Join-Path $out 'ResolvedBindingTests.exe')
        if($LASTEXITCODE -ne 0){throw 'Resolved binding test compilation failed.'}
        & (Join-Path $out 'ResolvedBindingTests.exe') $ReferenceExe
        if($LASTEXITCODE -ne 0){throw 'Resolved binding test failed.'}
    }
    Write-Output "Built: $out\$binary"
    if ($Package -and !$AllowUnreviewed) {
        $packageDir=Join-Path $out 'package'
        New-Item -ItemType Directory -Path $packageDir -Force | Out-Null
        $config=if($Autopilot){'SnowRunnerNativeAutopilot.ini'}else{'SnowRunnerScaleformProbe.ini'}
        Copy-Item -LiteralPath (Join-Path $out $binary),(Join-Path $root $config),(Join-Path $root 'README.md') -Destination $packageDir
        Copy-Item -LiteralPath (Join-Path $minhook 'LICENSE.txt') -Destination (Join-Path $packageDir 'MinHook-LICENSE.txt')
        $archive=Join-Path $out $(if($Autopilot){"SnowRunnerCC-HUD-ingame-v$version-win-x64.zip"}else{'SnowRunner-Scaleform-Prototype-v7.zip'})
        if($ResolveSignatures){$archive=Join-Path $out 'SnowRunnerCC-HUD-signatures-experimental.zip'}
        $packageFiles=@($binary,$config,'README.md','MinHook-LICENSE.txt') | ForEach-Object { Join-Path $packageDir $_ }
        Compress-Archive -LiteralPath $packageFiles -DestinationPath $archive -Force
        Write-Output "Packaged: $archive"
    }
    if($AllowUnreviewed -and $Package) {
        $stage=Join-Path $out ('experimental-'+[guid]::NewGuid().ToString('N'))
        $licenses=Join-Path $stage 'licenses'
        New-Item -ItemType Directory -Path $licenses -Force | Out-Null
        $compat=Join-Path $root '..\..\compatibility'
        $loader=Join-Path $root '..\..\vendor\asi-loader-v10.0.2\dinput8.dll'
        if((Get-FileHash -LiteralPath $loader).Hash -ne '57EC56390C92A18B82F58855E63C93985E254D1E29D5F75CBC28DC08F96C8A12'){throw 'Loader hash mismatch.'}
        Copy-Item -LiteralPath (Join-Path $out $binary),(Join-Path $compat 'SnowRunnerCCExperimental.ini'),$loader -Destination $stage
        Copy-Item -LiteralPath (Join-Path $compat 'EXPERIMENTAL_RU.md') -Destination (Join-Path $stage 'README_EXPERIMENTAL_RU.md')
        Copy-Item -LiteralPath (Join-Path $root '..\..\build\compatibility\SnowRunnerCC-Compatibility.exe') -Destination $stage
        Copy-Item -LiteralPath (Join-Path $minhook 'LICENSE.txt') -Destination (Join-Path $licenses 'MinHook.txt')
        Copy-Item -LiteralPath (Join-Path $root '..\..\vendor\asi-loader-v10.0.2\LICENSE.txt') -Destination (Join-Path $licenses 'Ultimate-ASI-Loader.txt')
        $meta=@{product='SnowRunner CC experimental HUD';hashAllowlist=$false;defaultStage=0;signatureChecks=$true;sourceCommit=(git -C (Join-Path $root '..\..\..') rev-parse HEAD).Trim();sourceTreeDirty=[bool](git -C (Join-Path $root '..\..\..') status --porcelain);asiSha256=(Get-FileHash -LiteralPath (Join-Path $out $binary)).Hash.ToLowerInvariant()}
        [IO.File]::WriteAllText((Join-Path $stage 'BUILD_INFO.json'),($meta|ConvertTo-Json),(New-Object Text.UTF8Encoding($false)))
        $archive=Join-Path $out 'SnowRunnerCC-HUD-unreviewed-experimental.zip'
        Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -Force
        [IO.File]::WriteAllText((Join-Path $out 'SHA256SUMS.txt'),((Get-FileHash -LiteralPath $archive).Hash.ToLowerInvariant()+'  '+[IO.Path]::GetFileName($archive)+"`n"),(New-Object Text.UTF8Encoding($false)))
        Write-Output "Experimental tester package (hash gate OFF, Stage=0): $archive"
    }
} finally { $env:PATH = $oldPath }
