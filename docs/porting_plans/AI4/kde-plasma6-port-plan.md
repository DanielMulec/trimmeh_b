# Trimmeh → KDE Plasma 6.4+ Port Plan (AI4)

Last updated: 2025-12-15

## Executive summary (recommended approach)

Build a **native, always-on KDE tray app** (Qt 6 + KDE Frameworks 6) that:

- Watches the clipboard, runs the **existing Trimmeh trimming core** (the exact same `trimmeh-core-js` used by the GNOME extension), and writes back trimmed text when it changes.
- Exposes **tray menu actions**: Paste Trimmed, Paste Original, Restore last copy, Toggle auto-trim, Preferences.
- Implements **global shortcuts** via **KGlobalAccel** (KDE-native).
- Implements **Wayland paste injection** for “Paste Trimmed/Original” via **xdg-desktop-portal RemoteDesktop** (EIS/libei). This is the closest KDE/Wayland analogue to Trimmy’s “requires Accessibility permission to paste”.
- Starts automatically via an **XDG autostart** entry (user toggle).

This is a “new KDE app” rather than a plasmoid “port”, because **parity requires always-on behavior** and a reliable shortcut + paste story.

---

## Parity contract (non-negotiables)

Parity means matching **Trimmy behavior as already implemented by Trimmeh** (repo root) plus Trimmy’s always-on lifecycle:

1. **Auto-trim**: watch clipboard changes, detect command-like multi-line snippets, and rewrite clipboard to trimmed output.
2. **Heuristics parity**: `trimmeh-core-js` output must match `tests/trim-vectors.json` for all aggressiveness levels and options.
3. **Aggressiveness**: low / normal / high; manual Paste Trimmed uses **high**.
4. **Toggles**: enable auto-trim, keep blank lines, strip prompts, strip box gutters, max-lines safety valve.
5. **Manual actions**: Paste Trimmed, Paste Original, Restore last copy (and never cause infinite loops).
6. **No network calls**, no telemetry.
7. **Always-on**: runs for the session (tray app) and can **start on login**.

---

## What the codebases tell us (why this design)

### Upstream Trimmy (macOS) behavior we must preserve conceptually

- Always-on menu-bar app with continuous clipboard monitoring.
- Uses a **grace delay (~80ms)** before reading clipboard content after it changes (promised data settling).
- “Paste Trimmed/Original” works by **temporary clipboard swap → paste → restore after ~200ms**.
- Paste requires **OS permission** (macOS Accessibility). On Wayland, paste injection similarly requires a privileged path.

### Trimmeh (GNOME 49+) parity mechanisms we must preserve

- Trimming logic is already split into a **platform-agnostic core** (`trimmeh-core-js`) and a **platform integration layer** (clipboard watcher, settings, paste injection).
- The clipboard watcher is a **race-safe state machine** (debounce, generation guard, self-write guard, restore guard).
- Hotkeys exist but are **disabled by default** and user-configurable.

The KDE implementation should mirror this separation: keep the core identical; replace only the platform layer.

---

## Architecture options considered (and why they’re rejected/accepted)

### Rejected: plasmoid-only (“widget”)
Not acceptable for parity because:
- It is not inherently “always-on” unless the user pins it and keeps it loaded.
- Global shortcuts and especially paste injection on Wayland are not a clean fit for a widget-only approach.

### Rejected: plasmoid + compiled plugin
Even if technically feasible, it complicates distribution (KDE Store generally expects QML/JS widgets; compiled parts typically need distro packaging), and it still doesn’t solve paste injection cleanly.

### Considered: KDED module
Viable long-term (background daemon managed by KDE), but **more moving parts** (service + UI) than needed for an initial parity port. A tray app already provides a persistent process plus UI.

### Accepted: tray app (“Trimmy-like”)
This maps 1:1 to Trimmy’s always-on UX and gives us a single place to own:
- clipboard monitoring
- trimming
- shortcuts
- settings
- paste injection permission UX
- autostart

---

## Proposed components

### 1) `trimmeh-kde/` (new): Qt 6 + KF6 tray app

Responsibilities:
- Clipboard watcher (auto-trim) + manual actions
- Settings persistence
- Global shortcuts
- StatusNotifier tray icon + menu

Suggested stack:
- **Tray icon**: `KStatusNotifierItem` (KDE-native)
- **Global shortcuts**: `KGlobalAccel`
- **Clipboard**: `QClipboard`
- **Settings**: `KConfig` (or `KConfigXT` if you want generated strongly-typed settings)
- UI: minimal **Qt Widgets** settings dialog *or* QML/Kirigami (either is fine; start with the simplest to ship)

### 2) Paste injection (Wayland): RemoteDesktop portal (EIS)

Implement a `PasteInjector` abstraction with two backends:

1. **Portal backend (preferred on Wayland)**  
   - Uses **`org.freedesktop.portal.RemoteDesktop`** to request keyboard injection permission.
   - On success, uses **EIS** (`ConnectToEIS`) + libei to send the paste keystroke sequence.
   - Stores and reuses the portal restore token if supported (so the user doesn’t get prompted repeatedly).
   - Surface a clear “permission not granted” status in Preferences.

2. **No-injection fallback (only when permission denied/unavailable)**  
   - Still performs clipboard swap → *shows a passive notification* telling the user to paste manually → restore after delay.
   - This keeps the clipboard semantics correct, but should be treated as a “permission missing” state, not a finished parity state.

Rationale: KWin scripting is great for window management and shortcuts, but **relying on it for generic keyboard injection is fragile/likely unavailable**; the portal path is the standardized Wayland route for “control input”.

---

## Core reuse: keep heuristics identical

Use `trimmeh-core-js` as the single trimming implementation for KDE too.

Practical integration options:

- **Option A (recommended): run core-js inside the KDE app via `QJSEngine`**  
  - Build a bundled, non-ESM JS artifact (e.g. `trimmeh-core.bundle.js`) during build.
  - Load it into `QJSEngine` once; call `TrimmehCore.trim(...)` for each clipboard event.
  - This avoids QML JS module limitations and keeps trimming centrally in one place.

- Option B: port the core to Rust-only and use FFI from C++  
  - Possible, but currently `trimmeh-core-js` is the best-tested parity surface in this repo (`tests/trim-vectors.json`).

---

## Clipboard watcher design (KDE parity details)

Implement the same state machine concepts as `shell-extension/src/clipboardWatcher.ts`:

- **Debounce / grace delay**: on clipboard change, schedule processing after ~80ms.
- **Generation guard**: if another clipboard change happens while we’re waiting/reading, abandon the stale task.
- **Self-write guard**: hash the text we write; ignore the next clipboard change if it matches.
- **Restore guard**: when doing manual paste/restore flows, ignore owner-change feedback for a short window.
- **Cache**: remember `lastOriginal` and `lastTrimmed` for “Paste Original” and “Restore last copy”.

Selections:
- Use `QClipboard::Clipboard` always.
- If `QClipboard::supportsSelection()` is true (X11), optionally also mirror PRIMARY (nice-to-have; not required for Wayland parity).

---

## UX: match expectations without overbuilding

### Tray menu (minimum viable parity UI)
- Toggle: Auto-trim (on/off)
- Submenu: Aggressiveness (Low/Normal/High)
- Actions: Paste Trimmed, Paste Original, Restore last copy
- Preferences…
- A one-line **“Last”** preview like Trimmeh’s panel summary (ellipsized)

### Preferences dialog
Required:
- Auto-trim toggle
- Aggressiveness selector
- Keep blank lines / Strip prompts / Strip box gutters toggles
- Max lines spinbox
- Hotkey configuration (three actions + enable/disable)
- “Start on login” toggle
- Paste injection status + “Grant permission” button (portal)

---

## Development plan (de-risk-first, parity-first)

### Phase 0 — Parity checklist + test contract
- Declare `tests/trim-vectors.json` as canonical trimming vectors.
- Add a KDE parity checklist (clipboard loop safety, restore behavior, shortcuts, portal permission UX).

### Phase 1 — Spike: tray app skeleton (clipboard read/write)
- Create `trimmeh-kde/` Qt app with tray icon and a menu action that reads + writes clipboard text.
- Validate on **Plasma 6.4 Wayland**.

### Phase 2 — Integrate `trimmeh-core-js` into the KDE app
- Bundle `trimmeh-core-js` into a JS artifact consumable by `QJSEngine`.
- Run `tests/trim-vectors.json` through that engine (in a small dev command or a unit test).

### Phase 3 — Implement auto-trim watcher + loop safety
- Implement the state machine and settings mapping.
- Confirm no infinite loops; confirm max-lines skip.

### Phase 4 — Manual actions (Restore / Paste Original / Paste Trimmed)
- Implement temporary clipboard swap + restore delay (~200ms).
- Add the “Last” preview string update behavior.

### Phase 5 — Wayland paste injection via portal (EIS)
- Implement portal session + permission UX.
- Implement keystroke injection (Shift+Insert or Ctrl+V), with a small post-hotkey delay.
- Confirm behavior in terminals + GUI apps.

### Phase 6 — Hotkeys, autostart, packaging
- Register actions with KGlobalAccel.
- Implement autostart entry toggle.
- Add Fedora packaging (RPM) once behavior is stable.

---

## Recommended repo layout (non-invasive)

Add a new top-level directory:

```
trimmeh-kde/
  CMakeLists.txt
  src/
  resources/   (icons, JS bundle, etc)
  packaging/   (future: rpm/flatpak manifests)
```

Keep the existing GNOME extension and CLI untouched.

---

## Why this plan is “parity-safe”

- The trimming algorithm stays **literally identical** to Trimmeh GNOME because it reuses `trimmeh-core-js`.
- The watcher + restore semantics are copied conceptually from Trimmeh’s proven state machine.
- The biggest parity risk (“paste on Wayland”) is addressed with the **standard portal path** rather than guessing about compositor-private APIs.

---

## Primary references (authoritative docs)

- Qt clipboard API (`QClipboard`): https://doc.qt.io/qt-6/qclipboard.html
- KDE global shortcuts (`KGlobalAccel`) API: https://api.kde.org/frameworks/kglobalaccel/html/index.html
- KDE tray item (`KStatusNotifierItem`) API: https://api.kde.org/frameworks/kstatusnotifieritem/html/classKStatusNotifierItem.html
- Plasma 6 widget porting notes: https://develop.kde.org/docs/plasma/widget/porting_kf6/
- Plasma widget C++ note (distribution constraints): https://develop.kde.org/docs/plasma/widget/c-api/
- KWin scripting API (capabilities overview): https://develop.kde.org/docs/plasma/kwin/api/
- xdg-desktop-portal RemoteDesktop: https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.RemoteDesktop.html
- libei (EIS input emulation): https://libinput.pages.freedesktop.org/libei/
- KDE portal pre-authorization (avoid repeated prompts): https://develop.kde.org/docs/plasma/portal-preauthorization/

