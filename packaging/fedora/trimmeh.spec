Name:           gnome-shell-extension-trimmeh
Version:        0.1.0
Release:        1%{?dist}
Summary:        Clipboard auto-flattener for GNOME 49+

License:        MIT
URL:            https://example.com/trimmeh
Source0:        %{name}-%{version}.tar.gz

BuildArch:      noarch
BuildRequires:  cargo
BuildRequires:  rust
BuildRequires:  glib2-devel
BuildRequires:  gobject-introspection
BuildRequires:  gnome-shell

Requires:       gnome-shell >= 49
Requires:       gjs
Requires:       gtk4

%description
Trims multi-line shell snippets on the clipboard into single-line commands for GNOME 49+ (Wayland).

%prep
%autosetup -n %{name}-%{version}

%build
just bundle-extension

%install
mkdir -p %{buildroot}%{_datadir}/gnome-shell/extensions/trimmeh@trimmeh.dev/
cp -a shell-extension/* %{buildroot}%{_datadir}/gnome-shell/extensions/trimmeh@trimmeh.dev/
mkdir -p %{buildroot}%{_datadir}/glib-2.0/schemas
cp shell-extension/schemas/org.gnome.shell.extensions.trimmeh.gschema.xml %{buildroot}%{_datadir}/glib-2.0/schemas/

%post
glib-compile-schemas %{_datadir}/glib-2.0/schemas > /dev/null 2>&1 || :

%files
%license LICENSE
%{_datadir}/gnome-shell/extensions/trimmeh@trimmeh.dev/
%{_datadir}/glib-2.0/schemas/org.gnome.shell.extensions.trimmeh.gschema.xml

%changelog
* Sun Dec 07 2025 Trimmeh Team <noreply@trimmeh.dev> 0.1.0-1
- Initial package skeleton
