# trimmeh-shell scaffold (GNOME 49)

Goal: GNOME Shell extension that watches CLIPBOARD and PRIMARY selections, runs `trimmeh-core` (wasm) on text, rewrites clipboard when changed, and exposes prefs UI.

## Directory layout (planned)
```
shell-extension/
  metadata.json
  extension.js        # compiled from src/extension.ts
  prefs.js            # compiled from src/prefs.ts (GTK4/libadwaita)
  src/
    extension.ts
    prefs.ts
    clipboard.ts      # glue to St.Clipboard
    wasm.ts           # loads wasm, wraps trim()
  wasm/
    libtrimmeh_core.wasm
    trimmeh_core.js   # wasm-bindgen glue (no modules)
  schemas/
    org.gnome.shell.extensions.trimmeh.gschema.xml
  stylesheet.css
```

## Clipboard hook
- Use `St.Clipboard.get_default()` and connect to `owner-change` for both `St.ClipboardType.CLIPBOARD` and `PRIMARY`.
- On change: `get_text(MimeType.PLAIN_TEXT, callback)`; ignore non-text or empty strings.
- Pass text to wasm `trim`; if `changed`, set text with `set_text()` and cache original per selection to support “Restore previous copy”.
- Tag writes with `hash` to avoid loops; skip if incoming text hash matches last written hash.
- Safety: skip blobs over `max_lines` (from settings).

## Wasm loading
- Load wasm bytes via `Gio.File.new_for_uri(Me.dir.get_child('wasm/libtrimmeh_core.wasm'))`.
- Instantiate with `WebAssembly.instantiateStreaming` when available in gjs 1.82; fallback to `WebAssembly.instantiate`.
- Import object only needs `env`: memory (provided by wasm-bindgen) and `wbindgen_malloc/free` shims.
- Wrap exported `trim_js(input, aggressiveness, opts)` in `wasm.ts` to provide typed helpers.

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
- ListBox with switches for each setting and a combobox for aggressiveness.
- “Trim current clipboard” button executes once without toggling auto mode.
- “Restore previous copy” button resets clipboard to cached original if present.

## Dev workflow (planned commands)
- `just build-wasm` → builds `libtrimmeh_core.wasm` + glue into `shell-extension/wasm/`.
- `just bundle-extension` → runs `ts-node` + `esbuild` to emit `extension.js`/`prefs.js` and copies schemas.
- `just install-extension` → copies to `~/.local/share/gnome-shell/extensions/trimmeh@trimmeh.dev/` and restarts shell in a controlled way.
