# KDE Packaging Matrix (Fedora 43, Debian 13, Ubuntu 25.10+, Arch)

Goal: ship Trimmeh KDE so it "just works" on Wayland Plasma by ensuring Klipper and portal dependencies are present.

## Supported targets (as of 2026-01-01)
- Fedora 43 (primary)
- Debian 13 "trixie" stable and Debian testing "forky"
- Ubuntu 25.10 (primary), plus Ubuntu 25.04 and 24.10
- Arch rolling

Notes:
- Ubuntu 24.04 LTS ships Plasma 5 (Qt5/KF5), so it is out of scope for this KF6 build.

## Runtime requirements (why)
- Klipper clipboard D-Bus lives in plasma-workspace.
- Paste injection uses xdg-desktop-portal plus the KDE backend.

## Package name map (runtime)

Fedora 43:
- qt6-qtbase, qt6-qtdeclarative
- kf6-kstatusnotifieritem, kf6-kglobalaccel
- plasma-workspace
- xdg-desktop-portal, xdg-desktop-portal-kde

Debian 13 / testing:
- libkf6statusnotifieritem6, libkf6globalaccel6
- plasma-workspace
- xdg-desktop-portal, xdg-desktop-portal-kde

Ubuntu 25.10 / 25.04 / 24.10:
- libkf6statusnotifieritem6, libkf6globalaccel6
- plasma-workspace
- xdg-desktop-portal, xdg-desktop-portal-kde

Arch:
- qt6-base, qt6-declarative
- kstatusnotifieritem, kglobalaccel
- plasma-workspace
- xdg-desktop-portal, xdg-desktop-portal-kde

## Build requirements
- CMake + C++ toolchain
- Qt6 + KF6 development packages
- esbuild (bundles trimmeh-core-js)
- Rust + cargo (builds trimmeh-cli)

## Clean-build checklist (fast path: COPR / PPA / AUR)

Fedora (mock or rpmbuild):
- rpmbuild -ba packaging/fedora/trimmeh-kde.spec --define "_sourcedir $(pwd)"

Debian / Ubuntu (sbuild or pbuilder):
- dpkg-buildpackage -b -us -uc

Arch (clean chroot or makepkg):
- makepkg -s

## Packaging hygiene notes
- Debian multiarch: trimmeh-core.js is installed under /usr/lib/<multiarch>/trimmeh/.
- For official distro inclusion, Rust crates must be vendored (cargo vendor) or packaged separately.
