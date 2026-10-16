---
sidebar: false
---

> Language: **English** | [简体中文](./download.md)

# Download

All artifacts are produced automatically by the **daily build** and distributed from the rolling nightly Release (a fixed tag `nightly`: every run clears the old assets and uploads new ones, so the assets are always the latest build).

- **Release page**: <https://github.com/cuihairu/unidict/releases/tag/nightly> (reachable from mobile browsers and the GitHub App; anonymous downloads work)
- **Same-domain mirror**: <https://cuihairu.github.io/unidict/packages.html> (build timestamps and a mirror of the full package set)

## Direct Links

| Artifact | Contents |
|---|---|
| [unidict-daily.zip](https://github.com/cuihairu/unidict/releases/download/nightly/unidict-daily.zip) | All-platform bundle + BUILD_INFO + VERIFY.md |
| [unidict-windows-x64-setup.exe](https://github.com/cuihairu/unidict/releases/download/nightly/unidict-windows-x64-setup.exe) | Windows installer (Program Files + Start menu + uninstall entry) |
| [unidict-windows-x64.zip](https://github.com/cuihairu/unidict/releases/download/nightly/unidict-windows-x64.zip) | Windows portable package (GUI + CLI) |
| [unidict-macos-arm64.zip](https://github.com/cuihairu/unidict/releases/download/nightly/unidict-macos-arm64.zip) | macOS Apple Silicon (GUI.app + CLI, run `xattr -cr` first) |
| [unidict-linux-x64.deb](https://github.com/cuihairu/unidict/releases/download/nightly/unidict-linux-x64.deb) / [.rpm](https://github.com/cuihairu/unidict/releases/download/nightly/unidict-linux-x64.rpm) | Linux x64 system packages (CLI) |
| [unidict-linux-arm64.deb](https://github.com/cuihairu/unidict/releases/download/nightly/unidict-linux-arm64.deb) / [.rpm](https://github.com/cuihairu/unidict/releases/download/nightly/unidict-linux-arm64.rpm) | Linux arm64 system packages (CLI) |
| [unidict-android.zip](https://github.com/cuihairu/unidict/releases/download/nightly/unidict-android.zip) | Android release apk (R8) |

## One-Click Install

::: code-group

```bash [Linux / macOS]
curl -fsSL https://raw.githubusercontent.com/cuihairu/unidict/main/install.sh | bash
```

```powershell [Windows]
irm https://raw.githubusercontent.com/cuihairu/unidict/main/install.ps1 | iex
```

:::

The script auto-detects the operating system and CPU architecture, downloads the matching platform package from the nightly Release, and installs it; running it again performs the upgrade. On Linux it prefers distro system packages (deb/rpm), leaving uninstalling to the package manager (`sudo apt remove unidict` / `sudo dnf remove unidict`); the Windows installer deploys to `Program Files\Unidict` with Start-menu shortcuts in place.

## Per-Platform Installation

### Windows 10/11 x64

Two ways to install:

- **Installer** (recommended): download `unidict-windows-x64-setup.exe` and double-click to install — it goes into `Program Files\Unidict` with Start-menu/desktop shortcuts and a Control Panel uninstall entry all in place; once installed, launch Unidict from the Start menu, with no console window at any point.
- **Portable package**: unzip `unidict-windows-x64.zip` and double-click `unidict_qml.exe` directly; the bundled dict.json loads automatically.

If SmartScreen blocks it, click "More info → Run anyway" (daily builds are not code-signed).

### macOS (Apple Silicon)

Unzip `unidict-macos-arm64.zip`, clear the quarantine attribute, then open:

```bash
xattr -cr unidict_qml.app && open unidict_qml.app
```

The bundle ships with a sample dictionary that loads automatically. The app is unsigned, so Gatekeeper blocks it when opened directly — run `xattr` first, without fail.

### Linux

- **System packages** (recommended; deb-based and rpm-based distros are auto-detected):

  ```bash
  sudo apt install ./unidict-linux-x64.deb    # Debian/Ubuntu
  sudo dnf install ./unidict-linux-x64.rpm    # Fedora/RHEL
  ```

  Installs to `/usr/bin/unidict_cli_std` + `/usr/share/unidict/dict.json`; uninstall with `sudo apt remove unidict`. Same story on arm64 (`unidict-linux-arm64.*`).

- **Portable package**: unzip `unidict-linux-<架构>.zip` and run `unidict_cli_std` directly (no Qt dependency).

Shipping CLI-only on Linux is an established decision: without bundled dependencies the Qt desktop app will not run on other machines (glibc/Qt version hell), so desktop users build it themselves [from source](/quickstart#源码构建).

### Android

Download `unidict-android.zip`, extract the apk, and install it (allow unknown sources as your device prompts). The first launch runs an automatic smoke test — a `M2-SMOKE-OK` in the status line means everything is fine; on the dictionary management page, import `.ifo`/`.mdx` and other dictionary files via the system file picker (SAF), and the lookup page and vocabulary cards offer one-tap read-aloud (system TTS engine).

## Look Up a Word After Installing

::: code-group

```bash [Linux / macOS]
UNIDICT_DICTS=dict.json unidict_cli_std hello
```

```powershell [Windows]
set UNIDICT_DICTS=dict.json && unidict_cli_std.exe hello
```

:::

If the output is `hello: A greeting or expression of goodwill.`, you are all set. For CLI dictionary loading and search usage see [Looking Up Words](/search); for loading your own dictionaries see [Dictionary Management](/dictionaries).
