# Generates app.ico and app_disabled.ico using System.Drawing
# Run: powershell -ExecutionPolicy Bypass -File gen_icons.ps1
Add-Type -AssemblyName System.Drawing

function Make-Icon {
    param([string]$OutPath, [System.Drawing.Color]$BgColor, [System.Drawing.Color]$GridColor)

    $sizes = @(16, 32, 48)
    $bitmaps = @()
    foreach ($sz in $sizes) {
        $bmp = New-Object System.Drawing.Bitmap($sz, $sz)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias

        # Background
        $g.Clear($BgColor)

        # Draw a 3x4 grid of small rounded rects to suggest a numpad
        $margin = [int]($sz * 0.08)
        $cols = 3; $rows = 4
        $cellW = ($sz - $margin * 2) / $cols
        $cellH = ($sz - $margin * 2) / $rows
        $pad = [Math]::Max(1, [int]($sz * 0.04))
        $radius = [Math]::Max(1, [int]($sz * 0.06))

        $brush = New-Object System.Drawing.SolidBrush($GridColor)
        for ($r = 0; $r -lt $rows; $r++) {
            for ($c = 0; $c -lt $cols; $c++) {
                $x = $margin + $c * $cellW + $pad
                $y = $margin + $r * $cellH + $pad
                $w = $cellW - $pad * 2
                $h = $cellH - $pad * 2
                if ($w -gt 1 -and $h -gt 1) {
                    $g.FillRectangle($brush, $x, $y, $w, $h)
                }
            }
        }
        $brush.Dispose()
        $g.Dispose()
        $bitmaps += $bmp
    }

    # Write ICO file manually
    $stream = New-Object System.IO.MemoryStream
    $writer = New-Object System.IO.BinaryWriter($stream)

    $count = $bitmaps.Count

    # ICO header
    $writer.Write([uint16]0)      # reserved
    $writer.Write([uint16]1)      # type: ICO
    $writer.Write([uint16]$count) # count

    # Calculate data offsets
    # Header = 6 bytes, directory entries = count * 16 bytes
    $dirOffset = 6 + $count * 16
    $pngStreams = @()
    foreach ($bmp in $bitmaps) {
        $ms = New-Object System.IO.MemoryStream
        $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
        $pngStreams += $ms
    }

    # Write directory entries
    $offset = $dirOffset
    for ($i = 0; $i -lt $count; $i++) {
        $sz = $sizes[$i]
        $szByte = if ($sz -ge 256) { 0 } else { $sz }
        $writer.Write([byte]$szByte)    # width
        $writer.Write([byte]$szByte)    # height
        $writer.Write([byte]0)          # colour count
        $writer.Write([byte]0)          # reserved
        $writer.Write([uint16]1)        # planes
        $writer.Write([uint16]32)       # bit count
        $writer.Write([uint32]$pngStreams[$i].Length)  # size
        $writer.Write([uint32]$offset)                 # offset
        $offset += $pngStreams[$i].Length
    }

    # Write PNG data
    foreach ($ps in $pngStreams) {
        $writer.Write($ps.ToArray())
        $ps.Dispose()
    }

    [System.IO.File]::WriteAllBytes($OutPath, $stream.ToArray())
    $writer.Dispose()
    $stream.Dispose()
    foreach ($bmp in $bitmaps) { $bmp.Dispose() }
    Write-Host "Written: $OutPath"
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Make-Icon `
    -OutPath (Join-Path $scriptDir "app.ico") `
    -BgColor ([System.Drawing.Color]::FromArgb(255, 0, 90, 180)) `
    -GridColor ([System.Drawing.Color]::FromArgb(255, 200, 230, 255))

Make-Icon `
    -OutPath (Join-Path $scriptDir "app_disabled.ico") `
    -BgColor ([System.Drawing.Color]::FromArgb(255, 100, 100, 100)) `
    -GridColor ([System.Drawing.Color]::FromArgb(255, 180, 180, 180))

Write-Host "Icons generated."
