# Trimmeh Shell Extension Layout (GNOME 49)

Goal: GNOME Shell extension that watches CLIPBOARD and PRIMARY selections, runs `trimmeh-core` (wasm) on text, rewrites clipboard when changed, and exposes a preferences UI + manual actions.

## Directory layout (current)
```
shell-extension/
  metadata.json
  extension.js        # compiled from src/extension.ts
  clipboard.js        # compiled from src/clipboard.ts
  prefs.js            # compiled from src/prefs.ts (GTK4/libadwaita)
  wasm.js             # compiled from src/wasm.ts
  src/
    extension.ts
    prefs.ts
    panel.ts          # top-bar menu UI
    clipboard.ts      # St.Clipboard adapter + manual paste entrypoints
    clipboardWatcher.ts # race-safe watcher core + manual paste flows
    virtualPaste.ts   # compositor virtual keyboard injection (Wayland)
    wasm.ts           # loads wasm, wraps trim()
  wasm/
    trimmeh_core_bg.wasm
    trimmeh_core.js   # wasm-bindgen glue (web target)
  schemas/
    org.gnome.shell.extensions.trimmeh.gschema.xml
  stylesheet.css
```

## Clipboard hook
- Use `St.Clipboard.get_default()` and connect to `owner-change` for both `St.ClipboardType.CLIPBOARD` and `PRIMARY`.
- On change: `get_text(selection, callback)`; ignore non-text or empty strings.
- Pass text to wasm `trim`; if `changed`, set text with `set_text()` and cache original per selection to support “Restore previous copy”.
- Tag writes with `hash` to avoid loops; skip if incoming text hash matches last written hash.
- Safety: skip blobs over `max_lines` (from settings).
- If `owner-change` is not available, fall back to polling.

## Wasm loading
- Dynamically import the wasm-bindgen glue (`wasm/trimmeh_core.js`) via a `file://` URI.
- Read the wasm bytes (`wasm/trimmeh_core_bg.wasm`) via `Gio.File.load_contents`.
- Initialize with `initSync({ module: wasmBytes })` when available; otherwise try async init if supported.
- Wrap exported `trim_js(input, aggressiveness, opts)` in `wasm.ts` to provide typed helpers and stable return shapes.

## Settings keys (GSettings)
Schema id: `org.gnome.shell.extensions.trimmeh`
- `aggressiveness` (enum string): `low|normal|high`
- `keep-blank-lines` (bool)
- `strip-box-chars` (bool)
- `enable-auto-trim` (bool)
- `max-lines` (int, default 10)
- `paste-trimmed-hotkey` (as)
- `paste-original-hotkey` (as)
- `toggle-auto-trim-hotkey` (as)

## Preferences UI (libadwaita)
- Behavior settings: toggles + aggressiveness + max-lines.
- Hotkeys: set/clear the three GSettings keybindings by capturing a shortcut in the UI.
- Manual actions (Paste Trimmed / Paste Original / Restore last copy) live in the top-bar menu.

## Dev workflow
- `just build-wasm` → builds wasm + wasm-bindgen outputs into `shell-extension/wasm/`.
- `just bundle-extension` → bundles TS via `esbuild` and compiles schemas.
- `just install-extension` → installs to `~/.local/share/gnome-shell/extensions/trimmeh@trimmeh.dev/` and toggles the extension (log out/in if GNOME doesn’t reload it).
