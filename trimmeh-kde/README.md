# Trimmeh KDE (Plasma 6)

This is the beginning of a KDE/Plasma port of Trimmeh as a **KF6/Qt6 tray app**.

Current status:
- Tray icon + menu (toggle auto-trim, toggles, aggressiveness)
- Clipboard watching + auto-trim (Qt `QClipboard` + race/loop guards)
- Manual actions currently perform a temporary clipboard swap and restore (paste injection via portal is planned but not implemented yet)

## Build (Fedora / Plasma 6)

Prereqs:
- Qt 6 (Core/Gui/Widgets/Qml)
- KDE Frameworks 6: `KStatusNotifierItem`, `KConfig` (hotkeys via `KGlobalAccel` are planned)
- Node.js + `npx` (used to bundle `trimmeh-core-js` into a single JS file for `QJSEngine`)

From repo root:

```sh
cmake -S trimmeh-kde -B build-kde -DCMAKE_BUILD_TYPE=Debug
cmake --build build-kde
./build-kde/trimmeh-kde
```

Vector parity check (runs `tests/trim-vectors.json` through the embedded JS engine):

```sh
./build-kde/trimmeh-kde --check-vectors tests/trim-vectors.json
```

Notes:
- The trimming logic is reused from `trimmeh-core-js` and is bundled during the build.
- “Paste Trimmed/Original” currently swaps the clipboard briefly; reliable Wayland paste injection is planned via `xdg-desktop-portal` RemoteDesktop.

## Testing

See `trimmeh-kde/TESTING.md`.
