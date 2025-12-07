# Packaging & automation (Fedora 43)

## just tasks (planned)
- `build-core` – `cargo build --release` for host binary.
- `build-wasm` – `cargo build -p trimmeh-core --release --target wasm32-unknown-unknown` + `wasm-bindgen --target no-modules --out-dir shell-extension/wasm`.
- `bundle-extension` – transpile TS → JS via `esbuild`, copy schemas, wasm, metadata.
- `rpm` – run `rpmbuild -ba packaging/fedora/trimmeh.spec --define "_sourcedir $(pwd)"`.
- `dev-shell` – install/uninstall extension in `$XDG_DATA_HOME/gnome-shell/extensions/` and restart GNOME Shell safely.

## RPM spec skeleton
- Name: `gnome-shell-extension-trimmeh`
- Version: `0.1.0`
- Summary: “Clipboard auto-flattener for GNOME 49+”
- License: MIT
- BuildRequires: `cargo`, `rust`, `wasm-bindgen-cli`, `glib2-devel`, `gobject-introspection-devel`, `gnome-shell-devel`
- Requires: `gnome-shell >= 49`, `gjs`, `xdg-desktop-portal >= 1.18` (optional feature), `gtk4`
- Files:
  - `/usr/share/gnome-shell/extensions/trimmeh@trimmeh.dev/*`
  - `/usr/share/glib-2.0/schemas/org.trimmeh.gschema.xml`
  - `/usr/bin/trimmeh-cli` (optional subpackage `trimmeh-cli`)
- %post: `glib-compile-schemas /usr/share/glib-2.0/schemas/`
- %files for CLI subpackage guarded so desktop installs without CLI are possible.

## Extension zip for EGO
- Produced from `shell-extension/` tree.
- `metadata.json` `shell-version`: `["49", "48"]` (no older versions).
- Exclude `node_modules`; ship built JS + wasm + schemas.
