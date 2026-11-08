$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = $PSScriptRoot
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path -LiteralPath $compiler)) {
    throw '.NET Framework 4.x compiler was not found.'
}
$outputDir = Join-Path $projectRoot 'dist'
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
$buildDir = Join-Path $projectRoot 'build'
$assetsDir = Join-Path $projectRoot 'assets'
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
$version = [IO.File]::ReadAllText((Join-Path $projectRoot 'VERSION')).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw 'Invalid standalone VERSION.' }
$assemblyInfo = Join-Path $buildDir 'AssemblyInfo.cs'
$metadata = @"
using System.Reflection;
[assembly: AssemblyTitle("SnowRunner Cruise control — standalone $version")]
[assembly: AssemblyProduct("SnowRunner Cruise control (SnowRunner CC)")]
[assembly: AssemblyDescription("SnowRunner CC standalone $version")]
[assembly: AssemblyVersion("$version.0")]
[assembly: AssemblyFileVersion("$version.0")]
"@
[IO.File]::WriteAllText($assemblyInfo, $metadata, (New-Object Text.UTF8Encoding($false)))
& $compiler /nologo /target:exe /reference:System.Drawing.dll "/out:$buildDir\GenerateIcons.exe" (Join-Path $assetsDir 'GenerateIcons.cs')
if ($LASTEXITCODE -ne 0) { throw 'Icon generator compilation failed.' }
& (Join-Path $buildDir 'GenerateIcons.exe') $assetsDir
if ($LASTEXITCODE -ne 0) { throw 'Icon generation failed.' }
$sourceFiles = @(
    $assemblyInfo,
    (Join-Path $projectRoot 'src\CruiseController.cs'),
    (Join-Path $projectRoot 'src\KeyboardPolicy.cs'),
    (Join-Path $projectRoot 'src\Native.cs'),
    (Join-Path $projectRoot 'src\Program.cs')
)
& $compiler /nologo /utf8output /codepage:65001 /target:winexe /platform:x64 /optimize+ /warnaserror+ /reference:System.Windows.Forms.dll /reference:System.Drawing.dll `
    "/win32icon:$assetsDir\cruise-control.ico" `
    "/resource:$assetsDir\cruise-control.ico,SnowRunnerCruise.Icons.Off.ico" `
    "/resource:$assetsDir\cruise-control-on.ico,SnowRunnerCruise.Icons.On.ico" `
    "/resource:$assetsDir\cruise-control-error.ico,SnowRunnerCruise.Icons.Error.ico" `
    "/out:$outputDir\SnowRunnerCruiseControl.exe" $sourceFiles
if ($LASTEXITCODE -ne 0) { throw 'Application compilation failed.' }
& $compiler /nologo /utf8output /target:exe /platform:x64 /warnaserror+ "/out:$buildDir\CruiseControllerTests.exe" (Join-Path $projectRoot 'src\CruiseController.cs') (Join-Path $projectRoot 'src\KeyboardPolicy.cs') (Join-Path $projectRoot 'tests\CruiseControllerTests.cs')
if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed.' }
& (Join-Path $buildDir 'CruiseControllerTests.exe')
if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
if (-not (Test-Path -LiteralPath (Join-Path $outputDir 'CruiseControl.ini'))) {
    Copy-Item -LiteralPath (Join-Path $projectRoot 'CruiseControl.ini') -Destination $outputDir
}
$packageReadme = [IO.File]::ReadAllText((Join-Path $projectRoot 'README.md')).Replace('![Иконка утилиты](assets/cruise-control.png)', '')
[IO.File]::WriteAllText((Join-Path $outputDir 'README.md'), $packageReadme, (New-Object Text.UTF8Encoding($false)))
Write-Output "Built: $outputDir\SnowRunnerCruiseControl.exe"
