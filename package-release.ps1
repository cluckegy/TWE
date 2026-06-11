$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$exportRoot = Join-Path $root "Export"
$sourceOut = Join-Path $exportRoot "TWE-Source"
$finalOut = Join-Path $exportRoot "TWE-Final"
$runtimeOut = Join-Path $exportRoot "TWE-Runtime"
$qtBin = "D:\Qt\5.15.2\msvc2019_64\bin"

foreach ($path in @($sourceOut, $finalOut, $runtimeOut)) {
    $resolvedRoot = [IO.Path]::GetFullPath($exportRoot)
    $resolvedPath = [IO.Path]::GetFullPath($path)
    if (-not $resolvedPath.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean path outside Export: $resolvedPath"
    }
    if (Test-Path -LiteralPath $resolvedPath) {
        Remove-Item -LiteralPath $resolvedPath -Recurse -Force
    }
    New-Item -ItemType Directory -Path $resolvedPath | Out-Null
}

# Minimal runtime package installed into AppData\LocalLow\CL\TWE.
Copy-Item (Join-Path $root "release\TWE.exe") $runtimeOut
& (Join-Path $qtBin "windeployqt.exe") `
    --release `
    --no-translations `
    --no-system-d3d-compiler `
    --no-opengl-sw `
    --no-angle `
    --compiler-runtime `
    --dir $runtimeOut `
    (Join-Path $runtimeOut "TWE.exe") | Out-Null

Copy-Item (Join-Path $root "release\libssl-1_1-x64.dll") $runtimeOut
Copy-Item (Join-Path $root "release\libcrypto-1_1-x64.dll") $runtimeOut

$redistRoot = Get-ChildItem "D:\vs\VC\Redist\MSVC" -Directory |
    Where-Object { $_.Name -match '^\d+\.' } |
    Sort-Object Name -Descending |
    Select-Object -First 1
if (-not $redistRoot) {
    throw "MSVC redistributable directory was not found."
}
$crtDirectory = Join-Path $redistRoot.FullName "x64\Microsoft.VC145.CRT"
foreach ($runtimeDll in @("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll")) {
    Copy-Item (Join-Path $crtDirectory $runtimeDll) $runtimeOut
}

$allowedRuntimeFiles = @(
    "TWE.exe",
    "Qt5Core.dll",
    "Qt5Gui.dll",
    "Qt5Network.dll",
    "Qt5Widgets.dll",
    "libssl-1_1-x64.dll",
    "libcrypto-1_1-x64.dll",
    "msvcp140.dll",
    "vcruntime140.dll",
    "vcruntime140_1.dll"
)

Get-ChildItem -LiteralPath $runtimeOut -File | Where-Object {
    $_.Name -notin $allowedRuntimeFiles
} | Remove-Item -Force

Get-ChildItem -LiteralPath $runtimeOut -Directory | Where-Object {
    $_.Name -ne "platforms"
} | Remove-Item -Recurse -Force

Get-ChildItem -LiteralPath (Join-Path $runtimeOut "platforms") -File | Where-Object {
    $_.Name -ne "qwindows.dll"
} | Remove-Item -Force

$runtimeZip = Join-Path $finalOut "TWE-runtime.zip"
Compress-Archive -Path (Join-Path $runtimeOut "*") -DestinationPath $runtimeZip -CompressionLevel Optimal

# The public download contains only the native launcher.
Copy-Item (Join-Path $root "launcher\x64\Release\TWE-Launcher.exe") `
          (Join-Path $finalOut "TWE.exe")

@"
TWE Release Files
=================

Give users: TWE.exe

Upload TWE-runtime.zip to a GitHub Release using this exact asset name.
The launcher downloads the latest release asset and installs it into:
%USERPROFILE%\AppData\LocalLow\CL\TWE
"@ | Set-Content -LiteralPath (Join-Path $finalOut "UPLOAD-INSTRUCTIONS.txt") -Encoding UTF8

# Clean source package.
foreach ($directory in @("src", "launcher", "tests")) {
    New-Item -ItemType Directory -Path (Join-Path $sourceOut $directory) -Force | Out-Null
}

Copy-Item (Join-Path $root "src\*.cpp") (Join-Path $sourceOut "src")
Copy-Item (Join-Path $root "src\*.h") (Join-Path $sourceOut "src")
New-Item -ItemType Directory -Path (Join-Path $sourceOut "src\resources") | Out-Null
foreach ($resource in @(
    "fa-solid-900.ttf",
    "twe-logo.png",
    "twe.ico",
    "down_arrow.png",
    "down_arrow_hover.png"
)) {
    Copy-Item (Join-Path $root "src\resources\$resource") `
              (Join-Path $sourceOut "src\resources")
}

Copy-Item (Join-Path $root "launcher\launcher.cpp") (Join-Path $sourceOut "launcher")
Copy-Item (Join-Path $root "launcher\launcher.rc") (Join-Path $sourceOut "launcher")
Copy-Item (Join-Path $root "launcher\TWE-Launcher.vcxproj") (Join-Path $sourceOut "launcher")
Copy-Item (Join-Path $root "launcher\TWE-Launcher.sln") (Join-Path $sourceOut "launcher")

Copy-Item (Join-Path $root "tests\*.cpp") (Join-Path $sourceOut "tests")
Copy-Item (Join-Path $root "tests\*.pro") (Join-Path $sourceOut "tests")
Copy-Item (Join-Path $root "TWE.pro") $sourceOut
Copy-Item (Join-Path $root "resources.qrc") $sourceOut
Copy-Item (Join-Path $root "README.md") $sourceOut
Copy-Item (Join-Path $root "package-release.ps1") $sourceOut

New-Item -ItemType Directory -Path (Join-Path $sourceOut "npcap-sdk\Include") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $sourceOut "npcap-sdk\Lib\x64") -Force | Out-Null
Copy-Item (Join-Path $root "npcap-sdk\Include\*") `
          (Join-Path $sourceOut "npcap-sdk\Include") -Recurse
Copy-Item (Join-Path $root "npcap-sdk\Lib\x64\Packet.lib") `
          (Join-Path $sourceOut "npcap-sdk\Lib\x64")
Copy-Item (Join-Path $root "npcap-sdk\Lib\x64\wpcap.lib") `
          (Join-Path $sourceOut "npcap-sdk\Lib\x64")

Push-Location $sourceOut
try {
    & (Join-Path $qtBin "qmake.exe") -tp vc -o TWE.vcxproj TWE.pro
} finally {
    Pop-Location
}

$projectFile = Join-Path $sourceOut "TWE.vcxproj"
$projectXml = Get-Content -LiteralPath $projectFile -Raw
$projectXml = $projectXml.Replace(
    "{95A5E925-4724-30A9-99AA-C3D53C41CB3D}",
    "{A5407877-3887-4BA1-BE9C-E6CECEB3410A}")
$projectXml = $projectXml.Replace(
    "<PlatformToolset>v142</PlatformToolset>",
    "<PlatformToolset>v145</PlatformToolset>")
$projectXml = $projectXml.Replace(
    $sourceOut + "\",
    '$(ProjectDir)')
Set-Content -LiteralPath $projectFile -Value $projectXml -Encoding UTF8

foreach ($generatedDirectory in @("debug", "release")) {
    $generatedPath = Join-Path $sourceOut $generatedDirectory
    if (Test-Path -LiteralPath $generatedPath) {
        Remove-Item -LiteralPath $generatedPath -Recurse -Force
    }
}

@"
Microsoft Visual Studio Solution File, Format Version 12.00
# Visual Studio Version 17
VisualStudioVersion = 17.0.31903.59
MinimumVisualStudioVersion = 10.0.40219.1
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "TWE", "TWE.vcxproj", "{A5407877-3887-4BA1-BE9C-E6CECEB3410A}"
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "TWE-Launcher", "launcher\TWE-Launcher.vcxproj", "{0F755247-B449-44EB-A026-8A0B6BC49560}"
EndProject
Global
    GlobalSection(SolutionConfigurationPlatforms) = preSolution
        Release|x64 = Release|x64
    EndGlobalSection
    GlobalSection(ProjectConfigurationPlatforms) = postSolution
        {A5407877-3887-4BA1-BE9C-E6CECEB3410A}.Release|x64.ActiveCfg = Release|x64
        {A5407877-3887-4BA1-BE9C-E6CECEB3410A}.Release|x64.Build.0 = Release|x64
        {0F755247-B449-44EB-A026-8A0B6BC49560}.Release|x64.ActiveCfg = Release|x64
        {0F755247-B449-44EB-A026-8A0B6BC49560}.Release|x64.Build.0 = Release|x64
    EndGlobalSection
EndGlobal
"@ | Set-Content -LiteralPath (Join-Path $sourceOut "TWE.sln") -Encoding ASCII

$sourceZip = Join-Path $exportRoot "TWE-Source.zip"
if (Test-Path -LiteralPath $sourceZip) {
    Remove-Item -LiteralPath $sourceZip -Force
}
Compress-Archive -Path (Join-Path $sourceOut "*") -DestinationPath $sourceZip -CompressionLevel Optimal

if (Test-Path -LiteralPath $runtimeOut) {
    Remove-Item -LiteralPath $runtimeOut -Recurse -Force
}

Write-Host "Created:"
Write-Host "  $sourceOut"
Write-Host "  $sourceZip"
Write-Host "  $finalOut"
Write-Host "  $runtimeZip"
