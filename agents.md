# Agents Playbook for Trimmeh

Use this as a lightweight RACI so we can parallelize the build. All agents work against GNOME 49+ on Fedora 43; no GNOME 47/46 support work is allowed.

## Agent model (single agent, all roles)
- Codex acts as all roles at all times. The role list is a decision guide, not separate agents.
- Pick the right responsibilities per task, but never assume another agent will handle anything.

## Validation & currency rules
- Always validate any solution design against the official developer documentation first (GNOME, gjs, GTK, xdg-desktop-portal, Rust, etc.). If that is insufficient, run additional fresh web searches before implementation.
- Web search is always available and must be used to guide solution design for any task (after official docs checks).
- Keep the stack current as of December 7, 2025: Rust stable 1.91.0 (Edition 2024) or newer (core + CLI), GNOME 49 runtime (gjs/GTK from Fedora 43) for the extension, and Node+npx (`esbuild`) for bundling TypeScript into readable ESM JavaScript for EGO. Re-evaluate versions quarterly.

## 1) Research Agent
- Tracks upstream changes in GNOME Shell, xdg-desktop-portal, and Fedora packaging that affect clipboard access.
- Maintains a “compat matrix” note for GNOME 48/49, Wayland-only.
- Feeds breaking-change alerts to Core & Shell agents.

## 2) Core Agent (Rust + shared TS/JS core)
- Owns `trimmeh-core` crate: parsing, prompt stripping, box-gutter removal, blank-line handling, aggressiveness levels.
- Owns `trimmeh-core-js` shared trimming core used by the GNOME extension (runtime-agnostic TS/JS; pure string + RegExp).
- Keeps Rust + TS/JS behavior in lockstep via `tests/trim-vectors.json` (avoid drift between CLI and extension).
- Exposes a pure function `trim(input: &str, Aggressiveness, Options) -> TrimResult`.
- CLI contract: `trimmeh-cli trim < file` and `trimmeh-cli diff` (shows before/after).
- Tests: unit + property tests; add fuzz targets via `cargo fuzz` for edge cases.

## 3) Shell Agent (GNOME extension)
- Implements the GNOME Shell extension in GJS/TypeScript (`shell-extension/`).
- Hooks `St.Clipboard` owner-change for CLIPBOARD and PRIMARY selections; reads plain text via `get_text()` and ignores empty/non-text.
- Invokes the shared TS/JS trimming core (via `shell-extension/src/trimmer.ts` → `trimmeh-core-js`); hashes payloads to avoid loops; caches original for “Restore previous copy”.
- Preferences UI: GTK4/libadwaita; toggles map to GSettings keys documented in README.
- Logging via `log()` to journal; no network calls.

## 4) Integration Agent
- Writes and maintains `justfile` tasks: `just build-core`, `just build-cli`, `just bundle-extension`, `just bundle-tests`, `just install-extension`, `just extension-zip`, `just rpm`.
- Ensures bundling paths are correct (extension includes the shared `trimmeh-core-js` logic; EGO zip ships only JS + schemas, no extra build inputs).
- Maintains lightweight smoke checks (e.g. `gjs -m tests/clipboard.test.js`) and documents the GNOME 49 Wayland dev loop (install → enable/disable → log out/in if needed).

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
- KDE work must never affect the GNOME version, and GNOME work must never affect the KDE version.
- Do not modify files outside the repository workspace (e.g., installed copies under `~/.local/share/gnome-shell/extensions/`); fix sources in-repo and provide installation steps instead.
- Never edit generated/compiled artifacts (e.g., `shell-extension/extension.js`, `shell-extension/prefs.js`, `tests/dist/*`, `shell-extension/schemas/gschemas.compiled`) directly. Update source files, rebuild, and commit only the source plus intended build outputs when the release process calls for them.
- Before implementing a feature or test, first check whether equivalent logic or coverage already exists in the codebase to avoid duplicating functionality or effort.
- In GJS code, avoid importing `resource:///org/gnome/gjs/modules/byteArray.js`; use `TextDecoder` or other standard APIs instead.
