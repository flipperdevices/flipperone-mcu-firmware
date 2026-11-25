# Set up the Flipper One MCU toolchain pinned in toolchain.json, from PowerShell:
#
#   . .\scripts\toolchain\fbtenv.ps1
#
# If scripts are blocked:
#   powershell -ExecutionPolicy Bypass -NoExit -File .\scripts\toolchain\fbtenv.ps1
#
# The first time, it downloads the windows-x64 release archive, checks its sha256
# against toolchain.json and unpacks it into
# $env:FLIPPERONE_TOOLCHAIN_HOME (default ~\.flipperone-mcu-toolchain)\<tag>\.
# Then, and on every later run, it dot-sources that bundle's own fbtenv.ps1, which
# puts the tools first on PATH and sets PICO_SDK_PATH and the rest, and points the
# `current` junction beside it at <tag> for IDE configs. Running it again is harmless.
#
# Offline: set $env:FLIPPERONE_TOOLCHAIN_ARCHIVE to an already downloaded archive. It
# is checked against toolchain.json all the same.

$_foBundle = & {
    $ErrorActionPreference = 'Stop'
    $ProgressPreference = 'SilentlyContinue'  # Invoke-WebRequest is many times slower with it

    if (-not ($IsWindows -or $env:OS -eq 'Windows_NT')) {
        throw 'fbtenv.ps1 is for Windows; on Linux and macOS source scripts/toolchain/fbtenv.sh'
    }
    $pin = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot '..\..\toolchain.json') | ConvertFrom-Json
    $tag = $pin.toolchain
    $archive = $pin.archives.'windows-x64'
    if ($tag -notmatch '^v[1-9][0-9]*$' -or -not $archive.name -or $archive.sha256 -notmatch '^[0-9a-f]{64}$') {
        throw 'fbtenv.ps1: toolchain.json has no usable windows-x64 archive'
    }

    $home_ = if ($env:FLIPPERONE_TOOLCHAIN_HOME) { $env:FLIPPERONE_TOOLCHAIN_HOME }
             else { Join-Path $HOME '.flipperone-mcu-toolchain' }
    $dir = Join-Path $home_ $tag
    $marker = Join-Path $dir '.archive-sha256'
    $installed = if (Test-Path -LiteralPath $marker) { (Get-Content -LiteralPath $marker -TotalCount 1).Trim() }

    if ($installed -ne $archive.sha256) {
        New-Item -ItemType Directory -Force -Path $home_ | Out-Null
        $local = [bool]$env:FLIPPERONE_TOOLCHAIN_ARCHIVE
        if ($local) {
            $download = $env:FLIPPERONE_TOOLCHAIN_ARCHIVE
        } else {
            $download = Join-Path $home_ "$($archive.name).$PID.part"
            $url = "https://github.com/$($pin.repository)/releases/download/$tag/$($archive.name)"
            Write-Host "fbtenv.ps1: downloading $url"
            try {
                Invoke-WebRequest -Uri $url -OutFile $download -UseBasicParsing
            } catch {
                Remove-Item -LiteralPath $download -Force -ErrorAction SilentlyContinue
                throw
            }
        }

        $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $download).Hash.ToLowerInvariant()
        if ($actual -ne $archive.sha256) {
            if (-not $local) { Remove-Item -LiteralPath $download -Force }
            throw "fbtenv.ps1: $download has sha256 $actual, toolchain.json expects $($archive.sha256)"
        }

        # Unpack beside the final place and rename, so an interrupted run leaves no
        # half-unpacked bundle behind under the real name. The bsdtar that ships with
        # Windows reads zip, and is much faster than Expand-Archive.
        Write-Host "fbtenv.ps1: unpacking $($archive.name) into $dir"
        $partial = Join-Path $home_ ".$tag.partial.$PID"
        Remove-Item -LiteralPath $partial -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Path $partial | Out-Null
        & (Join-Path $env:SystemRoot 'System32\tar.exe') -xf $download -C $partial | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Remove-Item -LiteralPath $partial -Recurse -Force
            throw "fbtenv.ps1: tar failed to unpack $download"
        }
        Set-Content -LiteralPath (Join-Path $partial '.archive-sha256') -Value $archive.sha256 -Encoding ascii
        if (-not $local) { Remove-Item -LiteralPath $download -Force }
        # Another session may have installed it meanwhile: keep that copy
        if ((Test-Path -LiteralPath $marker) -and
            (Get-Content -LiteralPath $marker -TotalCount 1).Trim() -eq $archive.sha256) {
            Remove-Item -LiteralPath $partial -Recurse -Force
        } else {
            if (Test-Path -LiteralPath $dir) { Remove-Item -LiteralPath $dir -Recurse -Force }
            Move-Item -LiteralPath $partial -Destination $dir
            # Moved into a copy installed meanwhile
            Remove-Item -LiteralPath (Join-Path $dir (Split-Path -Leaf $partial)) -Recurse -Force -ErrorAction SilentlyContinue
        }
    }

    # A junction needs no admin rights or developer mode, unlike a symlink
    $current = Join-Path $home_ 'current'
    try {
        $link = Get-Item -LiteralPath $current -Force -ErrorAction SilentlyContinue
        if ($link -and -not ($link.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            Write-Warning "fbtenv.ps1: $current is not a link, left alone"
        } elseif (-not $link -or @($link.Target)[0] -ne $dir) {
            if ($link) { [IO.Directory]::Delete($current) }  # the link only, not its target
            New-Item -ItemType Junction -Path $current -Target $dir | Out-Null
        }
    } catch {
        Write-Warning "fbtenv.ps1: cannot point $current at ${dir}: $_"
    }
    $dir
}

if ($_foBundle) {
    . (Join-Path $_foBundle 'fbtenv.ps1')
    if ($env:FLIPPERONE_TOOLCHAIN -ne (Split-Path -Leaf $_foBundle)) {
        Write-Error "fbtenv.ps1: $_foBundle is toolchain $env:FLIPPERONE_TOOLCHAIN, expected $(Split-Path -Leaf $_foBundle)"
    }
}
Remove-Variable _foBundle
