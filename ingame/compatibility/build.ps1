param([string]$Toolchain='C:\msys64\ucrt64\bin',[string]$ReferenceExe,[switch]$Package)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=$PSScriptRoot
$ingame=Split-Path $root -Parent
$repo=Split-Path $ingame -Parent
$out=Join-Path $ingame 'build\compatibility'
$minhook=Join-Path $ingame 'vendor\minhook-1.3.4'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$oldPath=$env:PATH
try{
 $env:PATH="$Toolchain;$oldPath"
 $flags=@('-std=c++17','-O2','-Wall','-Wextra','-Werror','-static')
 & "$Toolchain\g++.exe" @flags (Join-Path $root 'ResolverTests.cpp') -o (Join-Path $out 'ResolverTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Resolver test compilation failed.'}
 & (Join-Path $out 'ResolverTests.exe')
 if($LASTEXITCODE -ne 0){throw 'Resolver tests failed.'}
 $exe=Join-Path $out 'SnowRunnerCC-Compatibility.exe'
 & "$Toolchain\g++.exe" @flags -municode (Join-Path $root 'Diagnostic.cpp') -lbcrypt -lcomdlg32 -o $exe
 if($LASTEXITCODE -ne 0){throw 'Diagnostic compilation failed.'}
 $asi=Join-Path $out 'SnowRunnerCCCompatibility.asi'
 & "$Toolchain\g++.exe" @flags -shared '-Wl,--exclude-all-symbols' -DSR_CC_SIGNATURE_DIAGNOSTIC "-I$minhook\include" (Join-Path $ingame 'experimental\scaleform\Probe.cpp') -lbcrypt -o $asi
 if($LASTEXITCODE -ne 0){throw 'Diagnostic ASI compilation failed.'}
 if($ReferenceExe){
   $report=Join-Path $out ('reference-'+[guid]::NewGuid().ToString('N')+'.log')
   & $exe $ReferenceExe --output $report
   if($LASTEXITCODE -ne 0){throw "Reference scan did not validate. Report: $report"}
   Write-Output "Reference report: $report"
 }
 if($Package){
   $stage=Join-Path $out ('package-'+[guid]::NewGuid().ToString('N'))
   New-Item -ItemType Directory -Path $stage | Out-Null
   Copy-Item -LiteralPath $exe,(Join-Path $root 'README.md') -Destination $stage
   $info=@{tool='SnowRunner CC compatibility diagnostic';version='1.0.0';signatureRevision=1;sourceCommit=(git -C $repo rev-parse HEAD).Trim();sourceTreeDirty=[bool](git -C $repo status --porcelain);mode='read-only, no hooks, no AS3, no input';exeSha256=(Get-FileHash -LiteralPath $exe).Hash.ToLowerInvariant()}
   [IO.File]::WriteAllText((Join-Path $stage 'BUILD_INFO.json'),($info|ConvertTo-Json),(New-Object Text.UTF8Encoding($false)))
   $zip=Join-Path $out 'SnowRunnerCC-Compatibility-1.0.0-win-x64.zip'
   Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -Force
   $hash=(Get-FileHash -LiteralPath $zip).Hash.ToLowerInvariant()
   [IO.File]::WriteAllText((Join-Path $out 'SHA256SUMS.txt'),"$hash  $([IO.Path]::GetFileName($zip))`n",(New-Object Text.UTF8Encoding($false)))
   Write-Output "Tester package: $zip"
 }
 Write-Output "Read-only diagnostic EXE: $exe"
 Write-Output "Optional read-only diagnostic ASI (not in tester ZIP): $asi"
}finally{$env:PATH=$oldPath}
