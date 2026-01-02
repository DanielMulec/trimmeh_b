# Trimmeh ✂️ (GNOME 49+ / KDE Plasma 6.4+)

Trimmeh is the South-Parkian cousin of [Trimmy](https://github.com/steipete/Trimmy) — same heuristics, now for Wayland on **GNOME** and **KDE**. It flattens multi-line shell snippets on your clipboard so they paste and run cleanly without needing a background daemon.

## What it does (parity with Trimmy)
- Detects command-like multi-line snippets, joins lines (respects `\` continuations and wrapped tokens), strips prompts/box gutters, repairs wrapped URLs, and preserves blank lines when you ask.
- Aggressiveness levels: Low / Normal / High (High is used by force/“Paste Trimmed” flows).
- Toggles: Keep blank lines; Strip box gutters; Strip prompts; Enable auto-trim.
- Manual: “Paste Trimmed”, “Paste Original”, and “Restore last copy” (one-shot) via panel menu; CLI with `--force`, `--json`, preserve/keep flags.
- Safety: skips blobs >10 lines by default; tags own writes to avoid loops.

## Platform & support stance
- **GNOME:** Shell 49 on Fedora 43 (Wayland). GNOME 48 should work; earlier is unsupported.
- **KDE:** Plasma 6.5.4 primary; baseline Plasma ≥ 6.4 (Wayland).
- No X11. No telemetry. No network calls.

## Install options
**GNOME (extension)**
- **Local dev install:** `just install-extension` (bundle JS, compile schemas, install to `~/.local/share/gnome-shell/extensions/trimmeh@trimmeh.dev/`).
- **Extension zip (for extensions.gnome.org or manual):** `just extension-zip` → `trimmeh-extension.zip`.
- **RPM (Fedora 43+):** `just rpm` → `.rpmbuild/RPMS/noarch/gnome-shell-extension-trimmeh-<ver>.rpm`.

**KDE (tray app)**
- Build and run from source:
  - `cmake -S trimmeh-kde -B build-kde -DCMAKE_BUILD_TYPE=Release`
  - `cmake --build build-kde`
  - `./build-kde/trimmeh-kde`
- Packaging status and clean-build results: see `docs/kde-packaging-results.md`.
- Optional portal pre-authorization (avoids repeated permission prompts):
  - `flatpak permission-set kde-authorized remote-desktop dev.trimmeh.TrimmehKDE yes`

**CLI**
- `cargo build -p trimmeh-cli --release` (optionally package as subpackage in RPM).

## Using it
**GNOME**
- Enable the extension (GNOME Extensions app or `gnome-extensions enable trimmeh@trimmeh.dev`).
- Top-bar menu: Last preview, Auto-trim toggle, Paste Trimmed, Paste Original, Restore last copy, Preferences.
- Global hotkeys: disabled by default for EGO compliance. Set them in Trimmeh Preferences (Hotkeys section) or via GSettings.
  - Inspect: `gsettings get org.gnome.shell.extensions.trimmeh paste-trimmed-hotkey`
  - Set (example): `gsettings set org.gnome.shell.extensions.trimmeh paste-trimmed-hotkey "['<Super><Alt>y']"` and `gsettings set org.gnome.shell.extensions.trimmeh paste-original-hotkey "['<Super><Alt><Shift>y']"`
- Manual paste is injected via GNOME Shell’s compositor virtual keyboard (prefers Shift+Insert, falls back to Ctrl+V). Trimmeh waits briefly for hotkey modifiers (Super/Alt/Shift) to be released before injecting paste (Wayland-friendly, best-effort).
- Known limitation: on Wayland this paste injection is still best-effort; some apps may ignore synthetic paste key events. If a paste hotkey/menu action fires but nothing is pasted, try again or rely on auto-trim + normal app paste (and use “Restore last copy” if you need to revert the clipboard).
- Preferences (libadwaita): aggressiveness, keep blank lines, strip prompts, strip box gutters, max lines, enable auto-trim.

**KDE**
- Tray menu: Paste Trimmed, Paste Original, Restore last copy, Auto-Trim toggle, Settings, About.
- Preferences (Qt): aggressiveness, keep blank lines, strip prompts/box chars, restore delay, hotkeys, start at login, optional clipboard fallbacks.
- Hotkeys are managed via the Shortcuts tab (KGlobalAccel).
- Manual paste uses the RemoteDesktop portal (Shift+Insert → Ctrl+V fallback). If permission is denied/unavailable, Trimmeh swaps the clipboard and shows a “Paste now” hint so you can paste manually.

**CLI examples**
- Trim stdin: `printf 'echo a\necho b\n' | trimmeh-cli trim --json`
- Trim a file: `trimmeh-cli trim --trim ./script.sh`
- Force High: `trimmeh-cli trim -f`
- Keep box/prompt: `trimmeh-cli trim --keep-box-drawing --keep-prompts`
- Preserve blank lines: `trimmeh-cli trim --preserve-blank-lines`
- Unchanged exit code = 2 (matches Trimmy).

## Build from source
Prereqs:
- Rust stable (1.91+) for the CLI and core
- Node+`npx` with `esbuild` for the shared JS core / GNOME bundle
- GNOME Shell headers/libs (GNOME)
- Qt6 + KF6 (StatusNotifierItem, GlobalAccel) + CMake (KDE)

Common tasks (from repo root):
- `just build-core` — native core
- `just bundle-extension` — bundle TS → JS & schemas (GNOME)
- `just install-extension` — dev install + schemas (GNOME)
- `just extension-zip` — release zip (GNOME)
- `just rpm` — build RPM (GNOME)

KDE build:
- `cmake -S trimmeh-kde -B build-kde -DCMAKE_BUILD_TYPE=Debug`
- `cmake --build build-kde`
- `./build-kde/trimmeh-kde`

## Release checklist
- Bump version in `Cargo.toml` (CLI/core) and tags (EGO manages extension versions; avoid setting `metadata.json` `version`).
- `cargo test -p trimmeh-core`
- `just extension-zip` (for EGO upload)
- `just rpm` (for Fedora/COPR)
- Create Git tag/release, attach zip/RPM.
- Submit zip to extensions.gnome.org (shell-version ["49","48"]).

## Differences vs macOS Trimmy
- Same parsing heuristics and CLI flags/exit codes.
- GNOME shell extension + KDE tray app instead of a macOS menu bar app.
- No Sparkle updates (Linux updates via package managers).
- Manual “Paste Trimmed/Original” actions are available via the tray/panel menu and global hotkeys.

## Contributing / repo quickstart
- Clone, then: `rustup target add wasm32-unknown-unknown`; `cargo install wasm-bindgen-cli`; `npm install` (if you want esbuild locally, otherwise npx).
- GNOME dev loop: edit core/TS, run `just install-extension`, reload GNOME Shell extension, test.
- KDE dev loop: `cmake -S trimmeh-kde -B build-kde -DCMAKE_BUILD_TYPE=Debug`, `cmake --build build-kde`, run `./build-kde/trimmeh-kde`.
- Tests: `cargo test -p trimmeh-core` (goldens for prompts, gutters, URLs, blank lines, list skipping, backslash merge). KDE manual checklist lives in `docs/kde-qa.md`.

## Credit
Trimmeh is a port of Peter Steinberger’s Trimmy — think of it as Trimmy’s Wayland-native cousin. MIT licensed.
