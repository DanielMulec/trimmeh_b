# Agents Playbook for Trimmeh

Use this as a lightweight RACI so we can parallelize the build. All agents work against GNOME 49+ on Fedora 43; no GNOME 47/46 support work is allowed.

## Validation & currency rules
- Always validate any solution design against the official developer documentation first (GNOME, gjs, GTK, xdg-desktop-portal, Rust, etc.). If that is insufficient, run additional fresh web searches before implementation.
- Keep the stack current as of December 7, 2025: Rust stable 1.91.0 (Edition 2024) or newer, GNOME 49 runtime (gjs/GTK from Fedora 43), latest `wasm-bindgen` compatible with that toolchain. Re-evaluate versions quarterly.

## 1) Research Agent
- Tracks upstream changes in GNOME Shell, xdg-desktop-portal, and Fedora packaging that affect clipboard access.
- Maintains a “compat matrix” note for GNOME 48/49, Wayland-only.
- Feeds breaking-change alerts to Core & Shell agents.

## 2) Core Agent (Rust + Wasm)
- Owns `trimmeh-core` crate: parsing, prompt stripping, box-gutter removal, blank-line handling, aggressiveness levels.
- Exposes a pure function `trim(input: &str, Aggressiveness, Options) -> TrimResult`.
- Produces `libtrimmeh_core.wasm` via `wasm-bindgen --target no-modules` suitable for gjs.
- CLI contract: `trimmeh-cli trim < file` and `trimmeh-cli diff` (shows before/after).
- Tests: unit + property tests; add fuzz targets via `cargo fuzz` for edge cases.

## 3) Shell Agent (GNOME extension)
- Implements `trimmeh-shell` extension in GJS/TypeScript.
- Hooks `St.Clipboard` owner-change for CLIPBOARD and PRIMARY selections; skips binary/HTML targets.
- Loads wasm from extension dir, invokes core trim; hashes payloads to avoid loops; caches original for “Restore previous copy”.
- Preferences UI: GTK4/libadwaita; toggles map to GSettings keys documented in README.
- Logging via `log()` to journal; no network calls.

## 4) Integration Agent
- Writes `justfile` tasks: `just build-core`, `just build-wasm`, `just bundle-extension`, `just rpm`.
- Ensures wasm + JS bundling paths are correct inside the extension UUID dir.
- Sets up `scripts/dev-shell.sh` to install/uninstall the extension for GNOME 49 session and restart `gnome-shell --replace` safely.

## 5) Packaging Agent
- Crafts Fedora 43 RPM spec (`packaging/fedora/trimmeh.spec`), including:
  - `gnome-shell-extension-trimmeh` files under `/usr/share/gnome-shell/extensions/trimmeh@trimmeh.dev/`
  - GSettings schema under `/usr/share/glib-2.0/schemas/`
  - Optional `trimmeh-cli` under `/usr/bin`
- Preps extension zip for extensions.gnome.org with correct metadata.json version bounds (`shell-version: ['49', '48']`).

## 6) QA Agent
- Regression suite: golden test fixtures for core trims; in-shell integration test using `gjs` harness that simulates clipboard events.
- Manual checklist on Fedora 43 Wayland: trim simple command, trim with prompts, restore previous copy, toggle aggressiveness, ensure no infinite clipboard loop.
- Monitors for portal permission prompts when optional “Paste trimmed” feature flag is enabled.

## 7) Release Agent
- Tags versions (`v0.1.0` MVP); drafts changelog; publishes RPM to COPR.
- Submits to extensions.gnome.org review once GNOME 49 branch passes smoke.
- Maintains SBOM (cargo + npm lockfiles) and ensures MIT license headers present.

## Communication rules
- Open short issues for cross-agent blockers; avoid large PRs.
- If a change risks touching clipboard semantics or GNOME 48/49 compatibility, loop in Research + QA before merging.
