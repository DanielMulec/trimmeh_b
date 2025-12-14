# Trimmeh Shell Extension Layout (GNOME 49)

Goal: GNOME Shell extension that watches CLIPBOARD and PRIMARY selections, runs the shared JS trimming core on text, rewrites clipboard when changed, and exposes a preferences UI + manual actions.

## Directory layout (current)
```
shell-extension/
  metadata.json
  extension.js        # compiled from src/extension.ts
  clipboard.js        # compiled from src/clipboard.ts
  trimmer.js          # compiled from src/trimmer.ts (wraps JS core)
  prefs.js            # compiled from src/prefs.ts (GTK4/libadwaita)
  src/
    extension.ts
    prefs.ts
    panel.ts          # top-bar menu UI
    clipboard.ts      # St.Clipboard adapter + manual paste entrypoints
    clipboardWatcher.ts # race-safe watcher core + manual paste flows
    virtualPaste.ts   # compositor virtual keyboard injection (Wayland)
    trimmer.ts        # wraps JS core trim()
  schemas/
    org.gnome.shell.extensions.trimmeh.gschema.xml
  stylesheet.css
```

## Clipboard hook
- Use `St.Clipboard.get_default()` and connect to `owner-change` for both `St.ClipboardType.CLIPBOARD` and `PRIMARY`.
- On change: `get_text(selection, callback)`; ignore non-text or empty strings.
- Pass text to the JS core `trim`; if `changed`, set text with `set_text()` and cache original per selection to support “Restore previous copy”.
- Tag writes with `hash` to avoid loops; skip if incoming text hash matches last written hash.
- Safety: skip blobs over `max_lines` (from settings).
- If `owner-change` is not available, fall back to polling.

## Trimming core
- The extension calls a runtime-agnostic JS core (string + RegExp only).
- Source: `trimmeh-core-js/src/index.ts`
- Adapter: `shell-extension/src/trimmer.ts` (keeps a stable `trimmer.trim()` interface for the watcher).

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
- `just bundle-extension` → bundles TS via `esbuild` and compiles schemas.
- `just install-extension` → installs to `~/.local/share/gnome-shell/extensions/trimmeh@trimmeh.dev/` and toggles the extension (log out/in if GNOME doesn’t reload it).
