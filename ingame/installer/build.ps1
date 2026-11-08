param([switch]$RebuildHud)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=Split-Path $PSScriptRoot -Parent
$repo=Split-Path $root -Parent
if($RebuildHud){& (Join-Path $root 'experimental\scaleform\build.ps1') -Autopilot; if($LASTEXITCODE -ne 0){throw 'HUD build failed.'}}
$version=[IO.File]::ReadAllText((Join-Path $root 'VERSION')).Trim()
if($version -notmatch '^\d+\.\d+\.\d+$'){throw 'Invalid HUD version.'}
$out=Join-Path $root "build\installer\$version"
New-Item -ItemType Directory -Path $out -Force | Out-Null
$compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if(-not(Test-Path -LiteralPath $compiler)){throw '.NET Framework 4.x compiler is required.'}
$versionHeader=Join-Path $root 'build\scaleform-probe\SnowRunnerCCVersion.h'
if([IO.File]::ReadAllText($versionHeader) -notmatch ('"'+[regex]::Escape($version)+'"')){throw 'HUD binary version differs. Rebuild HUD first.'}
$files=[ordered]@{
 'SnowRunnerNativeAutopilot.asi'=(Join-Path $root 'build\scaleform-probe\SnowRunnerNativeAutopilot.asi')
 'SnowRunnerNativeAutopilot.ini'=(Join-Path $root 'experimental\scaleform\SnowRunnerNativeAutopilot.ini')
 'dinput8.dll'=(Join-Path $root 'vendor\asi-loader-v10.0.2\dinput8.dll')
}
if((Get-FileHash -LiteralPath $files['dinput8.dll']).Hash -ne '57EC56390C92A18B82F58855E63C93985E254D1E29D5F75CBC28DC08F96C8A12'){throw 'Loader checksum mismatch.'}
$hashes=@{}
foreach($entry in $files.GetEnumerator()){$hashes[$entry.Key]=(Get-FileHash -LiteralPath $entry.Value).Hash.ToLowerInvariant()}
$entries=@($hashes.GetEnumerator()|ForEach-Object{'{ "'+$_.Key+'", "'+$_.Value+'" }'}) -join ",`r`n"
$metadata=@"
using System.Collections.Generic;
using System.Reflection;
[assembly: AssemblyTitle("SnowRunner CC HUD $version Setup")]
[assembly: AssemblyProduct("SnowRunner Cruise control (SnowRunner CC)")]
[assembly: AssemblyVersion("$version.0")]
[assembly: AssemblyFileVersion("$version.0")]
namespace SnowRunnerCC.Setup {
internal static class BuildInfo {
internal const string Version="$version";
internal const string SupportedExe="a3cb442ec138eb2eb7e335e5c80e376766550740e49e233ac9eace035e182285";
internal static readonly Dictionary<string,string> Hashes=new Dictionary<string,string> { $entries };
}}
"@
$metadataPath=Join-Path $out 'BuildInfo.cs'
[IO.File]::WriteAllText($metadataPath,$metadata,(New-Object Text.UTF8Encoding($false)))
$testExe=Join-Path $out 'InstallerTests.exe'
& $compiler /nologo /utf8output /codepage:65001 /platform:x64 /target:exe /warnaserror+ /reference:System.Web.Extensions.dll "/out:$testExe" (Join-Path $PSScriptRoot 'InstallerCore.cs') (Join-Path $PSScriptRoot 'InstallerTests.cs')
if($LASTEXITCODE -ne 0){throw 'Installer test compilation failed.'}
& $testExe (Join-Path $out ('test-'+[guid]::NewGuid().ToString('N')))
if($LASTEXITCODE -ne 0){throw 'Installer tests failed.'}
$exe=Join-Path $out "SnowRunnerCC-HUD-$version-Setup.exe"
$resources=@($files.GetEnumerator()|ForEach-Object{"/resource:$($_.Value),SnowRunnerCC.$($_.Key)"})
$extras=[ordered]@{
 'app.ico'=(Join-Path $PSScriptRoot 'assets\app.ico')
 'INSTALL_NATIVE.md'=(Join-Path $root 'INSTALL_NATIVE.md')
 'MODIO_DESCRIPTION_EN.md'=(Join-Path $root 'MODIO_DESCRIPTION_EN.md')
 'MinHook.txt'=(Join-Path $root 'vendor\minhook-1.3.4\LICENSE.txt')
 'Ultimate-ASI-Loader.txt'=(Join-Path $root 'vendor\asi-loader-v10.0.2\LICENSE.txt')
}
$resources+=@($extras.GetEnumerator()|ForEach-Object{"/resource:$($_.Value),SnowRunnerCC.$($_.Key)"})
& $compiler /nologo /utf8output /codepage:65001 /platform:x64 /target:winexe /optimize+ /warnaserror+ /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Web.Extensions.dll "/win32icon:$($extras['app.ico'])" "/win32manifest:$(Join-Path $PSScriptRoot 'app.manifest')" "/out:$exe" @resources $metadataPath (Join-Path $PSScriptRoot 'InstallerCore.cs') (Join-Path $PSScriptRoot 'Program.cs')
if($LASTEXITCODE -ne 0){throw 'Installer compilation failed.'}
$smoke=Start-Process -FilePath $exe -ArgumentList '--verify-package' -WindowStyle Hidden -Wait -PassThru
if($smoke.ExitCode -ne 0){throw 'Embedded payload verification failed.'}
$info=@{product='SnowRunner Cruise control';shortName='SnowRunner CC';variant='HUD';version=$version;sourceCommit=(git -C $repo rev-parse HEAD).Trim();sourceTreeDirty=[bool](git -C $repo status --porcelain);payloadSha256=$hashes;installerSha256=(Get-FileHash -LiteralPath $exe).Hash.ToLowerInvariant()}
[IO.File]::WriteAllText((Join-Path $out 'BUILD_INFO.json'),($info|ConvertTo-Json -Depth 4),(New-Object Text.UTF8Encoding($false)))
Copy-Item -LiteralPath (Join-Path $root 'INSTALL_NATIVE.md'),(Join-Path $root 'MODIO_DESCRIPTION_EN.md') -Destination $out
$licenses=Join-Path $out 'licenses'
New-Item -ItemType Directory -Path $licenses -Force | Out-Null
Copy-Item -LiteralPath $extras['MinHook.txt'] -Destination (Join-Path $licenses 'MinHook.txt')
Copy-Item -LiteralPath $extras['Ultimate-ASI-Loader.txt'] -Destination (Join-Path $licenses 'Ultimate-ASI-Loader.txt')
$package=Join-Path $out "SnowRunnerCC-HUD-$version-Setup.zip"
$packageFiles=@($exe,(Join-Path $out 'INSTALL_NATIVE.md'),(Join-Path $out 'MODIO_DESCRIPTION_EN.md'),(Join-Path $out 'BUILD_INFO.json'),$licenses)
Compress-Archive -LiteralPath $packageFiles -DestinationPath $package -Force
$sums=@($exe,$package)|ForEach-Object{(Get-FileHash -LiteralPath $_).Hash.ToLowerInvariant()+'  '+[IO.Path]::GetFileName($_)}
[IO.File]::WriteAllText((Join-Path $out 'SHA256SUMS.txt'),(($sums -join "`n")+"`n"),(New-Object Text.UTF8Encoding($false)))
Write-Output "Built installer: $exe"
Write-Output "Upload archive: $package"
