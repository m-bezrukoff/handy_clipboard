param(
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $root "src\main.cpp"
$rc = Join-Path $root "src\app.rc"
$outDir = Join-Path $root "build"
$exe = if ($OutputPath) {
    if ([System.IO.Path]::IsPathRooted($OutputPath)) {
        $OutputPath
    } else {
        Join-Path $root $OutputPath
    }
} else {
    Join-Path $outDir "ExtendedClipboard.exe"
}
$obj = Join-Path $outDir "main.obj"
$res = Join-Path $outDir "app.res"

New-Item -ItemType Directory -Force -Path $outDir | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $exe) | Out-Null

$vswhereCandidates = @(
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
    "$env:ProgramFiles\Microsoft Visual Studio\Installer\vswhere.exe"
)

$vswhere = $vswhereCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $vswhere) {
    throw "vswhere.exe was not found. Install Visual Studio Build Tools with the C++ x64 component."
}

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    throw "Visual Studio Build Tools with C++ x64 were not found."
}

$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
    throw "vcvars64.bat was not found: $vcvars"
}

$compile = @(
    "cl",
    "/nologo",
    "/std:c++20",
    "/utf-8",
    "/O1",
    "/GL",
    "/MT",
    "/EHsc",
    "/W4",
    "/DUNICODE",
    "/D_UNICODE",
    "/Fo`"$obj`"",
    "`"$src`"",
    "`"$res`"",
    "/link",
    "/SUBSYSTEM:WINDOWS",
    "/OPT:REF",
    "/OPT:ICF",
    "/LTCG",
    "/OUT:`"$exe`"",
    "user32.lib",
    "dwmapi.lib",
    "shell32.lib",
    "gdi32.lib",
    "advapi32.lib"
) 
$compile = $compile -join " "
$resourceCompile = "rc /nologo /fo`"$res`" `"$rc`""

cmd.exe /s /c "`"$vcvars`" >nul && $resourceCompile && $compile"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Done: $exe"
