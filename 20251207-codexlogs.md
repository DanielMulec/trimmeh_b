Here’s a beginner-friendly, step‑by‑step walkthrough for the three tasks, with what each command does and why it matters. You can copy/paste the commands in a Fedora 43 GNOME
  49 terminal.

  Prereqs (do once)

  - Install Rust toolchain with wasm target: rustup target add wasm32-unknown-unknown
  - Install wasm-bindgen CLI (needed to turn raw wasm into gjs‑loadable files): cargo install wasm-bindgen-cli
  - Install node + esbuild (for bundling the extension JS): npm install -g esbuild
  - Make sure gjs is present (it is by default on GNOME): gjs --version
  - From the repo root (~/projekte/trimmeh_b), ensure just is available (if not: cargo install just).

  1. Build the core to WebAssembly and smoke-test it in gjs
     Why: proves the Rust core compiles to wasm and gjs can load and call it before we wire it into GNOME Shell.

  Commands:

  cd ~/projekte/trimmeh_b
  just build-wasm
  ./scripts/gjs-smoketest.js

  What happens:

  - just build-wasm compiles trimmeh-core for wasm32-unknown-unknown, then runs wasm-bindgen to emit shell-extension/wasm/trimmeh_core.js (JS glue) and trimmeh_core_bg.wasm
    (the binary).
  - gjs-smoketest.js imports that glue, initializes the wasm, calls the exported trim_js on a sample $ echo hi, and prints the JSON result. Success output means wasm+gjs interop
    works.

  2. Bundle the GNOME extension and install it locally
     Why: packages the TypeScript sources plus the wasm artifacts into the shape GNOME Shell expects, then installs to your local extensions directory so you can enable it.

  Commands:

  cd ~/projekte/trimmeh_b
  just bundle-extension
  just install-extension

  What happens:

  - bundle-extension runs esbuild over shell-extension/src/*.ts, producing extension.js, clipboard.js, wasm.js, prefs.js; copies metadata, stylesheet, compiled schemas, and wasm
    artifacts into shell-extension/.
  - install-extension copies that folder to ~/.local/share/gnome-shell/extensions/trimmeh@trimmeh.dev/, disables+enables it (so GNOME reloads it). After that, you can toggle it
    in the GNOME Extensions app. The current code will load the wasm and auto‑trim clipboard text; prefs UI is basic but wired to settings.

  3. Add a simple in-shell UI button for “Restore previous copy”
     Why: we already cache the last original clipboard per selection in clipboard.ts; exposing it lets users revert a trim if we were too aggressive.

  How to implement (no commands yet, just plan):

  - Add a quick toggle/entry in the extension UI (e.g., a menu item in a panel indicator or a keybinding). Minimal path: create a small panel button or a quick settings tile
    that, when clicked, calls watcher.restore(St.ClipboardType.CLIPBOARD).
  - The logic is already present: ClipboardWatcher.restore() sets the cached original back onto the clipboard.
  - Once added, rebuild/bundle/install again (just bundle-extension && just install-extension) to test.

  If you want me to add the restore button now, say the word and I’ll code it and guide you through testing.