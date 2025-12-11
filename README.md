# Trimmeh ✂️ (GNOME 49+)

Trimmeh is the South-Parkian cousin of [Trimmy](https://github.com/steipete/Trimmy) — same heuristics, now for GNOME/Wayland. It flattens multi-line shell snippets on your clipboard so they paste and run cleanly on GNOME 49 (Fedora 43+) without needing a background daemon.

## What it does (parity with Trimmy)
- Detects command-like multi-line snippets, joins lines (respects `\` continuations and wrapped tokens), strips prompts/box gutters, repairs wrapped URLs, and preserves blank lines when you ask.
- Aggressiveness levels: Low / Normal / High (High is used by force/“Paste Trimmed” flows).
- Toggles: Keep blank lines; Strip box gutters; Strip prompts; Enable auto-trim.
- Manual: “Paste Trimmed”, “Paste Original”, and “Restore last copy” (one-shot) via panel menu; CLI with `--force`, `--json`, preserve/keep flags.
- Safety: skips blobs >10 lines by default; tags own writes to avoid loops.

## Platform & support stance
- GNOME Shell 49 on Fedora 43 (Wayland). GNOME 48 should work; earlier is unsupported.
- No X11. No telemetry. No network calls.

## Install options
- **Local dev install:** `just install-extension` (build wasm, bundle JS, compile schemas, install to `~/.local/share/gnome-shell/extensions/trimmeh@trimmeh.dev/`).
- **Extension zip (for extensions.gnome.org or manual):** `just extension-zip` → `trimmeh-extension.zip`.
- **RPM (Fedora 43+):** `just rpm` → `.rpmbuild/RPMS/noarch/gnome-shell-extension-trimmeh-<ver>.rpm`.
- **CLI:** `cargo build -p trimmeh-cli --release` (optionally package as subpackage in RPM).

## Using it
- Enable the extension (GNOME Extensions app or `gnome-extensions enable trimmeh@trimmeh.dev`).
- Top-bar menu: Auto-trim toggle, Restore last copy (one-shot), Preferences.
- Top-bar menu: Last preview, Auto-trim toggle, Paste Trimmed, Paste Original, Restore last copy, Preferences.
- Global hotkeys (defaults): Paste Trimmed = `<Super><Alt>t`, Paste Original = `<Super><Alt><Shift>t`. Rebind via dconf key arrays `paste-trimmed-hotkey` / `paste-original-hotkey`.
- Preferences (libadwaita): aggressiveness, keep blank lines, strip prompts, strip box gutters, max lines, enable auto-trim.
- CLI examples:
  - Trim stdin: `printf 'echo a\necho b\n' | trimmeh-cli trim --json`
  - Trim a file: `trimmeh-cli trim --trim ./script.sh`
  - Force High: `trimmeh-cli trim -f`
  - Keep box/prompt: `trimmeh-cli trim --keep-box-drawing --keep-prompts`
  - Preserve blank lines: `trimmeh-cli trim --preserve-blank-lines`
  - Unchanged exit code = 2 (matches Trimmy).

## Build from source
Prereqs: Rust stable (1.91+), `wasm32-unknown-unknown` target, `wasm-bindgen` on PATH, Node+`npx` with `esbuild`, GNOME Shell headers/libs.

Common tasks (from repo root):
- `just build-core` — native core
- `just build-wasm` — wasm artifact
- `just bundle-extension` — bundle TS → JS & schemas
- `just install-extension` — dev install + schemas
- `just extension-zip` — release zip
- `just rpm` — build RPM (workspace-local buildroot)

## Release checklist
- Bump version in `Cargo.toml`/`metadata.json` and tags.
- `cargo test -p trimmeh-core`
- `just extension-zip` (for EGO upload)
- `just rpm` (for Fedora/COPR)
- Create Git tag/release, attach zip/RPM.
- Submit zip to extensions.gnome.org (shell-version ["49","48"]).

## Differences vs macOS Trimmy
- Same parsing heuristics and CLI flags/exit codes.
- GNOME shell extension instead of menu bar app; no Sparkle updates, no auto-launch toggle needed.
- Manual “Paste Trimmed/Original” actions are available in the panel menu; global hotkeys are still planned.

## Contributing / repo quickstart
- Clone, then: `rustup target add wasm32-unknown-unknown`; `cargo install wasm-bindgen-cli`; `npm install` (if you want esbuild locally, otherwise npx).
- Dev loop: edit core/TS, run `just install-extension`, reload GNOME Shell extension, test.
- Tests: `cargo test -p trimmeh-core` (goldens for prompts, gutters, URLs, blank lines, list skipping, backslash merge).

## Credit
Trimmeh is a port of Peter Steinberger’s Trimmy — think of it as Trimmy’s Wayland-native cousin. MIT licensed.
