# Unidict one-shot installer (Windows x64).
# NOTE: keep this file pure ASCII. Windows PowerShell 5.1 reads remote
# scripts piped into iex using the system ANSI codepage; non-ASCII
# characters would be mangled and break parsing.
#
# Usage (one line):
#   irm https://raw.githubusercontent.com/cuihairu/unidict/main/install.ps1 | iex
#
# Downloads the daily-build Windows x64 package from the GitHub Pages
# mirror (anonymous download, no GitHub login needed), unpacks it to
# %LOCALAPPDATA%\Programs\Unidict, adds it to the user PATH (idempotent,
# re-run = upgrade), then smoke-tests:
#   UNIDICT_DICTS=dict.json unidict_cli_std.exe hello
# The CLI has no --version flag; the hello smoke test is the verification.
# ARM64 Windows: the matrix does not ship an arm64 package; the x64
# package generally runs via the built-in x64 emulation, but it is
# unsupported -- reported clearly below instead of failing silently.

$ErrorActionPreference = 'Stop'

$RepoUrl = 'https://github.com/cuihairu/unidict'
# Rolling nightly Release asset URL: public repo assets download without login.
$BaseUrl = "$RepoUrl/releases/download/nightly"
$PkgName = 'unidict-windows-x64.zip'

function Fail($msg) {
    Write-Host "ERROR: $msg" -ForegroundColor Red
    exit 1
}

# ---------- architecture ----------
$arch = $env:PROCESSOR_ARCHITECTURE
if ($arch -eq 'AMD64') {
    # supported
} elseif ($arch -eq 'ARM64') {
    Fail "CPU architecture $arch: the daily build has no native Windows ARM64 package. The x64 package may run via Windows' built-in x64 emulation (unsupported). If you need native ARM64 builds, please open an issue at $RepoUrl/issues with this message."
} elseif ($arch -eq 'x86') {
    Fail "CPU architecture $arch (32-bit): only x64 builds are provided."
} else {
    Fail "Unsupported CPU architecture: $arch (supported: AMD64; ARM64 see note; x86 not provided)."
}

# ---------- dependencies ----------
try { [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12 } catch {}

# ---------- download ----------
$TmpDir = Join-Path $env:TEMP ("unidict-install-" + [guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $TmpDir | Out-Null
try {
    $PkgPath = Join-Path $TmpDir $PkgName
    Write-Host "==> Downloading $BaseUrl/$PkgName ..."
    try {
        Invoke-WebRequest -Uri "$BaseUrl/$PkgName" -OutFile $PkgPath -UseBasicParsing
    } catch {
        Fail "Download failed ($($_.Exception.Message)). Check network access to $BaseUrl, or whether today's build has been deployed (daily at 05:17 Beijing time). Manual download: $RepoUrl/actions/workflows/daily-build.yml"
    }
    if (-not (Test-Path $PkgPath) -or (Get-Item $PkgPath).Length -lt 1MB) {
        Fail "Downloaded package is missing or suspiciously small. Retrying later or manual download from $RepoUrl/actions/workflows/daily-build.yml"
    }

    # ---------- install ----------
    $InstallDir = Join-Path $env:LOCALAPPDATA 'Programs\Unidict'
    Write-Host "==> Installing to $InstallDir ..."
    if (Test-Path $InstallDir) {
        try { Remove-Item -Recurse -Force $InstallDir }
        catch { Fail "Cannot replace previous install at $InstallDir ($($_.Exception.Message)). Close running Unidict (unidict_gui.exe) and retry." }
    }
    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
    try {
        Expand-Archive -Path $PkgPath -DestinationPath $InstallDir -Force
    } catch {
        Fail "Unpack failed ($($_.Exception.Message)). The package may be corrupt; retry or download manually."
    }
    $Cli = Join-Path $InstallDir 'unidict_cli_std.exe'
    if (-not (Test-Path $Cli)) {
        Fail "unidict_cli_std.exe not found in package (incomplete package?)."
    }

    # ---------- PATH (user scope, idempotent) ----------
    $UserPath = [Environment]::GetEnvironmentVariable('Path', 'User')
    if (($UserPath -split ';') -notcontains $InstallDir) {
        [Environment]::SetEnvironmentVariable('Path', ($UserPath.TrimEnd(';') + ';' + $InstallDir), 'User')
        Write-Host "==> Added to user PATH: $InstallDir"
    } else {
        Write-Host "==> PATH already contains $InstallDir (re-run = upgrade, files replaced)"
    }
    $env:Path = "$env:Path;$InstallDir"

    # ---------- smoke test ----------
    Write-Host '==> Smoke test: UNIDICT_DICTS=dict.json unidict_cli_std hello'
    $env:UNIDICT_DICTS = Join-Path $InstallDir 'dict.json'
    $out = & $Cli hello 2>&1
    if ($LASTEXITCODE -eq 0 -and ($out -match 'greeting')) {
        Write-Host "==> OK: $out" -ForegroundColor Green
    } else {
        Fail "Smoke test failed (output: $out). Please report at $RepoUrl/issues"
    }

    Write-Host ''
    Write-Host 'Done. Open a NEW terminal so the updated PATH takes effect, then run:'
    Write-Host '  unidict_qml.exe          # GUI'
    Write-Host '  unidict_cli_std.exe hello  # CLI (UNIDICT_DICTS must point to a dictionary)'
    Write-Host "Uninstall: delete $InstallDir and remove it from user PATH."
} finally {
    if (Test-Path $TmpDir) { Remove-Item -Recurse -Force $TmpDir -ErrorAction SilentlyContinue }
}
