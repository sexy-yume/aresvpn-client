# AresVPN Client

The VPN client for the AresVPN service - Windows and Android. A user logs in with an account id,
a password and the alias (`idx`) of one of their rents, and the client fetches that rent's
configuration from the AresVPN console and connects. WireGuard, AresWG (AmneziaWG), OpenVPN,
Shadowsocks and Socks5 rents are supported; HTTP rents are a browser/curl product and are not.

## Provenance and licence

AresVPN Client is a **fork of [Amnezia VPN](https://github.com/amnezia-vpn/amnezia-client)**,
the client Amnezia publishes under the GNU General Public License version 3. This repository is
that code plus the changes that make it this product: the branding, the AresVPN login and
endpoint, and the removal of the parts that are Amnezia's own service (the self-hosted server
installer and the Premium store). Upstream is tracked as the `upstream` remote and merged
regularly; the changes are kept small and at the seams for that reason.

- **Licence:** GNU GPL v3.0 - see [`LICENSE`](LICENSE). Every binary we distribute comes with
  its complete corresponding source at the place the binary is offered; this repository is that
  source.
- **Third-party components** and their licences: [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md).
  It describes what the Windows and Android builds actually ship, not a historical list.
- **Trademarks.** "Amnezia" and "AmneziaVPN" are marks of their owners; this product is not
  affiliated with or endorsed by Amnezia. "WireGuard" is a registered trademark of Jason A.
  Donenfeld; "OpenVPN" is a trademark of OpenVPN Inc.; "PT Root UI" is a trademark of ParaType.
  None of them is part of this product's name. The AresVPN mark and artwork in
  `branding/aresvpn/` are AresVPN's and are not covered by the GPL.
- **Copyright notices** in the source files are upstream's and their contributors' and are kept
  as the licence requires; files this fork modifies say so in the commit history, and files it
  adds carry an AresVPN notice.

## Building on Windows

Upstream's build is unchanged; the fork adds one cache preload and one driver:

    deploy\aresvpn\build_windows.cmd

which runs `cmake -C cmake/aresvpn/branding.cmake ...` with the same arguments as upstream's
`deploy\build.bat`. It needs Visual Studio 2022 (MSVC x64), Qt 6.10.x `msvc2022_64` with the
`qtremoteobjects`, `qt5compat` and `qtshadertools` modules under `C:\Qt`, Python >= 3.12 (it
creates a venv with `conan==2.28.0` itself), CMake and Ninja (Visual Studio's own are used). The
protocol backends come as Conan packages from Amnezia's artifactory where a prebuilt matches the
toolchain and are built from source where it does not. The driver's header lists the workstation
facts it works around. Output: `deploy\build\client\Release\AresVPNClient.exe` and
`deploy\build\service\server\Release\AresVPNClient-service.exe`.

Android: see upstream's `deploy/build.sh` and the AresProject notes; the Android build has not yet
been reproduced on this fork's build machine.

## What this fork changes, and where

| change | where | why it lives there |
|---|---|---|
| product identity (names, organisation, keychain, Android package for the store URL) | `cmake/aresvpn/branding.cmake` | upstream's own `CLIENT_*` branding seam - zero upstream lines |
| artwork | `branding/aresvpn/` masters + `generate.py`; rasters written in place under upstream's filenames | no `.qrc`/`.rc`/manifest line changes |
| OS namespaces (services, tunnel, pipe, IPC, credential-store tags, WFP keys, installer scripts) | small in-place edits, each marked `AresVPN Client:` | upstream's lowest-churn code |
| the self-hosted installer and the Premium store | routes cut in `client/ui/`, the update check guarded, the write-access predicate forced false, `server_scripts` not embedded | every controller stays constructed; a merge survives |
| the AresVPN login and endpoint | `client/core/controllers/aresProfile*` (in progress) | new files beside upstream's |

Amnezia's original README is in the upstream history (`git show upstream/dev:README.md`).
