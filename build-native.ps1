param([string]$Toolchain = 'C:\msys64\ucrt64\bin', [switch]$RunRenderTests)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = $PSScriptRoot
$out = Join-Path $root 'dist\native'
$obj = Join-Path $root 'native\build'
$imgui = Join-Path $root 'vendor\imgui-1.91.6'
$minhook = Join-Path $root 'vendor\minhook-1.3.4'
$cxx = Join-Path $Toolchain 'g++.exe'
$cc = Join-Path $Toolchain 'gcc.exe'
if (-not (Test-Path -LiteralPath $cxx)) { throw "C++ compiler not found: $cxx" }
New-Item -ItemType Directory -Path $out,$obj -Force | Out-Null
$oldPath = $env:PATH
try {
    $env:PATH = "$Toolchain;$oldPath"
    $objects = @()
    foreach ($file in @('buffer.c','hook.c','trampoline.c','hde\hde64.c')) {
        $source = Join-Path $minhook "src\$file"
        $target = Join-Path $obj (([IO.Path]::GetFileNameWithoutExtension($file)) + '.o')
        & $cc -O2 -c $source "-I$minhook\include" -o $target
        if ($LASTEXITCODE -ne 0) { throw "MinHook compilation failed: $file" }
        $objects += $target
    }
    $sources = @(
        (Join-Path $root 'native\Autopilot.cpp'),
        (Join-Path $imgui 'imgui.cpp'),
        (Join-Path $imgui 'imgui_draw.cpp'),
        (Join-Path $imgui 'imgui_tables.cpp'),
        (Join-Path $imgui 'imgui_widgets.cpp'),
        (Join-Path $imgui 'backends\imgui_impl_win32.cpp'),
        (Join-Path $imgui 'backends\imgui_impl_dx11.cpp')
    )
    $cppObjects = @()
    foreach ($source in $sources) {
        $target = Join-Path $obj (([IO.Path]::GetFileNameWithoutExtension($source)) + '.o')
        if (-not (Test-Path -LiteralPath $target) -or (Get-Item -LiteralPath $source).LastWriteTimeUtc -gt (Get-Item -LiteralPath $target).LastWriteTimeUtc -or $source -like '*Autopilot.cpp') {
            $warnings = @('-Wall','-Wextra')
            if ($source -like '*Autopilot.cpp') { $warnings += '-Werror' } else { $warnings += '-Wno-array-bounds' }
            & $cxx -std=c++17 -O2 @warnings -pthread -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS "-I$imgui" "-I$minhook\include" -c $source -o $target
            if ($LASTEXITCODE -ne 0) { throw "C++ compilation failed: $source" }
        }
        $cppObjects += $target
    }
    & $cxx -shared -static -pthread '-Wl,--exclude-all-symbols' `
        $cppObjects $objects -o (Join-Path $out 'SnowRunnerAutopilot.asi') `
        -ld3d11 -ldxgi -ld3dcompiler -ldwmapi -lgdi32 -limm32 -luser32
    if ($LASTEXITCODE -ne 0) { throw 'Native ASI compilation failed.' }
    & $cxx -std=c++17 -O2 -Wall -Wextra -Werror -static -o (Join-Path $obj 'StateTests.exe') (Join-Path $root 'native\StateTests.cpp')
    if ($LASTEXITCODE -ne 0) { throw 'Native state tests compilation failed.' }
    & (Join-Path $obj 'StateTests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Native state tests failed.' }
    if ($RunRenderTests) {
        $renderObjects = @($cppObjects | Where-Object { [IO.Path]::GetFileName($_) -ne 'Autopilot.o' })
        & $cxx -std=c++17 -O2 -Wall -Wextra -Wno-unused-function -static -pthread -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS `
            "-I$imgui" "-I$minhook\include" (Join-Path $root 'native\RenderTests.cpp') $renderObjects $objects `
            -o (Join-Path $obj 'RenderTests.exe') -ld3d11 -ldxgi -ld3dcompiler -ldwmapi -lgdi32 -limm32 -luser32
        if ($LASTEXITCODE -ne 0) { throw 'Render test compilation failed.' }
        Push-Location $root
        try { & (Join-Path $obj 'RenderTests.exe') } finally { Pop-Location }
        if ($LASTEXITCODE -ne 0) { throw 'DX11 render tests failed.' }
    }
    $loader = Join-Path $root 'vendor\asi-loader-v10.0.2\dinput8.dll'
    if (-not (Test-Path -LiteralPath $loader)) { throw 'x64 ASI Loader is missing from vendor.' }
    Copy-Item -LiteralPath $loader -Destination $out
    $config = Join-Path $out 'SnowRunnerAutopilot.ini'
    if (-not (Test-Path -LiteralPath $config)) { Copy-Item -LiteralPath (Join-Path $root 'native\SnowRunnerAutopilot.ini') -Destination $out }
    Copy-Item -LiteralPath (Join-Path $root 'native\README.md') -Destination $out
    $licenses = Join-Path $out 'licenses'
    New-Item -ItemType Directory -Path $licenses -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $imgui 'LICENSE.txt') -Destination (Join-Path $licenses 'ImGui.txt')
    Copy-Item -LiteralPath (Join-Path $minhook 'LICENSE.txt') -Destination (Join-Path $licenses 'MinHook.txt')
    Copy-Item -LiteralPath (Join-Path $root 'vendor\asi-loader-v10.0.2\LICENSE.txt') -Destination (Join-Path $licenses 'Ultimate-ASI-Loader.txt')
    Write-Output "Built native mod: $out"
} finally { $env:PATH = $oldPath }
