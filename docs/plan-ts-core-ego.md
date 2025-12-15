# Plan: JS/TS Core + EGO-Compatible GNOME Extension

This document is a step-by-step migration + validation plan for making Trimmeh’s GNOME extension acceptable for submission to extensions.gnome.org (EGO) by removing the WASM dependency and using a readable JS/TS trimming core.

It is written so a future maintainer (or a future chat/agent instance) can re-run the work from scratch and know what to check, change, and verify.

## Goals
- Keep **Trimmy parity** for trimming heuristics (command detection + flattening + prompt/box/URL handling).
- Make the GNOME extension **EGO-review compatible**:
  - No binaries/libraries shipped inside the extension zip (including `.wasm`).
  - No default keybindings for clipboard actions (user must opt in).
  - Readable JS (no minification/obfuscation).
  - Correct enable()/disable() lifecycle behavior.
- Keep the trimming core **runtime-agnostic** (no GJS-only/Node-only APIs), so it can be reused later for a KDE/Plasma port (QML/JS).

## Non-goals (for this migration)
- Rewriting the CLI to JS/TS. (The CLI can remain Rust; EGO does not care about your CLI.)
- Implementing a KDE/Plasma port right now (only enable future reuse).
- Changing Trimmeh’s UX semantics.

## Current target architecture (post-migration)
- **Trimming core**: pure TS/JS module (`trimmeh-core-js/`) containing only strings + RegExp logic.
- **GNOME extension**: bundles TS → readable ESM JS via esbuild, calls the JS core directly.
- **CLI**: continues to use Rust `trimmeh-core` (optional future work: ship a bundled JS CLI if desired).

## Step 0 — Pre-flight: audit for EGO blockers
1. Identify whether the extension currently ships any binaries:
   - Any `.wasm` files.
   - Any native `.so`, `.node`, etc.
2. Identify any default clipboard keybindings:
   - Check `shell-extension/schemas/org.gnome.shell.extensions.trimmeh.gschema.xml`.
   - EGO requires clipboard hotkey defaults to be empty (`[]`), with user opt-in.
3. Confirm the extension zip will contain only what is needed to run:
   - `metadata.json`
   - `extension.js` (and any other runtime JS your `extension.js` imports)
   - `prefs.js`
   - `stylesheet.css` (if used)
   - schema XML + compiled schema
   - license file(s)

## Step 1 — Create a runtime-agnostic JS/TS core
Create a new module (directory) for the shared trimming logic:
- `trimmeh-core-js/src/index.ts`

Requirements:
- No Node APIs, no GJS GI imports.
- Only string processing + RegExp.
- Export a stable API shape:
  - `trim(input, aggressiveness, options) -> { output, changed, reason? }`
  - `DEFAULT_TRIM_OPTIONS`
  - Types: `Aggressiveness`, `TrimOptions`, `TrimReason`, `TrimResult`

Keep the algorithm aligned with:
- `upstream/Trimmy/Sources/TrimmyCore/TextCleaner.swift`
- `trimmeh-core/src/lib.rs`

## Step 2 — Add shared “golden” vectors to prevent drift
Create one canonical set of test vectors and run them against both cores:
- `tests/trim-vectors.json`

Then:
- Add a Rust test reading vectors via `include_str!()` and validating `trimmeh-core` matches.
- Add a GJS test runner that imports the bundled JS core and validates the same vectors.

Recommended commands:
- Rust: `cargo test -p trimmeh-core`
- JS core (GJS): bundle + run
  - `just bundle-tests`
  - `gjs -m tests/trimCore.test.js`

Why this matters:
- If you keep a Rust CLI and a JS extension, vectors are what prevents behavioral divergence.

## Step 3 — Switch the GNOME extension from WASM to JS
1. Create a thin adapter that matches the old trimmer interface expected by the extension:
   - `shell-extension/src/trimmer.ts`
2. Update imports so the extension uses the new adapter:
   - `shell-extension/src/extension.ts` should create `createTrimAdapter()` instead of loading WASM.
   - `shell-extension/src/clipboardWatcher.ts` and `shell-extension/src/clipboard.ts` should import types from `./trimmer.js`.
3. Remove WASM loader sources and build outputs from the extension tree:
   - Remove `shell-extension/src/wasm.ts`.
   - Ensure the release zip does **not** include any `.wasm`.

## Step 4 — EGO compliance fixes (checklist)
### 4.1 No default clipboard hotkeys
In `shell-extension/schemas/org.gnome.shell.extensions.trimmeh.gschema.xml`:
- `paste-trimmed-hotkey` default must be `[]`
- `paste-original-hotkey` default must be `[]`
- `toggle-auto-trim-hotkey` can stay `[]`

Also update user-facing docs so they don’t claim default hotkeys exist.

### 4.2 Readable JS (no minification/obfuscation)
- Ensure your build uses esbuild without minification.
- Prefer bundling into a few ESM files that are easy to review.

### 4.3 Reduce “excessive logging”
- Keep `log()` for important state transitions and actionable warnings.
- Keep `logError()` for errors.
- Avoid per-event spam (clipboard changes can be frequent).

### 4.4 Clipboard disclosure
- Ensure `metadata.json` description clearly states the extension reads/writes clipboard and does not share it.

### 4.5 enable()/disable() lifecycle hygiene
- No GObject/GTK objects created at module scope.
- Everything created in `enable()` is destroyed/disconnected in `disable()`.
- All GLib sources removed in `disable()` if created.

## Step 5 — Tighten packaging (EGO zip)
Update `justfile` so the EGO zip includes only what’s needed and excludes the source tree.

Recommended behavior:
- `just extension-zip` should build and zip only:
  - `metadata.json`
  - `extension.js`
  - `prefs.js`
  - `stylesheet.css`
  - `LICENSE`
  - `schemas/org.gnome.shell.extensions.trimmeh.gschema.xml`
  - `schemas/gschemas.compiled`

Then verify:
- `zipinfo -1 trimmeh-extension.zip`

## Step 6 — Local testing before EGO upload
### Build/install locally
- `just install-extension`

### Reload
- Wayland: log out/in OR disable/enable the extension.
- Quick toggle:
  - `gnome-extensions disable trimmeh@trimmeh.dev`
  - `gnome-extensions enable trimmeh@trimmeh.dev`

### Watch logs
- `journalctl --user -f | rg -i "trimmeh|gjs"`

### Manual UX test checklist
- Auto-trim works when enabled and does not loop.
- Panel menu actions:
  - Paste Trimmed
  - Paste Original
  - Restore last copy
- Preferences window:
  - Aggressiveness + toggles work.
  - Hotkeys can be set manually and are disabled by default.

## Step 7 — Build the final EGO artifact
- `just extension-zip`

Upload `trimmeh-extension.zip` to EGO.

## Notes for future KDE/Plasma port
- If Plasma is implemented in QML/JS (plasmoid), the JS core can be reused directly.
- UI/integration is still new work, but the trimming heuristics stay shared via `trimmeh-core-js`.

