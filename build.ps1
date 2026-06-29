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
    Join-Path $outDir "MaxB Handy Clipboard.exe"
}
$obj = Join-Path $outDir "main.obj"
$res = Join-Path $outDir "app.res"
$icon = Join-Path $outDir "app.ico"

New-Item -ItemType Directory -Force -Path $outDir | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $exe) | Out-Null

function New-RoundRectPath {
    param(
        [float]$X,
        [float]$Y,
        [float]$Width,
        [float]$Height,
        [float]$Radius
    )

    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $diameter = $Radius * 2
    $path.AddArc($X, $Y, $diameter, $diameter, 180, 90)
    $path.AddArc($X + $Width - $diameter, $Y, $diameter, $diameter, 270, 90)
    $path.AddArc($X + $Width - $diameter, $Y + $Height - $diameter, $diameter, $diameter, 0, 90)
    $path.AddArc($X, $Y + $Height - $diameter, $diameter, $diameter, 90, 90)
    $path.CloseFigure()
    return $path
}

function New-AppIcon {
    param([string]$Path)

    Add-Type -AssemblyName System.Drawing

    $sizes = @(16, 24, 32, 48, 64, 128, 256)
    $pngs = New-Object System.Collections.Generic.List[byte[]]

    foreach ($size in $sizes) {
        $bitmap = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([System.Drawing.Color]::Transparent)

        $scale = [float]$size / 130.0
        $blue = [System.Drawing.Color]::FromArgb(255, 11, 92, 173)

        $paper = New-RoundRectPath (24 * $scale) (24 * $scale) (82 * $scale) (94 * $scale) (14 * $scale)
        $paperBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)
        $paperPen = New-Object System.Drawing.Pen ($blue), ([Math]::Max(2.1, 9 * $scale))
        $graphics.FillPath($paperBrush, $paper)
        $graphics.DrawPath($paperPen, $paper)

        $clip = New-RoundRectPath (45 * $scale) (10 * $scale) (40 * $scale) (26 * $scale) (9 * $scale)
        $clipBounds = [System.Drawing.RectangleF]::new(45 * $scale, 10 * $scale, 40 * $scale, 26 * $scale)
        $clipBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush $clipBounds, ([System.Drawing.Color]::FromArgb(255, 47, 125, 225)), $blue, ([System.Drawing.Drawing2D.LinearGradientMode]::ForwardDiagonal)
        $graphics.FillPath($clipBrush, $clip)

        $linePen = New-Object System.Drawing.Pen ($blue), ([Math]::Max(1.3, 6 * $scale))
        $linePen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
        $linePen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
        $graphics.DrawLine($linePen, 40 * $scale, 58 * $scale, 90 * $scale, 58 * $scale)
        $graphics.DrawLine($linePen, 40 * $scale, 78 * $scale, 82 * $scale, 78 * $scale)
        $graphics.DrawLine($linePen, 40 * $scale, 98 * $scale, 72 * $scale, 98 * $scale)

        $stream = New-Object System.IO.MemoryStream
        $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
        $pngs.Add($stream.ToArray())

        $linePen.Dispose()
        $clipBrush.Dispose()
        $clip.Dispose()
        $paperPen.Dispose()
        $paperBrush.Dispose()
        $paper.Dispose()
        $graphics.Dispose()
        $bitmap.Dispose()
        $stream.Dispose()
    }

    $file = [System.IO.File]::Open($Path, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
    $writer = New-Object System.IO.BinaryWriter $file
    $writer.Write([UInt16]0)
    $writer.Write([UInt16]1)
    $writer.Write([UInt16]$sizes.Count)

    $offset = 6 + (16 * $sizes.Count)
    for ($i = 0; $i -lt $sizes.Count; $i++) {
        $dimension = if ($sizes[$i] -eq 256) { 0 } else { [byte]$sizes[$i] }
        $writer.Write([byte]$dimension)
        $writer.Write([byte]$dimension)
        $writer.Write([byte]0)
        $writer.Write([byte]0)
        $writer.Write([UInt16]1)
        $writer.Write([UInt16]32)
        $writer.Write([UInt32]$pngs[$i].Length)
        $writer.Write([UInt32]$offset)
        $offset += $pngs[$i].Length
    }

    foreach ($png in $pngs) {
        $writer.Write($png)
    }

    $writer.Close()
    $file.Close()
}

New-AppIcon $icon

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

Remove-Item -LiteralPath $icon -Force -ErrorAction SilentlyContinue

Write-Host "Done: $exe"
