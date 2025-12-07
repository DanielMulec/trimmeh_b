# Trimmeh (GNOME 49+)

Linux-first cousin of Trimmy (https://github.com/steipete/Trimmy) that flattens multi-line shell snippets on the clipboard so they paste and run cleanly on GNOME 49 (Fedora 43 and later).

## Why
- Web tutorials and chats often ship shell commands split across lines, with prompts or box-drawing gutters. Pasting them into a terminal on Linux usually fails.
- macOS users get Trimmy (https://github.com/steipete/Trimmy); GNOME/Wayland users do not, largely because clipboard snooping requires compositor-level privileges.
- GNOME 49 is our minimum supported desktop. We intentionally do **not** target GNOME 47/46 or older; side-effect compatibility is fine but unsupported.

## Target platform
- GNOME 49 on Fedora 43 (Wayland session). Should also work on GNOME 48; anything older is out-of-scope.
- Shell extension sandbox: GNOME Shell 45+ JS runtime (gjs) with GTK 4.20 libs available for prefs UI.
- PipeWire / xdg-desktop-portal >= 1.18 present by default on Fedora 43.

## User-facing feature set (MVP)
- Auto-trim: watch the clipboard and rewrite multi-line shell commands into a single line, respecting `\` continuations.
- Prompt cleanup: strip leading `$ ` or `# ` prompts when the line looks like a command; leave Markdown headings intact.
- Aggressiveness levels: Low / Normal / High mirroring Trimmy (https://github.com/steipete/Trimmy).
- Toggles: keep blank lines, remove box-drawing gutters (`│┃`), enable/disable auto-trim.
- Manual actions: “Trim current copy” and “Restore previous copy”.
- Safety valves: skip blobs >10 lines; avoid loops by tagging our own writes (hash-based).

## Non-goals (for now)
- Full clipboard history manager.
- Supporting X11.
- Supporting GNOME 47/46 or earlier (explicitly out-of-scope).
- Universal hotkey that pastes on your behalf without user consent (blocked by Wayland security; see Roadmap for optional portal-assisted flow).

## Architecture
**Components**
- `trimmeh-core` (Rust): parsing + rewrite engine; compiled to:
  - native CLI (`trimmeh-cli`) for testing and headless use
  - WebAssembly for in-shell use (gjs supports WebAssembly).
- `trimmeh-shell` (GNOME Shell extension):
  - Listens to clipboard owner changes via `St.Clipboard`.
  - Pulls `text/plain`, runs it through `trimmeh-core` (wasm), and writes back if transformed.
  - Stores last-original clipboard in memory to enable “Restore previous copy”.
  - Preferences UI built with libadwaita/GTK4 (runs from `gnome-extensions prefs`).
- Packaging:
  - RPM spec for Fedora 43 (`gnome-shell-extension-trimmeh` + `trimmeh-cli`).
  - Zip for manual install / extensions.gnome.org (pending review).

**Clipboard strategy (Wayland constraints)**
- GNOME does not expose `ext-data-control` to regular apps; clipboard managers must run with Shell privileges. We therefore implement as a Shell extension, not a standalone daemon.
- Auto-paste is not possible without elevated portals. We keep the workflow “rewrite the clipboard, user pastes manually” by default.
- To avoid reprocessing our own writes, we track a 128-bit hash of the last transformed payload per selection type and bail if unchanged.

**Data flow**
1. Shell extension receives clipboard owner change.
2. Fetch `text/plain` (clipboard & primary).
3. Pass through wasm `trimmeh-core`.
4. If transformed and within safety limits, update clipboard and record original.
5. Update UI badge to reflect last action (trimmed / skipped / restored).

**Preferences (GSettings schema)**
- `org.trimmeh.aggressiveness` enum: low|normal|high (default normal).
- `org.trimmeh.keep-blank-lines` boolean.
- `org.trimmeh.strip-box-chars` boolean.
- `org.trimmeh.enable-auto-trim` boolean (default true).

## Roadmap
1. **MVP (Sprint 1)** — core heuristics + Shell extension auto-trim, prefs panel, RPM packaging.
2. **Quality (Sprint 2)** — fuzz tests for parser, telemetry-free crash logging to journal, better prompt heuristics (PS1 variants).
3. **Convenience (Sprint 3)** — optional “Paste trimmed” using xdg-desktop-portal RemoteDesktop/Keyboard injection with explicit one-time consent, plus CLI `trimmeh paste` helper for terminals that allow programmatic paste.
4. **Polish (Sprint 4)** — localization, onboarding tooltip, extension review for extensions.gnome.org.

## Build notes (planned)
- Core: Rust 1.82+; `cargo build --release` produces `libtrimmeh_core.wasm` and `trimmeh-cli`.
- Wasm glue: `wasm-bindgen --target no-modules` for gjs compatibility; load via `Gio.File.load_contents` and `WebAssembly.instantiate`.
- Shell extension: TypeScript (ts-node for dev) transpiled to plain JS; bundle with `esbuild` (no node_modules at runtime).
- Packaging: use `just` recipes to build wasm, bundle extension, and produce RPM via `rpmbuild -ba packaging/fedora/trimmeh.spec`.

## Risks & mitigations
- **Clipboard access limits on Wayland:** solved by running inside GNOME Shell. No data-control dependency.
- **User trust:** no network I/O; no telemetry. Keep code small, MIT-licensed.
- **Portal regressions:** Fedora updates xdg-desktop-portal aggressively; keep portal use optional and feature-flagged.

## Next steps
- Finalize `trimmeh-core` heuristics spec and test vectors.
- Scaffold repo layout (`core/`, `shell-extension/`, `packaging/`).
- Start implementing core parser in Rust + wasm pipeline.

---
Document authored December 7, 2025. Aligns with GNOME 49 (“Brescia”) and xdg-desktop-portal 1.18 releases. Older GNOME releases are intentionally unsupported.***
