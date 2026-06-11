# TWE

[![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![Qt](https://img.shields.io/badge/Qt-5.15.2-41CD52?style=for-the-badge&logo=qt&logoColor=white)](https://www.qt.io/)
[![Windows](https://img.shields.io/badge/Windows-10%2B-0078D6?style=for-the-badge&logo=windows&logoColor=white)](https://www.microsoft.com/windows)
[![Platform](https://img.shields.io/badge/Platform-x64-173D36?style=for-the-badge)](#requirements)
[![Latest Release](https://img.shields.io/github/v/release/cluckegy/TWE?style=for-the-badge&logo=github&color=277F6C)](https://github.com/cluckegy/TWE/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/cluckegy/TWE/total?style=for-the-badge&logo=github&color=0EA5A8)](https://github.com/cluckegy/TWE/releases)
[![License](https://img.shields.io/github/license/cluckegy/TWE?style=for-the-badge&color=64748B)](https://github.com/cluckegy/TWE/blob/main/LICENSE)
[![Discord](https://img.shields.io/badge/Discord-Join%20Community-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/YrtTNQwFrH)

TWE is a lightweight Windows desktop tool for monitoring WE internet quota
usage and managing devices connected to your local network.

Developed by **Mohamed Wael (MoGlitch)** and sponsored by **CodeLuck**.

> Thank you for downloading TWE. Support for more networks and additional
> features is coming soon.

## Tech Stack

| Technology | Usage |
| --- | --- |
| C++17 | Core application and native launcher |
| Qt 5.15.2 | Desktop interface, networking, and application services |
| Win32 API | Lightweight installer and first-run downloader |
| Npcap | Network packet handling and device control |
| OpenSSL | Secure HTTPS communication |
| Visual Studio | Windows x64 build toolchain |

## Features

- View remaining and total WE quota.
- Monitor quota usage and renewal dates.
- Scan devices connected to the local network.
- Search and filter detected devices.
- Apply download and upload speed limits.
- Block or restore internet access for selected devices.
- Automatic quota refresh.
- Secure local credential storage using Windows DPAPI.
- Single-instance behavior: opening TWE again restores the running window.
- Small launcher that downloads runtime files only on first launch.

## Download

1. Open the [latest release](https://github.com/cluckegy/TWE/releases/latest).
2. Download `TWE.exe`.
3. Run it and wait for the first-time installation to finish.

The launcher downloads the required runtime package and installs it into:

```text
%USERPROFILE%\AppData\LocalLow\CL\TWE
```

Future launches open the installed application directly.

## Network Control Requirements

Device scanning works with standard Windows networking APIs.

Speed limiting and blocking features require
[Npcap](https://npcap.com/#download) to be installed. Run TWE with suitable
permissions when using network control features.

Only use network control features on networks and devices you own or are
authorized to administer.

## Build From Source

### Requirements

- Windows 10 or newer, x64
- Visual Studio with Desktop development with C++
- Qt 5.15.2 MSVC x64
- Npcap SDK
- OpenSSL 1.1 x64 runtime

### Visual Studio

Open:

```text
TWE.sln
```

Select `Release | x64`, then build the solution.

The solution contains:

- `TWE`: the main Qt desktop application.
- `TWE-Launcher`: the native first-run downloader.

### qmake

```bat
D:\Qt\5.15.2\msvc2019_64\bin\qmake.exe TWE.pro
nmake /f Makefile.Release
```

Paths may need to be adjusted to match your Qt installation.

## Create Distribution Files

Run the packaging script from PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\package-release.ps1
```

It creates:

```text
Export/
|-- TWE-Final/
|   |-- TWE.exe
|   |-- TWE-runtime.zip
|   `-- UPLOAD-INSTRUCTIONS.txt
|-- TWE-Source/
`-- TWE-Source.zip
```

## Publish a GitHub Release

1. Push the source code to this repository.
2. Create a new GitHub Release, for example `v1.0.0`.
3. Upload `Export/TWE-Final/TWE-runtime.zip`.
4. Keep the asset name exactly `TWE-runtime.zip`.
5. Upload `Export/TWE-Final/TWE.exe` for users.

The launcher downloads:

```text
https://github.com/cluckegy/TWE/releases/latest/download/TWE-runtime.zip
```

## Privacy

- The landline number is stored locally.
- The password and session data are protected using Windows DPAPI.
- Protected credentials can only be decrypted by the same Windows user.
- TWE communicates with WE services to authenticate and retrieve quota data.

## Community

- [GitHub](https://github.com/cluckegy/TWE)
- [Discord](https://discord.gg/YrtTNQwFrH)

## License

Review the repository license before using, modifying, or redistributing TWE.
