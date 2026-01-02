# KDE Packaging Clean-Build Results (2026-01-02)

Host: Fedora 43 KDE (Wayland). Provide clean builds via distrobox containers.

## Summary
- Fedora RPM: OK
- Debian 13 (trixie): OK
- Ubuntu 25.10: OK
- Arch (PKGBUILD): OK

## Fedora 43 (RPM)
Command:
- `rpmbuild -ba packaging/fedora/trimmeh-kde.spec --define "_topdir .../.rpmbuild" --define "_sourcedir .../.rpmbuild/SOURCES"`

Artifacts:
- `~/.rpmbuild/RPMS/x86_64/trimmeh-kde-0.1.0-1.fc43.x86_64.rpm`
- `~/.rpmbuild/RPMS/x86_64/trimmeh-cli-0.1.0-1.fc43.x86_64.rpm`
- `~/.rpmbuild/SRPMS/trimmeh-kde-0.1.0-1.fc43.src.rpm`

## Debian 13 (trixie) container
Command:
- `dpkg-buildpackage -b -us -uc`

Artifacts (parent dir of repo inside container):
- `../trimmeh-kde_0.1.0-1_amd64.deb`
- `../trimmeh-cli_0.1.0-1_amd64.deb`
- `../trimmeh-kde-dbgsym_0.1.0-1_amd64.deb` (or .ddeb depending on toolchain)
- `../trimmeh-cli-dbgsym_0.1.0-1_amd64.deb` (or .ddeb)

Notes:
- Optional CMake warning about XKB missing; build still succeeds.

## Ubuntu 25.10 container
Command:
- `dpkg-buildpackage -b -us -uc`

Artifacts (parent dir of repo inside container):
- `../trimmeh-kde_0.1.0-1_amd64.deb`
- `../trimmeh-cli_0.1.0-1_amd64.deb`
- `../trimmeh-kde-dbgsym_0.1.0-1_amd64.ddeb`
- `../trimmeh-cli-dbgsym_0.1.0-1_amd64.ddeb`

## Arch (PKGBUILD)
Command:
- `makepkg -s --noconfirm` (with local tarball)

Artifacts:
- `/tmp/trimmeh-arch-build/trimmeh-kde-0.1.0-1-x86_64.pkg.tar.zst`
- `/tmp/trimmeh-arch-build/trimmeh-cli-0.1.0-1-x86_64.pkg.tar.zst`
- `/tmp/trimmeh-arch-build/trimmeh-debug-0.1.0-1-x86_64.pkg.tar.zst`

Notes:
- Required `debugedit` + `fakeroot` in the container.
- PKGBUILD must avoid using repo root; build happens under `${srcdir}/trimmeh_b-${pkgver}`.

## Fixes applied during the run
- Fedora spec: use `/usr` install prefix (not `/usr/local`) so metainfo validation finds files.
- Debian rules: use `cmake -S trimmeh-kde -B build-kde` to point at the correct source dir.
- Arch PKGBUILD: `cd "${srcdir}/trimmeh_b-${pkgver}"` in build/package steps.
