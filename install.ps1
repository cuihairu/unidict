# Unidict one-shot installer (Windows x64).
# NOTE: keep this file pure ASCII. Windows PowerShell 5.1 reads remote
# scripts piped into iex using the system ANSI codepage; non-ASCII
# characters would be mangled and break parsing.
#
# Usage (one line):
#   irm https://raw.githubusercontent.com/cuihairu/unidict/main/install.ps1 | iex
#
# Downloads the Windows installer (unidict-windows-x64-setup.exe, Inno
# Setup) from the rolling nightly Release (anonymous download, no GitHub
# login needed) and runs it silently:
#   - installs to Program Files\Unidict
#   - Start menu shortcuts (GUI + CLI) and an Add/Remove Programs
#     uninstaller entry
#   - optional desktop icon (interactive setup only)
# The GUI main program unidict_qml.exe is the WIN32 (GUI subsystem)
# build: launching it shows no console window. The bundled dict.json next
# to the exe is auto-loaded when UNIDICT_DICTS is unset.
# UAC: the installer requests elevation itself (a UAC prompt appears in
# interactive sessions; re-run = upgrade, files are replaced in place).
# The CLI has no --version flag; the hello smoke test is the verification.
# ARM64 Windows: the matrix does not ship an arm64 installer; the x64
# package generally runs via the built-in x64 emulation, but it is
# unsupported -- reported clearly below instead of failing silently.

$ErrorActionPreference = 'Stop'

$RepoUrl = 'https://github.com/cuihairu/unidict'
# Rolling nightly Release asset URL: public repo assets download without login.
$SetupName = 'unidict-windows-x64-setup.exe'
$BaseUrl = "$RepoUrl/releases/download/nightly"

function Fail($msg) {
    Write-Host "ERROR: $msg" -ForegroundColor Red
    exit 1
}

# ---------- architecture ----------
$arch = $env:PROCESSOR_ARCHITECTURE
if ($arch -eq 'AMD64') {
    # supported
} elseif ($arch -eq 'ARM64') {
    Fail "CPU architecture $arch: the daily build has no native Windows ARM64 installer. The x64 installer may run via Windows' built-in x64 emulation (unsupported). If you need native ARM64 builds, please open an issue at $RepoUrl/issues with this message."
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
    $SetupPath = Join-Path $TmpDir $SetupName
    Write-Host "==> Downloading $BaseUrl/$SetupName ..."
    try {
        Invoke-WebRequest -Uri "$BaseUrl/$SetupName" -OutFile $SetupPath -UseBasicParsing
    } catch {
        Fail "Download failed ($($_.Exception.Message)). Check network access to $BaseUrl, or whether today's build has been deployed (daily at 05:17 Beijing time). Manual download: $RepoUrl/releases/tag/nightly"
    }
    if (-not (Test-Path $SetupPath) -or (Get-Item $SetupPath).Length -lt 1MB) {
        Fail "Downloaded installer is missing or suspiciously small. Retry later or download manually from $RepoUrl/releases/tag/nightly"
    }

    # ---------- silent install ----------
    Write-Host "==> Installing (silent; accept the UAC prompt) ..."
    $p = Start-Process -Wait -PassThru -FilePath $SetupPath -ArgumentList '/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART'
    if ($p.ExitCode -ne 0) {
        Fail "Installer exited with code $($p.ExitCode). Re-run interactively (double-click the installer) to see the error."
    }

    # ---------- verify install ----------
    $app = Join-Path $env:ProgramFiles 'Unidict'
    foreach ($f in @('unidict_qml.exe', 'unidict_cli_std.exe', 'dict.json', 'unins000.exe')) {
        if (-not (Test-Path (Join-Path $app $f))) {
            Fail "$f not found in $app (install incomplete?)."
        }
    }
    $lnk = Join-Path $env:ProgramData 'Microsoft\Windows\Start Menu\Programs\Unidict\Unidict.lnk'
    if (-not (Test-Path $lnk)) {
        Fail "Start menu shortcut not found: $lnk"
    }

    # ---------- PATH (user scope, idempotent) ----------
    $UserPath = [Environment]::GetEnvironmentVariable('Path', 'User')
    if (($UserPath -split ';') -notcontains $app) {
        [Environment]::SetEnvironmentVariable('Path', ($UserPath.TrimEnd(';') + ';' + $app), 'User')
        Write-Host "==> Added to user PATH: $app"
    } else {
        Write-Host "==> PATH already contains $app (re-run = upgrade, files replaced)"
    }
    $env:Path = "$env:Path;$app"

    # ---------- smoke test ----------
    Write-Host '==> Smoke test: unidict_cli_std hello (dict.json from install dir)'
    $env:UNIDICT_DICTS = Join-Path $app 'dict.json'
    $out = & (Join-Path $app 'unidict_cli_std.exe') hello 2>&1
    if ($LASTEXITCODE -eq 0 -and ($out -match 'greeting')) {
        Write-Host "==> OK: $out" -ForegroundColor Green
    } else {
        Fail "Smoke test failed (output: $out). Please report at $RepoUrl/issues"
    }

    Write-Host ''
    Write-Host 'Done. Launch from the Start menu (Unidict), or:'
    Write-Host '  unidict_qml.exe            # GUI (no console window)'
    Write-Host '  unidict_cli_std.exe hello  # CLI (UNIDICT_DICTS must point to a dictionary)'
    Write-Host "Uninstall: Apps & Features (Add/Remove Programs) -> Unidict, or run $app\unins000.exe"
} finally {
    if (Test-Path $TmpDir) { Remove-Item -Recurse -Force $TmpDir -ErrorAction SilentlyContinue }
}
