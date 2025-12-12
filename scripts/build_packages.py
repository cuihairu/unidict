#!/usr/bin/env python3
"""
Unidict 跨平台安装包构建脚本
支持 Windows、macOS、 Linux 的安装包创建

使用方法:
  python scripts/build_packages.py [选项]

选项:
  --platform [all|windows|macos|linux]  构建平台 (默认: all)
  --type [all|installer|portable]      包类型 (默认: all)
  --version [版本号]                  版本号 (默认: git tag 或 latest)
  --clean                            构建前清理
  --help                             显示帮助信息
"""

import os
import sys
import shutil
import subprocess
import argparse
import hashlib
import json
import time
from pathlib import Path
from typing import Dict, List, Optional

# 项目配置
PROJECT_ROOT = Path(__file__).parent.parent
BUILD_ROOT = PROJECT_ROOT / "build"
PACKAGES_ROOT = PROJECT_ROOT / "packages"
DIST_ROOT = PROJECT_ROOT / "dist"

class PackageBuilder:
    def __init__(self):
        self.version = self.get_version()
        self.git_commit = self.get_git_commit()
        self.build_date = time.strftime("%Y%m%d")

    def get_version(self) -> str:
        """获取版本号"""
        try:
            # 尝试从git tag获取
            result = subprocess.run(
                ["git", "describe", "--tags", "--abbrev=0"],
                cwd=PROJECT_ROOT,
                capture_output=True,
                text=True
            )
            if result.returncode == 0:
                return result.stdout.strip()
        except:
            pass

        return "latest"

    def get_git_commit(self) -> str:
        """获取git commit hash"""
        try:
            result = subprocess.run(
                ["git", "rev-parse", "--short", "HEAD"],
                cwd=PROJECT_ROOT,
                capture_output=True,
                text=True
            )
            if result.returncode == 0:
                return result.stdout.strip()
        except:
            pass

        return "unknown"

    def run_command(self, cmd: List[str], cwd: Optional[Path] = None) -> bool:
        """运行命令并返回成功状态"""
        print(f"执行: {' '.join(cmd)}")
        work_dir = cwd or PROJECT_ROOT

        result = subprocess.run(
            cmd,
            cwd=work_dir,
            capture_output=True,
            text=True
        )

        if result.returncode != 0:
            print(f"错误: 命令失败")
            print(f"输出: {result.stdout}")
            print(f"错误: {result.stderr}")
            return False

        return True

    def create_directories(self):
        """创建必要的目录"""
        for directory in [BUILD_ROOT, PACKAGES_ROOT, DIST_ROOT]:
            directory.mkdir(parents=True, exist_ok=True)

    def clean_build(self):
        """清理构建目录"""
        print("清理构建目录...")
        if BUILD_ROOT.exists():
            shutil.rmtree(BUILD_ROOT)
        if PACKAGES_ROOT.exists():
            shutil.rmtree(PACKAGES_ROOT)
        self.create_directories()

    def build_project(self, platform: str):
        """构建项目"""
        print(f"为 {platform} 构建项目...")

        # 设置平台特定的CMake选项
        cmake_options = [
            "-DCMAKE_BUILD_TYPE=Release",
            "-DUNIDICT_BUILD_QT_CORE=ON",
            "-DUNIDICT_BUILD_ADAPTER_QT=ON",
            "-DUNIDICT_BUILD_QT_APPS=ON",
            "-DUNIDICT_BUILD_QT_TESTS=OFF",
            "-DUNIDICT_BUILD_STD_CLI=ON"
        ]

        if platform == "windows":
            cmake_options.extend([
                "-DCMAKE_GENERATOR=Visual Studio 17 2022",
                "-DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake",
                "-DVCPKG_TARGET_TRIPLET=x64-windows"
            ])
        elif platform == "macos":
            cmake_options.extend([
                "-DCMAKE_OSX_DEPLOYMENT_TARGET=10.15",
                "-DCMAKE_PREFIX_PATH=/usr/local"
            ])
        elif platform == "linux":
            cmake_options.extend([
                "-DCMAKE_PREFIX_PATH=/usr/local"
            ])

        # 配置CMake
        build_dir = BUILD_ROOT / f"build-{platform}"
        build_dir.mkdir(parents=True, exist_ok=True)

        if not self.run_command(["cmake", ".."] + cmake_options, cwd=build_dir):
            return False

        # 构建项目
        if platform == "windows":
            if not self.run_command(["cmake", "--build", ".", "--config", "Release"], cwd=build_dir):
                return False
        else:
            if not self.run_command(["cmake", "--build", ".", "-j", str(os.cpu_count())], cwd=build_dir):
                return False

        return True

    def create_windows_installer(self) -> bool:
        """创建Windows安装包"""
        print("创建Windows安装包...")

        build_dir = BUILD_ROOT / "build-windows"

        # 创建安装包结构
        package_dir = PACKAGES_ROOT / "windows"
        install_dir = package_dir / "Unidict"
        install_dir.mkdir(parents=True, exist_ok=True)

        # 复制可执行文件
        executables = ["unidict_cli.exe", "unidict_qml.exe"]
        for exe in executables:
            src = build_dir / "Release" / exe
            if src.exists():
                shutil.copy2(src, install_dir)
                print(f"  复制: {exe}")

        # 复制DLL文件
        release_dir = build_dir / "Release"
        for dll in release_dir.glob("*.dll"):
            shutil.copy2(dll, install_dir)
            print(f"  复制: {dll.name}")

        # 创建Qt部署
        if not self.run_command(["windeployqt", "--release", str(install_dir)]):
            print("警告: windeployqt失败，可能缺少运行时库")

        # 创建安装程序脚本
        installer_script = package_dir / "install.nsi"
        self.create_windows_nsis_script(installer_script)

        # 运行NSIS创建安装包
        if self.run_command(["makensis", "/V4", str(installer_script)]):
            package_file = DIST_ROOT / f"Unidict-{self.version}-Windows.exe"
            package_file.parent.mkdir(parents=True, exist_ok=True)

            # 查找生成的安装包
            for file in package_dir.glob("*.exe"):
                if "installer" in file.name.lower():
                    shutil.copy2(file, package_file)
                    print(f"  创建安装包: {package_file}")
                    break
            return True
        return False

    def create_windows_nsis_script(self, script_path: Path):
        """创建NSIS安装脚本"""
        script_content = f'''
; Unidict Windows Installer Script
; Generated by build_packages.py

!define APP_NAME "Unidict"
!define APP_VERSION "{self.version}"
!define APP_PUBLISHER "Unidict Team"
!define APP_URL "https://github.com/unidict/unidict"
!define APP_EXECUTABLE "unidict_qml.exe"

; Include modern UI
!include "MUI2.nsh"

; General settings
Name "${{APP_NAME}}"
OutFile "Unidict-${{APP_VERSION}}-installer.exe"
InstallDir "$PROGRAMFILES64\\${{APP_NAME}}"
InstallDirRegKey "HKLM\\Software\\${{APP_NAME}}" "InstallPath"
RequestExecutionLevel admin

; Interface settings
!define MUI_ABORTWARNING
!define MUI_ICON "${{APP_EXECUTABLE}}"
!define MUI_UNICON "${{APP_EXECUTABLE}}"

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Languages
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "SimpChinese"

; Installer sections
Section "Core Files" SecCore
    SectionIn RO

    SetOutPath "$INSTDIR"

    ; Main application files
    File /r "Unidict\\*.exe"
    File /r "Unidict\\*.dll"
    File /r "Unidict\\platforms"
    File /r "Unidict\\imageformats"
    File /r "Unidict\\styles"

    ; Create start menu shortcuts
    CreateDirectory "$SMPROGRAMS\\${{APP_NAME}}"
    CreateShortCut "$SMPROGRAMS\\${{APP_NAME}}\\${{APP_NAME}}.lnk" "$INSTDIR\\${{APP_EXECUTABLE}}"

    ; Create desktop shortcut
    CreateShortCut "$DESKTOP\\${{APP_NAME}}.lnk" "$INSTDIR\\${{APP_EXECUTABLE}}"

    ; Register file associations
    WriteRegStr HKCR ".mdx" "" "Unidict.MDict"
    WriteRegStr HKCR "Unidict.MDict" "" "MDict Dictionary File"
    WriteRegStr HKCR "Unidict.MDict\\DefaultIcon" "" "$INSTDIR\\unidict_qml.exe,0"
    WriteRegStr HKCR "Unidict.MDict\\shell\\open\\command" "" '"$INSTDIR\\unidict_qml.exe" "%1"'

    WriteRegStr HKCR ".ifo" "" "Unidict.StarDict"
    WriteRegStr HKCR "Unidict.StarDict" "" "StarDict Dictionary File"
    WriteRegStr HKCR "Unidict.StarDict\\DefaultIcon" "" "$INSTDIR\\unidict_qml.exe,0"
    WriteRegStr HKCR "Unidict.StarDict\\shell\\open\\command" "" '"$INSTDIR\\unidict_qml.exe" "%1"'
SectionEnd

; Uninstaller section
Section "Uninstall"
    Delete "$INSTDIR\\*.exe"
    Delete "$INSTDIR\\*.dll"
    RMDir /r "$INSTDIR\\platforms"
    RMDir /r "$INSTDIR\\imageformats"
    RMDir /r "$INSTDIR\\styles"
    RMDir "$INSTDIR"

    Delete "$SMPROGRAMS\\${{APP_NAME}}\\${{APP_NAME}}.lnk"
    RMDir "$SMPROGRAMS\\${{APP_NAME}}"
    Delete "$DESKTOP\\${{APP_NAME}}.lnk"

    DeleteRegKey HKLM "Software\\${{APP_NAME}}"
    DeleteRegKey HKCR ".mdx"
    DeleteRegKey HKCR "Unidict.MDict"
    DeleteRegKey HKCR ".ifo"
    DeleteRegKey HKCR "Unidict.StarDict"
SectionEnd
'''
        script_path.write_text(script_content, encoding='utf-8')

    def create_macos_package(self) -> bool:
        """创建macOS安装包"""
        print("创建macOS安装包...")

        build_dir = BUILD_ROOT / "build-macos"

        # 创建应用包结构
        package_dir = PACKAGES_ROOT / "macos"
        app_dir = package_dir / "Unidict.app"
        contents_dir = app_dir / "Contents"
        resources_dir = contents_dir / "Resources"
        macos_dir = contents_dir / "MacOS"
        frameworks_dir = contents_dir / "Frameworks"

        for directory in [resources_dir, macos_dir, frameworks_dir]:
            directory.mkdir(parents=True, exist_ok=True)

        # 复制可执行文件
        executables = ["unidict_qml", "unidict_cli"]
        for exe in executables:
            src = build_dir / exe
            if src.exists():
                shutil.copy2(src, macos_dir)
                print(f"  复制: {exe}")

        # 创建Info.plist
        info_plist = self.create_macos_info_plist()
        (contents_dir / "Info.plist").write_text(info_plist, encoding='utf-8')

        # 复制图标
        icon_source = PROJECT_ROOT / "assets" / "icon.icns"
        if icon_source.exists():
            shutil.copy2(icon_source, resources_dir / "AppIcon.icns")

        # 运行macdeployqt
        if not self.run_command(["macdeployqt", str(app_dir)]):
            print("警告: macdeployqt失败，可能缺少运行时库")

        # 创建DMG
        dmg_name = f"Unidict-{self.version}-macOS.dmg"
        dmg_file = DIST_ROOT / dmg_name

        if not self.run_command([
            "hdiutil", "create",
            "-volname", "Unidict",
            "-srcfolder", str(package_dir),
            "-ov", str(dmg_file)
        ]):
            return False

        print(f"  创建DMG: {dmg_file}")
        return True

    def create_macos_info_plist(self) -> str:
        """创建macOS Info.plist"""
        return f'''<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDisplayName</key>
    <string>Unidict</string>
    <key>CFBundleExecutable</key>
    <string>unidict_qml</string>
    <key>CFBundleIconFile</key>
    <string>AppIcon.icns</string>
    <key>CFBundleIdentifier</key>
    <string>com.unidict.app</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>Unidict</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>{self.version}</string>
    <key>CFBundleVersion</key>
    <string>{self.version}</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.15</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSSupportsAutomaticGraphicsSwitching</key>
    <true/>
    <key>CFBundleDocumentTypes</key>
    <array>
        <dict>
            <key>CFBundleTypeExtensions</key>
            <array>
                <string>mdx</string>
                <string>mdd</string>
            </array>
            <key>CFBundleTypeName</key>
            <string>MDict Dictionary File</string>
            <key>CFBundleTypeRole</key>
            <string>Viewer</string>
            <key>LSHandlerRank</key>
            <string>Alternate</string>
        </dict>
        <dict>
            <key>CFBundleTypeExtensions</key>
            <array>
                <string>ifo</string>
                <string>dict</string>
                <string>idx</string>
            </array>
            <key>CFBundleTypeName</key>
            <string>StarDict Dictionary File</string>
            <key>CFBundleTypeRole</key>
            <string>Viewer</string>
            <key>LSHandlerRank</key>
            <string>Alternate</string>
        </dict>
    </array>
</dict>
</plist>'''

    def create_linux_packages(self) -> bool:
        """创建Linux安装包"""
        print("创建Linux安装包...")

        build_dir = BUILD_ROOT / "build-linux"

        # 创建AppImage
        if not self.create_linux_appimage(build_dir):
            return False

        # 创建DEB包
        if not self.create_linux_deb(build_dir):
            return False

        # 创建RPM包
        if not self.create_linux_rpm(build_dir):
            return False

        return True

    def create_linux_appimage(self, build_dir: Path) -> bool:
        """创建Linux AppImage"""
        print("  创建AppImage...")

        appdir = PACKAGES_ROOT / "linux" / "Unidict.AppDir"
        appdir.mkdir(parents=True, exist_ok=True)

        # 复制可执行文件
        executables = ["unidict_qml", "unidict_cli"]
        for exe in executables:
            src = build_dir / exe
            if src.exists():
                shutil.copy2(src, appdir / "usr" / "bin")
                print(f"    复制: {exe}")

        # 创建桌面文件
        desktop_entry = self.create_linux_desktop_entry()
        (appdir / "usr" / "share" / "applications" / "unidict.desktop").write_text(desktop_entry)

        # 复制图标
        icon_sizes = [16, 32, 48, 64, 128, 256, 512]
        icons_dir = appdir / "usr" / "share" / "icons" / "hicolor"

        for size in icon_sizes:
            icon_dir = icons_dir / f"{size}x{size}" / "apps"
            icon_dir.mkdir(parents=True, exist_ok=True)

        # 创建AppRun脚本
        apprun_content = '''#!/bin/sh
HERE="$(dirname "$(readlink -f "${0}")"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH}"
export PATH="${HERE}/usr/bin:${PATH}"
export QT_PLUGIN_PATH="${HERE}/usr/plugins"
exec "${HERE}/usr/bin/unidict_qml" "$@"
'''
        (appdir / "AppRun").write_text(apprun_content)
        os.chmod(appdir / "AppRun", 0o755)

        # 使用appimagetool创建AppImage
        appimage_file = DIST_ROOT / f"Unidict-{self.version}-x86_64.AppImage"

        if not self.run_command([
            "appimagetool", "--appimage-extract-and-run",
            str(appdir),
            str(appimage_file)
        ]):
            print("  警告: appimagetool失败，手动创建AppImage")
            return False

        print(f"    创建AppImage: {appimage_file}")
        return True

    def create_linux_deb(self, build_dir: Path) -> bool:
        """创建DEB包"""
        print("  创建DEB包...")

        package_name = f"unidict_{self.version.replace('.', '_')}_amd64"
        deb_dir = PACKAGES_ROOT / "linux" / "deb"
        deb_build_dir = deb_dir / package_name

        # 创建DEB目录结构
        directories = [
            deb_build_dir / "DEBIAN",
            deb_build_dir / "usr" / "bin",
            deb_build_dir / "usr" / "share" / "applications",
            deb_build_dir / "usr" / "share" / "doc" / "unidict",
            deb_build_dir / "usr" / "share" / "man" / "man1"
        ]

        for directory in directories:
            directory.mkdir(parents=True, exist_ok=True)

        # 复制可执行文件
        executables = ["unidict_qml", "unidict_cli"]
        for exe in executables:
            src = build_dir / exe
            if src.exists():
                shutil.copy2(src, deb_build_dir / "usr" / "bin")

        # 创建控制文件
        control_content = f'''Package: unidict
Version: {self.version}
Section: utils
Priority: optional
Architecture: amd64
Depends: libqt6core6, libqt6gui6, libqt6qml6, libqt6quick6, libqt6network6, zlib1g
Maintainer: Unidict Team <team@unidict.org>
Description: Universal dictionary lookup tool
 Unidict is a powerful, cross-platform dictionary application
 supporting multiple formats including MDict, StarDict, DSL, and more.
 It features fast search, AI integration, and vocabulary management.
Homepage: https://github.com/unidict/unidict
'''

        (deb_build_dir / "DEBIAN" / "control").write_text(control_content)

        # 创建man页面
        man_content = self.create_man_page()
        (deb_build_dir / "usr" / "share" / "man" / "man1" / "unidict_qml.1").write_text(man_content)
        (deb_build_dir / "usr" / "share" / "man" / "man1" / "unidict_cli.1").write_text(man_content)

        # 复制文档
        doc_files = ["README.md", "LICENSE", "CONTRIBUTING.md"]
        for doc in doc_files:
            src = PROJECT_ROOT / doc
            if src.exists():
                shutil.copy2(src, deb_build_dir / "usr" / "share" / "doc" / "unidict")

        # 创建桌面文件
        desktop_content = self.create_linux_desktop_entry()
        (deb_build_dir / "usr" / "share" / "applications" / "unidict.desktop").write_text(desktop_content)

        # 构建DEB包
        deb_file = DIST_ROOT / f"unidict_{self.version}_amd64.deb"

        if not self.run_command([
            "dpkg-deb", "--build", str(deb_build_dir), str(deb_file)
        ]):
            print("  警告: dpkg-deb失败")
            return False

        print(f"    创建DEB: {deb_file}")
        return True

    def create_linux_rpm(self, build_dir: Path) -> bool:
        """创建RPM包"""
        print("  创建RPM包...")

        # 创建RPM spec文件
        spec_content = f'''Name: unidict
Version: {self.version}
Release: 1%{{?dist}}
Summary: Universal dictionary lookup tool
License: MIT
URL: https://github.com/unidict/unidict

BuildRequires: cmake, qt6-base-devel, zlib-devel
Requires: qt6-qtbase, qt6-qtdeclarative, zlib

%description
Unidict is a powerful, cross-platform dictionary application
supporting multiple formats including MDict, StarDict, DSL, and more.
It features fast search, AI integration, and vocabulary management.

%prep
%autosetup -n

%build
%cmake
%cmake_build

%install
rm -rf %{{buildroot}}
mkdir -p %{{buildroot}}%{{_prefix}}/bin
mkdir -p %{{buildroot}}%{{_datadir}}/applications
mkdir -p %{{buildroot}}%{{_mandir}}/man1

install -m 755 unidict_qml %{{buildroot}}%{{_prefix}}/bin/
install -m 755 unidict_cli %{{buildroot}}%{{_prefix}}/bin/
install -m 644 unidict.desktop %{{buildroot}}%{{_datadir}}/applications/
install -m 644 unidict_qml.1 %{{buildroot}}%{{_mandir}}/man1/
install -m 644 unidict_cli.1 %{{buildroot}}%{{_mandir}}/man1/

%files
%{{_prefix}}/bin/unidict_qml
%{{_prefix}}/bin/unidict_cli
%{{_datadir}}/applications/unidict.desktop
%{{_mandir}}/man1/unidict_qml.1
%{{_mandir}}/man1/unidict_cli.1
%doc README.md LICENSE

%changelog
* {time.strftime('%Y-%m-%d')} Unidict Team <team@unidict.org> - {self.version}
- Initial release
'''

        spec_path = PACKAGES_ROOT / "linux" / "unidict.spec"
        spec_path.write_text(spec_content, encoding='utf-8')

        # 构建RPM包
        rpm_file = DIST_ROOT / f"unidict-{self.version}-1.x86_64.rpm"

        if not self.run_command([
            "rpmbuild", "-bb", str(spec_path), "--target", "x86_64"
        ]):
            print("  警告: rpmbuild失败")
            return False

        # 查找生成的RPM文件
        for rpm in Path.home().glob("rpmbuild/RPMS/x86_64/*.rpm"):
            shutil.copy2(rpm, rpm_file)
            print(f"    创建RPM: {rpm_file}")
            return True

        return False

    def create_linux_desktop_entry(self) -> str:
        """创建Linux桌面文件"""
        return '''[Desktop Entry]
Name=Unidict
Comment=Universal dictionary lookup tool
GenericName=Dictionary
Exec=unidict_qml %F
Icon=unidict
Type=Application
Categories=Education;Office;Dictionary;
MimeType=application/x-mdict;application/x-stardict;
StartupWMClass=Unidict
StartupNotify=true
'''

    def create_man_page(self) -> str:
        """创建man页面"""
        return f'''.TH UNIDICT_QML 1 "November 2024" "Unidict {self.version}" "User Commands"

.SH NAME
unidict_qml \- Universal dictionary lookup tool with graphical interface

.SH SYNOPSIS
.B unidict_qml
.RI [options] [dictionary_files...]

.SH DESCRIPTION
Unidict is a cross-platform dictionary application supporting multiple formats
including MDict (.mdx/.mdd), StarDict (.ifo/.idx/.dict), DSL, and JSON.

.SH OPTIONS
.TP
.B \-d, \-\-dict <path>
Load dictionary file(s). Multiple dictionaries can be specified.
.TP
.B \-h, \-\-help
Show help information.

.SH ENVIRONMENT
.TP
.B UNIDICT_DICTS
Colon-separated list of dictionary file paths.

.SH FILES
.TP
.I ~/.local/share/unidict/
User data directory for dictionaries, vocabulary, and cache.
.TP
.I /usr/share/unidict/
System-wide dictionary directory.

.SH EXAMPLES
.TP
.B unidict_qml
Start the application with default settings.
.TP
.B unidict_qml \-d /path/to/dict.mdx
Start with a specific dictionary loaded.
.TP
.B UNIDICT_DICTS="/path/to/dict1.mdx:/path/to/dict2.ifo" unidict_qml
Load multiple dictionaries via environment variable.

.SH AUTHOR
Unidict Team <team@unidict.org>

.SH SEE ALSO
.BR unidict_cli(1), unidict_cli_std(1)

Full documentation at: <https://github.com/unidict/unidict>
'''

    def create_checksums(self) -> Dict[str, str]:
        """创建文件校验和"""
        print("创建校验和...")

        checksums = {}
        for package_file in DIST_ROOT.glob("*"):
            if package_file.is_file():
                sha256 = hashlib.sha256()
                with open(package_file, 'rb') as f:
                    for chunk in iter(lambda: f.read(8192), b''):
                        sha256.update(chunk)

                checksums[package_file.name] = sha256.hexdigest()
                print(f"  {package_file.name}: {checksums[package_file.name]}")

        # 保存校验和文件
        checksums_file = DIST_ROOT / f"checksums-{self.version}.txt"
        with open(checksums_file, 'w') as f:
            for filename, checksum in checksums.items():
                f.write(f"{checksum}  {filename}\\n")

        return checksums

    def build_all(self, platforms: List[str], package_types: List[str]):
        """构建所有指定的包"""
        success = True

        for platform in platforms:
            if platform not in ["windows", "macos", "linux"]:
                print(f"错误: 不支持的平台 '{platform}'")
                success = False
                continue

            # 构建项目
            if not self.build_project(platform):
                print(f"错误: {platform} 平台构建失败")
                success = False
                continue

            # 创建安装包
            if "installer" in package_types:
                if platform == "windows":
                    success &= self.create_windows_installer()
                elif platform == "macos":
                    success &= self.create_macos_package()
                elif platform == "linux":
                    success &= self.create_linux_packages()

        # 创建校验和
        if success and DIST_ROOT.exists():
            checksums = self.create_checksums()

            # 保存构建信息
            build_info = {
                "version": self.version,
                "git_commit": self.git_commit,
                "build_date": self.build_date,
                "platforms": platforms,
                "package_types": package_types,
                "checksums": checksums
            }

            with open(DIST_ROOT / "build-info.json", 'w') as f:
                json.dump(build_info, f, indent=2)

        return success

def main():
    parser = argparse.ArgumentParser(
        description="Unidict 跨平台安装包构建脚本",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  %(prog)s                          构建所有平台的安装包
  %(prog)s --platform windows      只构建Windows安装包
  %(prog)s --type portable          只构建便携版
  %(prog)s --version 1.0.0          指定版本号
  %(prog)s --clean                  构建前清理
"""
    )

    parser.add_argument(
        "--platform",
        choices=["all", "windows", "macos", "linux"],
        default="all",
        help="构建平台 (默认: all)"
    )

    parser.add_argument(
        "--type",
        choices=["all", "installer", "portable"],
        default="all",
        help="包类型 (默认: all)"
    )

    parser.add_argument(
        "--version",
        help="版本号 (默认: git tag 或 latest)"
    )

    parser.add_argument(
        "--clean",
        action="store_true",
        help="构建前清理"
    )

    parser.add_argument(
        "--help-detailed",
        action="store_true",
        help="显示详细帮助信息"
    )

    args = parser.parse_args()

    if args.help_detailed:
        print(__doc__)
        return 0

    # 设置版本号
    builder = PackageBuilder()
    if args.version:
        builder.version = args.version

    print(f"Unidict 包构建器")
    print(f"版本: {builder.version}")
    print(f"Git提交: {builder.git_commit}")
    print(f"构建日期: {builder.build_date}")
    print()

    # 清理构建目录
    if args.clean:
        builder.clean_build()

    # 确定要构建的平台
    platforms = []
    if args.platform == "all":
        platforms = ["windows", "macos", "linux"]
    else:
        platforms = [args.platform]

    # 确定包类型
    package_types = []
    if args.type == "all":
        package_types = ["installer", "portable"]
    else:
        package_types = [args.type]

    # 执行构建
    success = builder.build_all(platforms, package_types)

    if success:
        print()
        print("🎉 构建成功!")
        print(f"安装包已创建在: {DIST_ROOT}")

        # 显示创建的文件
        if DIST_ROOT.exists():
            print("\\n创建的文件:")
            for file in sorted(DIST_ROOT.glob("*")):
                size_mb = file.stat().st_size / (1024 * 1024)
                print(f"  {file.name} ({size_mb:.1f} MB)")
    else:
        print("❌ 构建失败!")
        return 1

    return 0

if __name__ == "__main__":
    sys.exit(main())