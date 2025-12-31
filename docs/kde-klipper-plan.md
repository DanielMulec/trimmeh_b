# Trimmeh KDE Port (Klipper D-Bus) — Plan + Trimmy Parity Spec

Target platform: **Plasma 6.5.4** primary.  
Baseline: **Plasma ≥ 6.4** (we can take older versions along if easy, but we do not optimize for < 6.4).

Goal: **full Trimmy parity (functionality + UI + UX)** on KDE/Plasma, using Klipper as the clipboard authority via D-Bus. GNOME work remains untouched.

---

## 0) Current status (2025-12-31)

Implemented and verified on **Plasma 6.5.4**:
- **Phase 0 probe**: `trimmeh-kde-probe` can read/write via Klipper DBus and receives `clipboardHistoryUpdated`.
- **Auto-trim**: working via Klipper DBus with debounce + self-write hash guard.
- **JS core**: `trimmeh-core-js` bundled for KDE (QJSEngine) via `trimmeh-core-js/src/kde-entry.ts`.
- **Tray UI (minimal)**: KStatusNotifierItem menu with a hotkey-permission callout (when portal not ready), plus:
  - Paste Trimmed (High)
  - Paste Original
  - Restore last copy
  - Auto‑Trim toggle
  - Last preview line
  - Settings…
  - Quit
- **Tray previews**: trimmed preview line + original preview line (plain text) with visible whitespace markers, plus a “Removed: N chars” line when applicable.
- **Tray stats**: paste actions append live character counts and trimmed counts (e.g. `· 1.2k chars · 340 trimmed`).
- **Tray shortcut hints**: menu shows configured shortcut hints when hotkeys are enabled.
- **Preferences window (Qt Widgets)**:
  - Tabs: General, Aggressiveness, Shortcuts (wired), About.
  - General: hotkey-permission group (status + “Enable Hotkeys Permanently” + “Open System Settings”), Auto‑trim, Keep blank lines, Strip box chars, Strip prompts, Paste timing (restore delay), Start at Login, Quit. “Extra clipboard fallbacks” is functional. CLI installer section present.
  - Aggressiveness: Low/Normal/High with live preview (Before/After).
  - About: icon + version + tagline + links (GitHub/Website/Twitter/Email) + update controls (disabled, package-manager note).
- **Manual paste swap**: clipboard swap → portal paste injection (Shift+Insert → Ctrl+V fallback) after a short delay → timed restore.
- **Settings persistence**: QSettings persists auto‑trim, keep blank lines, strip box chars, strip prompts, max lines, aggressiveness, start‑at‑login, paste restore delay, paste inject delay, hotkey toggles + sequences, and portal restore tokens.
- **Autostart wiring**: “Start at Login” creates/removes `~/.config/autostart/dev.trimmeh.TrimmehKDE.desktop` (legacy `trimmeh-kde.desktop` cleaned up).
- **Portal paste injection**: xdg-desktop-portal RemoteDesktop session + keyboard injection (Shift+Insert → Ctrl+V fallback).
- **App identity**: stable app ID `dev.trimmeh.TrimmehKDE`, desktop file ensured in `~/.local/share/applications`, and portal Registry registration at startup.
- **Permission UX**: tray info row + “Enable Hotkeys Permanently”; settings adds “Open System Settings”. Portal prompt is triggered by paste actions when needed; permission-denied/unavailable states show a manual “Paste now” hint.
- **Pre‑authorization**: uses KDE `kde-authorized` permission store via `flatpak permission-set` from the UI, checks permission store on launch, and auto‑requests session when pre‑authorized (no prompt).
- **Global hotkeys**: KGlobalAccel wired + shortcuts tab (persisted; no defaults assigned yet).
- **Restore safety**: restore guard prevents auto-trim loops after manual paste/restore.
- **Configurable restore delay**: preference for clipboard restore timing (persisted).
- **Clipboard fallbacks**: optional QClipboard/plain+HTML fallback path when Klipper returns empty.

Not yet implemented:
- **Frontmost app label** in tray menu (explicitly skipped for KDE).
- **Real updater plumbing** (menu row exists but is hidden; settings show package‑manager note).

Temporary deviations:
- **Paste restore delay** defaults to **1200 ms** (configurable 50–2000 ms). Trimmy parity target is **200 ms**.
- **Paste inject delay** is fixed at **120 ms** (persisted but not exposed in the UI).

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
- Fail gracefully if the service is missing; surface a UI warning and disable auto-trim. (Current: app exits on init failure; no UI warning yet.)

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
- If permission not granted: still swap clipboard and request permission; no passive “Paste now” hint yet, then restore.

### F. Hotkeys and autostart
Use `KGlobalAccel` for global shortcuts and an autostart desktop entry toggle.

### G. App identity + portal pre‑authorization
- Set a stable app ID (`dev.trimmeh.TrimmehKDE`) and ensure a matching desktop file.
- Register with `org.freedesktop.host.portal.Registry` at startup.
- Allow users to pre‑authorize via `flatpak permission-set kde-authorized remote-desktop dev.trimmeh.TrimmehKDE yes`.
- On launch, check `kde-authorized` and auto‑request a session only when pre‑authorized.

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
Status: **Tray menu done (minimal)**; **Preferences implemented** (General + Aggressiveness + Shortcuts + About; settings persisted)

**Phase 3 — Manual paste parity**
- `Paste Trimmed` and `Paste Original` using swap → paste → restore.
- Permission callout and portal prompt flow via paste actions.
Status: **Done** (swap/restore + portal injection + permission UX)

**Phase 4 — QA + release hygiene**
- Parity test checklist and manual test plan.
- Vector tests for the JS core.
- Autostart and config persistence.
Status: **Autostart + persistence done**; **QA checklist + integration plan documented; KDE vector harness available**

---

## 3.5) Suggested next steps (priority order)
1. **Parity UI polish**: frontmost app label (if we ever decide to add it on KDE).
2. **QA pass**: run the manual checklist + integration test plan for KDE.

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
   KDE current: hotkey‑permission wording; tray shows info + “Enable Hotkeys Permanently”; settings adds “Open System Settings”. No explicit “Grant Permission” button.
2. **Paste Trimmed to {Front App}** button
   - Shows keyboard shortcut if enabled.
   - Stats suffix: `· {N chars}` plus `· {M trimmed}` when applicable.
   - Preview line: monospaced, 2 lines max, middle truncation, visible whitespace (`⏎`, `⇥`).
3. **Paste Original to {Front App}** button
   - Shows keyboard shortcut if enabled.
   - Preview line uses strike‑through on characters removed by trimming.
   - KDE note: strike‑through isn’t possible in the Plasma tray menu; we show a plain‑text preview plus “Removed: N chars”.
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
  - KDE current: trimmed + original preview lines (plain text) with visible whitespace markers; middle ellipsize at 100 chars; optional “Removed: N chars” line.

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

### B.1 KDE current defaults (from code)
- Aggressiveness: **normal**
- Auto‑trim enabled: **true**
- Keep blank lines: **false**
- Strip box chars: **true**
- Strip prompts: **true**
- Max lines: **10**
- Grace delay: **80 ms** (fixed)
- Paste restore delay: **1200 ms** (configurable 50–2000 ms)
- Paste inject delay: **120 ms** (persisted, not exposed in UI)
- Start at login: **false**
- Paste Trimmed hotkey enabled: **true** (sequence empty by default)
- Paste Original hotkey enabled: **false** (sequence empty by default)
- Auto‑Trim toggle hotkey enabled: **false** (sequence empty by default)

### C. Settings window (tabs)
Tab view with fixed size: **410 × 484**.

**General**
- Hotkey permission group (status + “Enable Hotkeys Permanently” + “Open System Settings”).
- Toggles:
  - “Auto‑trim enabled”
  - “Keep blank lines”
  - “Remove box drawing chars (│┃)”
  - “Strip prompts”
  - “Use extra clipboard fallbacks”
  - (Debug builds only) “Enable debug tools”
- Paste timing:
  - “Restore delay” (50–2000 ms).
- CLI installer section:
  - Button: “Install CLI”
  - Text: “Install `trimmeh` into ~/.local/bin.”
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
- Links: GitHub, Website, Twitter, Email (Email link not implemented yet in KDE)
- Update controls (disabled with package‑manager note):
  - “Check for updates automatically” toggle
  - “Check for Updates…” button
- Copyright line

**Debug** (debug builds only)
- Toggle: “Enable debug tools”
- Buttons:
  - “Load strikeout sample”
  - “Trigger trim animation”

### D. Behavioral parity (timing + messaging)
- **Grace delay**: 80 ms before reading clipboard after change.
- **Paste restore delay**: configurable 50–2000 ms, default **1200 ms**. Trimmy parity target is **200 ms**.
- **Paste inject delay**: **120 ms** before portal paste injection.
- **Pre‑authorization**: when `kde-authorized` is present, hide permission buttons and auto‑request the session on launch (no prompt).
- **Permission message on failure**:
  - Trimmy uses: “Enable Accessibility to let Trimmy paste (System Settings → Privacy & Security → Accessibility).”
  - KDE current: hotkey-permission messaging plus manual “Paste now” hint when denied/unavailable.
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
- Update UX: on Linux, do we hide update UI or wire it to distro package updates?

---

## 7) Done criteria for “parity”

- All Trimmy UI/UX elements above are present and behave equivalently.
- All Trimmy defaults match (including hotkey toggles).
- Manual paste and restore semantics mirror Trimmy (swap → paste → restore).
- Parity vectors (`tests/trim-vectors.json`) match on KDE via core‑js.
- No network calls; robust on Plasma 6.5.4.
