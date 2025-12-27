# Trimmeh KDE Port (Klipper D-Bus) — Plan + Trimmy Parity Spec

Target platform: **Plasma 6.5.4** primary.  
Baseline: **Plasma ≥ 6.4** (we can take older versions along if easy, but we do not optimize for < 6.4).

Goal: **full Trimmy parity (functionality + UI + UX)** on KDE/Plasma, using Klipper as the clipboard authority via D-Bus. GNOME work remains untouched.

---

## 0) Current status (2025-12-27)

Implemented and verified on **Plasma 6.5.4**:
- **Phase 0 probe**: `trimmeh-kde-probe` can read/write via Klipper DBus and receives `clipboardHistoryUpdated`.
- **Auto-trim**: working via Klipper DBus with debounce + self-write hash guard.
- **JS core**: `trimmeh-core-js` bundled for KDE (QJSEngine) via `trimmeh-core-js/src/kde-entry.ts`.
- **Tray UI (minimal)**: KStatusNotifierItem menu with:
  - Paste Trimmed (High)
  - Paste Original
  - Restore last copy
  - Auto‑Trim toggle
  - Last preview line
  - Settings…
  - Quit
- **Preferences window (Qt Widgets)**:
  - Tabs: General, Aggressiveness, Shortcuts (placeholder), About.
  - General: Auto‑trim, Keep blank lines, Strip box chars, Strip prompts. “Extra clipboard fallbacks” + “Start at Login” are present but disabled.
  - Aggressiveness: Low/Normal/High with live preview (Before/After).
- **Manual paste swap**: clipboard swap → user paste → timed restore works (no keystroke injection yet).

Not yet implemented:
- **Paste injection** (portal permission + actual keystroke injection).
- **Hotkeys** (KGlobalAccel).
- **Autostart** toggle + persistence.
- **Settings persistence** (currently in‑memory only; no QSettings storage).
- **Parity UI polish** (frontmost app label, strike‑through preview, stats badges).
- **Permission callouts** for paste/input access.

Temporary deviations:
- **Paste restore delay** is currently **1200 ms** (to make manual timing easy). Trimmy parity target is **200 ms**. We should return to 200 ms or make it configurable once paste injection exists.

---

## 1) Klipper D-Bus surface (authoritative)

Klipper registers a session-bus service and object we can call directly:

- **Service**: `org.kde.klipper`
- **Object path**: `/klipper`
- **Interface**: `org.kde.klipper.klipper`
- **Signal**: `clipboardHistoryUpdated()`
- **Clipboard APIs** (string-based):
  - `QString getClipboardContents()`
  - `void setClipboardContents(QString s)`
  - `void clearClipboardContents()`
  - `void clearClipboardHistory()`
  - `void saveClipboardHistory()`
  - `QString getClipboardHistoryMenu()`
  - `QString getClipboardHistoryItem(int i)`
  - `void showKlipperPopupMenu()`
  - `void showKlipperManuallyInvokeActionMenu()`
  - `void reloadConfig()`

Source: `plasma-workspace/klipper/klipper.h` and `klipper.cpp` in Plasma 6.3.x (the service/object registration is explicit in code).

**Design consequence:** we can avoid `QClipboard` entirely for reads/writes and listen to `clipboardHistoryUpdated` for change detection.

---

## 2) Architecture plan (Klipper-first)

### A. `KlipperBridge` (QtDBus)
Responsibilities:
- Connect to session bus and verify `org.kde.klipper` is present.
- Subscribe to `clipboardHistoryUpdated`.
- Provide typed wrappers:
  - `getClipboardText()` → `getClipboardContents`
  - `setClipboardText(text)` → `setClipboardContents`
  - `clearClipboard()`, `historyItem(i)` if needed
- Fail gracefully if the service is missing; surface a UI warning and disable auto-trim.

### B. `ClipboardWatcher` (race-safe state machine)
Mirror the GNOME/Trimmy race-safety model:
- **Debounce / grace delay**: ~80 ms after a change (matches Trimmy).
- **Generation guard**: only latest change can write.
- **Self-write guard**: hash the text we write, ignore matching signal.
- **Restore guard**: ignore changes triggered by manual paste/restore flows.

Flow:
1. `clipboardHistoryUpdated` → start debounce (80 ms).
2. Read clipboard text via `getClipboardContents`.
3. Run `trimmeh-core-js` trim (Normal aggressiveness unless forced).
4. If changed: write via `setClipboardContents`.
5. Update cached `lastOriginal` / `lastTrimmed`, menu previews, and stats.

### C. `TrimCore` integration
Use **`trimmeh-core-js`** for parity (same vectors as GNOME):
- Bundle to a single JS file consumed by `QJSEngine`.
- Expose a stable `trim(text, aggressiveness, options)`.
- Validate parity with `tests/trim-vectors.json`.

### D. Tray UI + Preferences
Use `KStatusNotifierItem` for a Trimmy-like always-on tray menu.
Preferences window: Qt Widgets or QML, but **match Trimmy’s layout and copy** (see parity spec below).

### E. Paste injection (Wayland)
For **Paste Trimmed/Original** parity:
- Use **xdg-desktop-portal RemoteDesktop** for input injection.
- If permission not granted: still swap clipboard, show a passive “Paste now” hint, then restore.

### F. Hotkeys and autostart
Use `KGlobalAccel` for global shortcuts and an autostart desktop entry toggle.

---

## 3) Phased delivery plan

**Phase 0 — DBus probe (1–2 days)**
- Minimal QtConsole app that:
  - Connects to `org.kde.klipper` and `/klipper`.
  - Listens to `clipboardHistoryUpdated`.
  - Logs `getClipboardContents`.
- Confirms D-Bus contract on **Plasma 6.5.4**.
Status: **Done** (`trimmeh-kde-probe`)

**Phase 1 — Headless auto-trim**
- Implement `KlipperBridge` + `ClipboardWatcher`.
- Integrate `trimmeh-core-js` (QJSEngine).
- Auto-trim behavior and guards only (no UI).
Status: **Done** (now runs inside the tray app)

**Phase 2 — Tray UI + Preferences**
- Tray menu, preview, toggles, actions.
- Preferences window with tabs (General / Aggressiveness / Shortcuts / About).
Status: **Tray menu done (minimal)**; **Preferences implemented** (General + Aggressiveness + About; Shortcuts placeholder; no persistence)

**Phase 3 — Manual paste parity**
- `Paste Trimmed` and `Paste Original` using swap → paste → restore.
- Permission callout and “grant permission” flow via portal.
Status: **Swap/restore done**; **portal paste injection + permission UX pending**

**Phase 4 — QA + release hygiene**
- Parity test checklist and manual test plan.
- Vector tests for the JS core.
- Autostart and config persistence.
Status: **Pending**

---

## 4) Trimmy parity spec (UI/UX + behavior)

This is the **canonical UI/UX parity checklist** we will implement in KDE.
All strings, defaults, and behaviors below are derived from Trimmy’s upstream UI.

### A. Tray menu (Trimmy MenuContentView)
Layout order:
1. **Permission callout** (only when paste permission missing):
   - Title: “Accessibility needed to paste”
   - Subtitle: “Enable Trimmy in System Settings → Privacy & Security → Accessibility so ⌘V can be sent to the front app.”
   - Buttons: “Grant Accessibility”, “Open Settings”  
   KDE adaptation: replace with portal‑specific text (“Input permission needed to paste”) and buttons (“Grant Permission”, “Open Settings”).
2. **Paste Trimmed to {Front App}** button
   - Shows keyboard shortcut if enabled.
   - Stats suffix: `· {N chars}` plus `· {M trimmed}` when applicable.
   - Preview line: monospaced, 2 lines max, middle truncation, visible whitespace (`⏎`, `⇥`).
3. **Paste Original to {Front App}** button
   - Shows keyboard shortcut if enabled.
   - Preview line uses strike‑through on characters removed by trimming.
4. Divider
5. **Auto‑Trim** toggle
6. **Settings…**
7. **About Trimmy**
8. **Update ready, restart now?** (only when updater says ready)

Preview details:
- `MenuPreview.limit = 100`.
- `lastSummary` uses `ellipsize(..., limit: 90)` after a trim or manual action.
- Trimmed preview uses `PreviewMetrics.displayString` (maps `\n` → `⏎`, `\t` → `⇥`).
- Strike‑through preview uses visible whitespace mapping (space → `·`, tab → `⇥`, newline → `⏎`).

### B. Defaults (Trimmy AppSettings)
- Aggressiveness: **Normal**
- Auto‑trim enabled: **true**
- Keep blank lines: **false**
- Remove box drawing chars: **true**
- Use extra clipboard fallbacks: **false**
- Start at login: **false**
- Paste Trimmed hotkey enabled: **true**
- Paste Original hotkey enabled: **false**
- Auto‑Trim toggle hotkey enabled: **false**

### C. Settings window (tabs)
Tab view with fixed size: **410 × 484**.

**General**
- Permission callout (same as menu).
- Toggles:
  - “Auto‑trim enabled”
  - “Keep blank lines”
  - “Remove box drawing chars (│┃)”
  - “Use extra clipboard fallbacks”
  - (Debug builds only) “Enable debug tools”
- CLI installer section:
  - Button: “Install CLI”
  - Text: “Install `trimmy` into /usr/local/bin and /opt/homebrew/bin.”
  KDE adaptation: install `trimmeh` into `~/.local/bin` (or system‑wide if permitted).
- Divider
- Toggle: “Start at Login”
- Button: “Quit Trimmy” (prominent)

**Aggressiveness**
- Radio group:
  - Low (safer)
  - Normal
  - High (more eager)
- Blurb text per selection (from TrimmyCore Aggressiveness).
- Explanatory paragraph:
  - “Automatic trimming uses this aggressiveness level… Manual ‘Paste Trimmed’ always runs at High…”
- Preview cards (“Before”, “After”) with sample text.

**Shortcuts**
- Toggle + recorder for each:
  - “Enable global ‘Paste Trimmed’ hotkey”
  - “Enable global ‘Paste Original’ hotkey”
  - “Enable global Auto‑Trim toggle hotkey”
- Footnote: “Paste Trimmed always uses High aggressiveness and then restores your clipboard.”

**About**
- App icon and version
- “Paste‑once, run‑once clipboard cleaner for terminal snippets.”
- Links: GitHub, Website, Twitter, Email
- Update controls:
  - “Check for updates automatically” toggle
  - “Check for Updates…” button (if updater available)
- Copyright line

**Debug** (debug builds only)
- Toggle: “Enable debug tools”
- Buttons:
  - “Load strikeout sample”
  - “Trigger trim animation”

### D. Behavioral parity (timing + messaging)
- **Grace delay**: 80 ms before reading clipboard after change.
 - **Paste restore delay**: 200 ms after paste (Trimmy parity target).  
   Current KDE build uses **1200 ms** for easier manual timing; revert or make configurable once paste injection lands.
- **Permission message on failure**:
  - Trimmy uses: “Enable Accessibility to let Trimmy paste (System Settings → Privacy & Security → Accessibility).”
  - KDE adaptation: mention portal permission path.
- **“Nothing to paste.”** on manual paste when clipboard is empty.
- **“No actions yet”** in preview when nothing has ever been trimmed.

---

## 5) KDE + Klipper specifics (parity mapping)

**Auto‑trim loop**
- Trigger: `clipboardHistoryUpdated`
- Read: `getClipboardContents`
- Write: `setClipboardContents`
- Guard: hash self‑writes and manual swaps.

**Restore last copy**
- Use in‑memory `lastOriginal` like Trimmy.
- Do **not** dig into Klipper history unless needed; parity with Trimmy is in‑memory only.

**Extra clipboard fallbacks**
- Trimmy uses RTF/public types when plain text missing.
- Klipper D-Bus is string‑only. We can:
  - Keep the toggle and attempt a secondary read path (optional, via QClipboard or portal), or
  - Clearly label it as “Best effort” and log when unavailable.

---

## 6) Open questions / risk list

- Does Klipper’s D‑Bus signal always fire on all clipboard changes in Plasma 6.5.4? **Confirmed yes** via probe.
- Does `getClipboardContents` always return text even when the clipboard holds rich content?
- Portal permissions: best UX for “Grant Permission” on KDE/Wayland.
- Update UX: on Linux, do we hide update UI or wire it to distro package updates?

---

## 7) Done criteria for “parity”

- All Trimmy UI/UX elements above are present and behave equivalently.
- All Trimmy defaults match (including hotkey toggles).
- Manual paste and restore semantics mirror Trimmy (swap → paste → restore).
- Parity vectors (`tests/trim-vectors.json`) match on KDE via core‑js.
- No network calls; robust on Plasma 6.5.4.
