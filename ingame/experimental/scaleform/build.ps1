param([string]$Toolchain = 'C:\msys64\ucrt64\bin', [switch]$Package, [switch]$Autopilot)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = $PSScriptRoot
$out = Join-Path $root '..\..\build\scaleform-probe'
$minhook = Join-Path $root '..\..\vendor\minhook-1.3.4'
New-Item -ItemType Directory -Path $out -Force | Out-Null
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
    $flags = @('-std=c++17','-O2','-Wall','-Wextra','-Werror','-static','-pthread',"-I$minhook\include")
    if($Autopilot){$flags+='-DSCALEFORM_NATIVE_AUTOPILOT'}
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
    if($Autopilot){
        & "$Toolchain\g++.exe" @flags (Join-Path $root '..\..\tests\StateTests.cpp') -o (Join-Path $out 'StateTests.exe')
        if($LASTEXITCODE -ne 0){throw 'Control test compilation failed.'}
        & (Join-Path $out 'StateTests.exe')
        if($LASTEXITCODE -ne 0){throw 'Control tests failed.'}
    }
    $binary=if($Autopilot){'SnowRunnerNativeAutopilot.asi'}else{'SnowRunnerScaleformProbe.asi'}
    & "$Toolchain\g++.exe" @flags -shared '-Wl,--exclude-all-symbols' (Join-Path $root 'Probe.cpp') $objects -lbcrypt -o (Join-Path $out $binary)
    if ($LASTEXITCODE -ne 0) { throw 'Probe compilation failed.' }
    Write-Output "Built: $out\$binary"
    if ($Package) {
        $packageDir=Join-Path $out 'package'
        New-Item -ItemType Directory -Path $packageDir -Force | Out-Null
        $config=if($Autopilot){'SnowRunnerNativeAutopilot.ini'}else{'SnowRunnerScaleformProbe.ini'}
        Copy-Item -LiteralPath (Join-Path $out $binary),(Join-Path $root $config),(Join-Path $root 'README.md') -Destination $packageDir
        Copy-Item -LiteralPath (Join-Path $minhook 'LICENSE.txt') -Destination (Join-Path $packageDir 'MinHook-LICENSE.txt')
        $archive=Join-Path $out $(if($Autopilot){'SnowRunner-Native-Autopilot-v9.zip'}else{'SnowRunner-Scaleform-Prototype-v7.zip'})
        $packageFiles=@($binary,$config,'README.md','MinHook-LICENSE.txt') | ForEach-Object { Join-Path $packageDir $_ }
        Compress-Archive -LiteralPath $packageFiles -DestinationPath $archive -Force
        Write-Output "Packaged: $archive"
    }
} finally { $env:PATH = $oldPath }
