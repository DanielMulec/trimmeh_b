Name:           trimmeh-kde
Version:        0.1.0
Release:        1%{?dist}
Summary:        Clipboard auto-trimmer tray app for KDE Plasma

License:        MIT
URL:            https://www.danielmulec.com
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  kf6-kstatusnotifieritem-devel
BuildRequires:  kf6-kglobalaccel-devel
BuildRequires:  golang-github-evanw-esbuild
BuildRequires:  cargo
BuildRequires:  rust
BuildRequires:  appstream-util

Requires:       qt6-qtbase
Requires:       qt6-qtdeclarative
Requires:       kf6-kstatusnotifieritem
Requires:       kf6-kglobalaccel
Requires:       plasma-workspace
Requires:       xdg-desktop-portal
Requires:       xdg-desktop-portal-kde
Recommends:     trimmeh-cli

%description
Trimmeh KDE is a Plasma tray app that trims multi-line shell snippets on the clipboard into single-line commands.

%package -n trimmeh-cli
Summary:        Command-line interface for Trimmeh
Requires:       %{name}%{?_isa}

%description -n trimmeh-cli
Trimmeh CLI provides trimming and diff commands for shell snippets.

%prep
%autosetup -n %{name}-%{version}

%build
cmake -S trimmeh-kde -B build-kde -DCMAKE_BUILD_TYPE=Release
cmake --build build-kde
cargo build -p trimmeh-cli --release

%install
DESTDIR=%{buildroot} cmake --install build-kde
install -Dpm0755 target/release/trimmeh-cli %{buildroot}%{_bindir}/trimmeh-cli

%check
appstream-util validate-relax --nonet %{buildroot}%{_datadir}/metainfo/dev.trimmeh.TrimmehKDE.metainfo.xml

%files
%license LICENSE
%{_bindir}/trimmeh-kde
%{_libdir}/trimmeh/trimmeh-core.js
%{_datadir}/applications/dev.trimmeh.TrimmehKDE.desktop
%{_datadir}/metainfo/dev.trimmeh.TrimmehKDE.metainfo.xml

%files -n trimmeh-cli
%license LICENSE
%{_bindir}/trimmeh-cli

%changelog
* Wed Dec 31 2025 Daniel Mulec <noreply@trimmeh.dev> 0.1.0-1
- Initial KDE package
