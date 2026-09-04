# Third-party components in AresVPN Client

This file lists what the **Windows** and **Android** builds of AresVPN Client actually ship or
link, with each component's licence as read from its source tree or package. It replaces the
list inherited from upstream, which described a different set (it omitted OpenVPN 2.7, the
Windows tunnel DLL, the Xray wrappers, the fonts and icons, and named components no build
contains). Where a licence text has not yet been read from the component itself, the row says
so - such a row is a task, not a licence. Upstream's own copyright and this product's licence are
in [`LICENSE`](LICENSE) (GNU GPL v3.0) and [`README.md`](README.md).

Full licence texts that must accompany a distributed build live under [`licenses/`](licenses/)
and are installed beside the binaries (the installer's licence page shows the GPL first and
these after it).

## The application

| component | licence | notes |
|---|---|---|
| Amnezia VPN client (this fork's upstream), (c) 2020-2026 AmneziaVPN and contributors | GPL-3.0 | `LICENSE`; the parts of the tree derived from Mozilla VPN carry an MPL-2.0 header (see below) |
| Mozilla VPN daemon and platform code (120 files under `client/daemon`, `client/mozilla`, `client/platforms/*/daemon`, `client/ui/utils`), (c) Mozilla | MPL-2.0 | each file keeps its header and stays MPL-2.0 inside this GPL-3.0 work (MPL section 3.3); the source is this repository |
| Qv2ray serialization (`client/core/utils/serialization/*`), (c) Qv2ray Workgroup, modified by AmneziaVPN 2024 | GPL-3.0 | 7 files; notices kept |
| Private Internet Access firewall code (`client/platforms/{linux,macos}/daemon/*firewall*`) | GPL-3.0 | not in the Windows or Android build |
| Qt 6 (Core, Gui, Network, Xml, RemoteObjects, Quick, Svg, QuickControls2, Core5Compat, Concurrent, Widgets; DBus in the service) | LGPL-3.0 (open-source edition) | shipped as shared libraries, relinkable; Qt's Android Java bindings ship in the APK under the same terms |
| QtService (Qt Solutions), (c) 2013 Digia | BSD-3-Clause | compiled into the Windows service; notice required in binary form |
| qtkeychain | BSD-3-Clause | submodule |
| SortFilterProxyModel (mitchcurtis fork, (c) Pierre-Yves Siret) | MIT | submodule |
| qrcodegen (Project Nayuki) | MIT | vendored |
| QJsonStruct (Qv2ray Workgroup) | MIT | vendored |
| qAutoStart | MIT | `client/ui/utils/qAutoStart.*` |
| conan_provider.cmake (JFrog) | MIT | build-time only; source-distribution notice |
| OpenSSL 3.6 | Apache-2.0 | shared libraries, beside the client and the service; its NOTICE ships |
| libssh 0.11 | LGPL-2.1 | statically linked; used only by the self-hosted installer, which this product does not reach |
| zlib | zlib | |

## Windows: the tunnel and the proxies

| component | licence | how it ships | status of the text |
|---|---|---|---|
| amneziawg-windows (`tunnel.dll`, built from `github.com/amnezia-vpn/amneziawg-windows`, a fork of wireguard-windows) | MIT (wireguard-windows lineage) | loaded by the service | **licence file not yet read from the tagged source** |
| Wintun 0.14.1 (`wintun.dll`, the signed prebuilt) | **Prebuilt Binaries License** (proprietary, non-free) | loaded through its Permitted API only | `licenses/wintun-prebuilt-binaries-license.txt` - redistribution is permitted only beside software that uses the DLL through `wintun.h` (section 3(d)); the Wintun and WireGuard names are not used (3(e)) |
| tap-windows6 9.27 (`tap0901.sys`, `OemVista.inf`, `tap0901.cat`) | GPL-2.0 | driver installed for OpenVPN | **COPYING not yet read from the source** |
| `devcon.exe` (Microsoft) | Microsoft terms | driver installation | **terms not yet read** |
| Mullvad split-tunnel driver 1.2.5 (`mullvad-split-tunnel.sys`) | GPL-3.0 / MPL-2.0 | signed prebuilt | **ESTABLISHED 2026-09-04**: the four files are downloaded by the `win-split-tunnel/1.2.5.0` conan recipe from `raw.githubusercontent.com/mullvad/mullvadvpn-app-binaries/ff0e3746c89a04314377cffeb52faaa976413a69/x86_64-pc-windows-msvc/split-tunnel`, each verified by SHA-256 in the recipe - `mullvad-split-tunnel.sys` is `4056b22d08115c1a83bc2cafa17de0bb17db3705eac382de77fd7935eeff7edb`. See `SOURCE-REVISIONS.txt`, generated beside the binary |
| OpenVPN 2.7.0 (`openvpn.exe`, with lz4 BSD-2 and lzo GPL-2+) | GPL-2.0-only with the OpenSSL, Apache-2 and LZO exceptions | separate process | built from source; "OpenVPN" is a trademark of OpenVPN Inc. |
| Xray-core via amnezia-xray-bindings 1.4.0 (`amnezia_xray.dll`) | Xray-core MPL-2.0; its `sagernet/sing` and `sing-shadowsocks` modules GPL-3.0-or-later; a GPL-3.0 combined work | loaded by the service | **the wrapper's own licence and the Go module graph not yet read** |
| tun2socks 2.6.0 (`xjasonlyu/tun2socks`) | MIT | separate process | its Go module graph (gVisor Apache-2.0, golang.org/x BSD-3) ships inside it |
| v2ray-rules-dat (`geoip.dat`, `geosite.dat`) | not yet read | shipped beside the service | referenced by nothing in this client; scheduled for removal |
| Go runtime, inside `tunnel.dll`, `tun2socks.exe`, `amnezia_xray.dll` | BSD-3-Clause | statically embedded | per-artefact module notices to be generated with `go version -m` |
| Microsoft Visual C++ runtime | Microsoft redistributable terms | installed by the installer | |

## Android: the tunnel and the proxies

| component | licence | how it ships | status of the text |
|---|---|---|---|
| amneziawg-android (`libwg-go.so`) | MIT (amneziawg-go lineage) | in the APK | **licence file not yet read from the tagged source** |
| wireguard-tools binaries from the same recipe (`libwg.so`, `libwg-quick.so`) | not yet read (wireguard-tools upstream is GPL-2.0) | in the APK, loaded by nothing | candidates for removal |
| amnezia-libxray 1.0.3 (`libxray.aar`, Xray-core + `sagernet/sing*`) | MPL-2.0 + GPL-3.0-or-later, a GPL-3.0 combined work | in the APK | **wrapper licence and module graph not yet read** |
| openvpn-pt-android (`libovpn3.so`, `libovpnutil.so`, `librsapss.so`) - OpenVPN 3 core | AGPL-3.0 | in the APK | GPL-3.0-compatible under GPL-3 section 13; the recipe pins a branch, so the shipped revision must be recorded at release |
| OpenVPN 3 SWIG Java bindings (29 files, `net.openvpn.ovpn3`) | AGPL-3.0 | in the APK | |
| Cloak plugin (`libck-ovpn-plugin.so`) | GPL-3.0 | in the APK, loaded by nothing | scheduled for removal |
| Google ML Kit barcode-scanning 17.3.0 | Google proprietary - **not GPL-compatible** | in both flavours today | **must be replaced or removed before any Android binary is distributed** |
| androidx.*, kotlinx-coroutines, kotlinx-serialization | Apache-2.0 | | |
| Go runtime, inside `libxray.aar`, `libwg-go.so`, `libck-ovpn-plugin.so` | BSD-3-Clause | | per-artefact module notices to be generated |

Not built by this product: Google Play Billing (the `play` flavour), Apple and Linux components
(`amneziawg-apple`, `hev-socks5-tunnel`, `OpenVPNAdapter`, `libcap-ng`, `awg-go`), qtgamepad.

## Fonts, icons and images

| asset | licence | notes |
|---|---|---|
| PT Root UI VF (`client/fonts/pt-root-ui_vf.ttf`), (c) 2018 ParaType Inc., ParaType Ltd. | SIL Open Font License 1.1 | `client/fonts/OFL-PT-Root-UI.txt`; the font is not modified, not sold alone; "PT Root UI" is ParaType's trademark |
| FlagKit (`client/images/flagKit/`), (c) 2016 Bowtie AB | MIT | `client/images/flagKit/LICENSE` |
| control icons (`client/images/controls/*.svg`, 24x24 stroke set) | believed Lucide (ISC) / Feather (MIT) by name and idiom | **origin not yet confirmed per icon** |
| the AresVPN mark and wordmark (`branding/aresvpn/`, and every raster derived from them) | AresVPN's; not licensed under the GPL | see `README.md` |

## Known gaps in this list

Rows marked "not yet read" are read from the component's own source at its pinned tag before
the first binary is distributed; the ML Kit replacement is a release blocker, not a note. This
list is regenerated when a recipe, a Gradle dependency or a Qt module changes.

**The source revisions themselves are no longer a list anybody maintains by hand.**
`cmake/aresvpn/source-revisions.py` reads the conan recipes that produced this build, classifies
how each conveyed component names its source - tag, release, digest, pinned URL, or a branch that
MOVES - and writes `SOURCE-REVISIONS.txt` beside the executable. It is run from
`client/CMakeLists.txt` at configure and, under `-DARES_RELEASE=ON`, a moving reference is fatal
rather than a sentence in this file. Measured on 2026-09-04: **Windows is clean**; **Android is
not**, because `openvpn-pt-android` is cloned from the `update-ovpn3` BRANCH, so a conveyed APK
must record the resolved commit. Two further branch pins exist on the Apple network-extension
path - `openvpnadapter` (`master-amnezia`) and `hev-socks5-tunnel` - which this product does not
ship (`#D178`), and they are listed by the tool rather than filtered away.
