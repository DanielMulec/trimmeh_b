# Trimmeh → KDE Plasma Port Plan (Plasma 6.4+)

Last updated: 2025-12-14

## Why this doc exists

Trimmeh is currently a GNOME Shell extension + CLI with trimming heuristics aligned with upstream Trimmy.
This document proposes a KDE Plasma port approach that keeps feature parity with Trimmy while reusing Trimmeh’s existing JS/TS trimming core (`trimmeh-core-js/`).

This is written for a beginner maintainer: it calls out “gotchas” and includes decision checkpoints.

---

## Goals (parity requirements)

**Must match Trimmy user-facing behavior:**

1. **Auto-trim:** Watch clipboard changes, detect “command-like” multi-line snippets, and rewrite clipboard to the trimmed output when changed.
2. **Aggressiveness levels:** Low / Normal / High; manual actions use High.
3. **Toggles:** keep blank lines, strip prompts, strip box-gutters, enable/disable auto-trim, max lines safety limit.
4. **Manual actions:** “Paste Trimmed”, “Paste Original”, “Restore previous copy” equivalents.
5. **Loop safety:** Avoid infinite clipboard loops and stale overwrites (race-safe design).
6. **No network calls.**
7. **Start on login (practical parity):** Trimmy is an always-on menu-bar app; the KDE port must be able to run automatically when the session starts (tray app autostart or equivalent service).

**Target platform:**
- KDE Plasma **6.4+** (Wayland-first; test also on 6.5).

## What “parity” concretely means (from Trimmy’s macOS implementation)

Re-reading upstream Trimmy shows that “parity” isn’t just “a trimming function” — it includes always-on behavior and hotkeys:

- **Always-on app lifecycle:** Trimmy starts its clipboard monitor immediately on app init and runs continuously as a menu-bar app.
- **Global hotkeys exist and are user-configurable:** Trimmy has explicit settings toggles to enable/disable hotkeys, and users can record a shortcut for each action.
- **Hotkeys map to *paste* actions, not just “copy”:** hotkeys call `pasteTrimmed()` / `pasteOriginal()`.
- **Paste semantics:** for paste actions it temporarily swaps clipboard contents, performs paste, then restores the previous clipboard after a short delay (best-effort).
- **Race/loop avoidance:** Trimmy tags its own pasteboard writes with a marker type and ignores its own changes.
- **Starts at login is an explicit setting in Trimmy:** for KDE, we need an equivalent (autostart entry or service).

---

## Biggest “between the lines” constraints (things easy to miss)

### 1) KDE vs GNOME privilege model (Wayland)
GNOME Trimmeh runs *inside* `gnome-shell` (a privileged compositor process), so clipboard watching/writing is straightforward.

In KDE, a **plasmoid runs inside `plasmashell`**, not inside the compositor (`kwin_wayland`). On Wayland, clipboard/keyboard injection is security-sensitive. Depending on compositor policy and protocols, a random background client might not get the same capabilities as the compositor or the clipboard manager.

Practical implication: **we need an early spike** to confirm what we can do from a plasmoid on Plasma 6.4 Wayland:
- Can we reliably **observe clipboard changes**?
- Can we reliably **read text** content (not just history)?
- Can we reliably **write text** back?

If this is flaky, the port should pivot to a small **background component** (KDED module or Klipper-integrated helper) and keep the plasmoid as UI only.

### 2) “Paste Trimmed” on Wayland is hard everywhere
“Paste” means: set clipboard temporarily **and** simulate a paste key event into the focused app.

On GNOME we implemented a best-effort compositor virtual keyboard path. On KDE, the equivalent likely needs:
- a **KWin script** (runs inside compositor) to synthesize key events, or
- an external tool/daemon (undesirable), or
- accepting a UX compromise (“Copy Trimmed” + user manually pastes) if we can’t do reliable injection.

This is the #1 parity risk and needs a dedicated investigation step.

### 3) Global shortcuts are not “free” in plasmoids
GNOME extensions register keybindings via GNOME Shell. In KDE, global shortcuts are typically handled via **KGlobalAccel** (C++ / KF6).

If we want Trimmy-like hotkeys, there are two realistic routes:
- **Script-only (Store-friendly):** ship a small **KWin script** that uses KWin’s `registerShortcut(...)` to register global shortcuts and then triggers Trimmeh actions via DBus calls.
- **Native app/service:** implement hotkeys via **KF6::GlobalAccel** (typically C++), which is a better fit for a tray app or KDED module than for a plasmoid.

Important distribution constraint: KDE’s docs note that widgets which require compilation are not acceptable for KDE Store delivery (so a “plasmoid + compiled plugin” is a packaging trap unless we commit to distro/Flatpak packaging).

### 4) Don’t accidentally inherit GPL
KDE/Plasma source code often uses GPL-2.0-or-later. We can *use APIs* freely, but we should **avoid copying** implementation code from Plasma/klipper into Trimmeh if we want to keep Trimmeh MIT-only.

---

## Architecture decision (parity-first)

Because parity requires:
- **always-on behavior**, even if no widget is added, and
- **global hotkeys** that trigger actions,

…the simplest KDE model is a **regular always-running tray app (StatusNotifierItem)**, optionally paired with a **KWin script** for Wayland paste-injection.

A **plasmoid can still exist** as an optional UI surface, but it should not be the only runtime component.

---

## Proposed architecture (recommended): Tray app + optional KWin helper for paste

### Summary
Build an always-on **KDE tray app** (Qt 6 + KF6) that:
- watches clipboard changes (Wayland-safe),
- runs the existing **JS/TS trimming core**,
- writes trimmed text back to the clipboard (hash-guarded to avoid loops),
- provides a tray menu + settings UI,
- registers global shortcuts (Paste Trimmed / Paste Original / Toggle Auto-Trim).

Add an optional **KWin helper** only if we need compositor-side capabilities (most likely: best-effort paste injection on Wayland).

### Components

1. **`trimmeh-core-js` (existing)**
   - Source of truth for trimming behavior used by GNOME extension.
   - We’ll add a build output specifically compatible with QML’s JS import style.

2. **`trimmeh-kde` (new, tray app)**
   - Always-on background app with StatusNotifierItem tray icon:
     - Clipboard watcher + trimming pipeline (race-safe)
     - Settings UI (Kirigami/QML preferred)
     - Global shortcuts registration
     - DBus surface (recommended for debugging + for calling from a KWin script)

3. **Optional: `trimmeh-kde-kwin` (new, KWin script)**
   - Best-effort paste injection helper (Wayland): triggers “paste now” in the focused app.
   - Called by `trimmeh-kde` over DBus when handling Paste Trimmed/Original actions.
   - Not needed for hotkeys or clipboard; only for paste injection if a normal client app can’t do it.

---

## Tech stack decisions

### App / UI
- **Tray app**: Qt 6 + KDE Frameworks 6.
- UI: **QML + Kirigami** (recommended) or Qt Widgets (fallback).
- Tray integration: **StatusNotifierItem** (KDE-native).

### Clipboard access
- Use Qt’s `QClipboard` and `QMimeData` to:
  - observe changes,
  - read plain text,
  - skip HTML/binary targets (parity safety),
  - write back trimmed text.

### Trimming core reuse
- Keep algorithm source in `trimmeh-core-js/src/index.ts`.
- Add a build step that outputs a **single QML-importable JS file** (usable both by a QML app and by a plasmoid):
  - QML-friendly (no Node APIs, no dynamic `require`, avoid ESM `export` if QML can’t import ESM reliably across distro Qt versions).
  - Prefer a “library-style” JS file with `function trimmehTrim(...) { ... }`.

### Hotkeys
Implement hotkeys in the tray app via **KF6::GlobalAccel** (KGlobalAccel).

Note for future Flatpak work: there is an xdg-desktop-portal “GlobalShortcuts” portal that can replace KGlobalAccel in sandboxed environments, but it’s not required for the native app path.

---

## Rejected (for now): “plasmoid + KWin script” route

This is the “all-script, KDE Store-friendly” idea. We’re not choosing it because parity requires always-on behavior and clean hotkey UX, which is simpler as a tray app.

What it would likely look like for users:

1. Install the **Trimmeh plasmoid** (widget) from “Get New Widgets…”.
2. Install the **Trimmeh KWin script** from “Get New Scripts…” (KWin Scripts).
3. Enable the KWin script in System Settings.
4. Configure the global shortcuts in KDE’s shortcuts UI (or via the KWin script config, if we implement one).

Key caveat: KDE’s tooling treats widgets and KWin scripts as separate installables; there’s no reliable “install my dependency automatically” story, so the best we can do is document the steps clearly. If we care about “single install”, packaging a tray app (or service) wins.

### Paste injection
Decision checkpoint after Spike C (below):
- Preferred: implement best-effort paste injection via **KWin script** (Wayland, compositor-side).
- Fallback: “Copy Trimmed” + a passive notification/toast “Now paste manually (Ctrl+V)” if injection is not feasible.

Important learning point: on **Wayland**, a normal application generally **cannot** send synthetic keystrokes to other apps (by design). GNOME Trimmeh can because it runs inside the compositor (`gnome-shell`). On KDE, we may need compositor-side help (KWin) to get “Paste Trimmed/Original” parity.

---

## Step-by-step plan (tray app path)

### Phase 0 — Pre-flight (parity + repo hygiene)
1. Confirm trimming parity baseline:
   - Ensure `tests/trim-vectors.json` is canonical and GNOME extension is passing it.
2. Define KDE parity acceptance checklist (same as GNOME QA checklist + KDE-specific points).

Deliverable: written acceptance checklist section in this doc (see “QA checklist”).

---

### Phase 1 — Spike A: minimal tray app proves clipboard + hotkeys (Wayland)
Goal: de-risk the two parity-critical KDE integrations (clipboard watch + global shortcuts) early.

1. Create a minimal Qt/KF6 tray app:
   - Displays current clipboard text preview in its menu.
   - Logs whenever clipboard changes.
   - Has a menu action to set clipboard to “hello from trimmeh”.
2. Register a global shortcut (KGlobalAccel) to run that action.
3. Test on Plasma 6.4 and 6.5 (Wayland):
   - Change detection reliability
   - Plain text read/write reliability across apps
   - Shortcut registration + delivery

Deliverable: spike app + notes of what worked/failed.

---

### Phase 2 — Build system + app skeleton
Goal: a real always-on app that can be packaged and installed.

1. Add `trimmeh-kde/` (CMake project) with a minimal Qt app bootstrapping QML.
2. Add repo automation:
   - `just kde-build` / `just kde-run`
   - `just kde-install` (prefix install under `~/.local` for dev)
3. Add settings persistence (KConfig or QSettings).
4. Add “Start on login” toggle (autostart `.desktop` file).

Deliverable: runnable tray app with settings window.

---

### Phase 3 — Integrate trimming core + implement watcher logic (auto-trim)
Goal: match GNOME auto-trim semantics and race safety.

1. Add compiled trimming core JS into the plasmoid package.
2. Implement the watcher state machine (one per selection if supported):
   - Debounce clipboard-changed events.
   - Read current text.
   - If too many lines: skip (configurable max).
   - Hash-guard own writes to avoid loops.
   - If enabled: run `trim()` and write back if changed.
3. Respect settings:
   - aggressiveness: low/normal/high
   - keep blank lines
   - strip prompts
   - strip box chars
   - max lines

Deliverable: auto-trim working without loops.

---

### Phase 4 — Manual actions + UX parity
Goal: bring Trimmy parity features beyond auto-trim.

1. Implement “Restore previous copy”:
   - Keep last original text (per mode).
   - Allow restoring it and guard against auto-trim re-trigger.
2. Implement “Paste Original” / “Paste Trimmed” flows:
   - Read current clipboard.
   - Compute trimmed (High) or use cached original.
   - Temporarily set clipboard to chosen text.
   - Attempt paste injection (Phase 5).
   - Restore clipboard to previous contents after a short delay.
3. Add a panel widget UI (compact):
   - Toggle auto-trim
   - Actions menu (Paste Trimmed/Original/Restore)
   - Small last-action preview (optional parity polish)

Deliverable: feature parity minus paste injection reliability.

---

### Phase 5 — Spike C: paste injection on Wayland (KWin script?)
Goal: best-effort paste injection comparable to GNOME.

1. Investigate KWin scripting APIs for synthesizing key events.
2. If viable:
   - Create a small KWin script package that exposes a DBus method:
     - `paste()` (does Shift+Insert, fallback Ctrl+V)
   - Trimmeh plasmoid calls that DBus method during manual actions.
3. If not viable:
   - Decide on fallback UX:
     - “Copy Trimmed” instead of “Paste Trimmed”, or
     - show a notification: “Clipboard updated; press Ctrl+V”.

Deliverable: a documented, tested approach (even if best-effort).

---

### Phase 6 — Hotkeys (global shortcuts)
Goal: parity with Trimmy/Trimmeh GNOME: global shortcuts exist, are user-configurable, and trigger paste actions.

- Implement hotkeys in the tray app via **KF6::GlobalAccel**.
- Ensure hotkeys can be disabled/enabled individually in settings (parity with Trimmy).

Deliverable: working shortcuts.

---

### Phase 7 — QA, performance, and release packaging
1. Add a KDE-specific manual checklist (below).
2. Ensure no excessive logging and no crashes in `plasmashell`.
3. Package for KDE Store:
   - Provide `.plasmoid` zip.
   - Ensure metadata is correct and dependencies are documented.
4. Optional: distro packaging (RPM/PKGBUILD) after KDE Store.

Deliverable: release candidate package + tested notes.

---

## QA checklist (Plasma 6.4+ Wayland)

Auto-trim:
- Auto-trim toggled on/off does what it says.
- No infinite loops when Trimmeh writes to clipboard.
- Burst changes don’t regress to older clipboard contents.
- Max-lines safety works (skips big blobs).
- Skips non-plain-text (HTML/image/binary).

Heuristics:
- Prompts stripped when enabled; not stripped when disabled.
- Box gutter stripping works (│┃ etc).
- Backslash merges and wrapped URL repairs match GNOME behavior.
- “List-like” content is skipped unless High/manual (same as GNOME/Trimmy).

Manual actions:
- Restore previous copy restores the true original.
- Paste Trimmed/Original uses High aggressiveness and restores clipboard afterward.
- If paste injection fails, user still ends with sane clipboard contents.

Shortcuts:
- Hotkeys work when set.
- Hotkeys are unset by default if required by store policy.

Stability:
- Repeated enable/disable (add/remove widget) doesn’t leak or crash.

---

## Open questions for you (discussion starters)

The earlier questions were about choosing between “plasmoid-only” vs “always-on app/service”. With parity clarified, the remaining questions are now narrower:

1. Do we want to ship an **optional plasmoid UI** in addition to the tray icon, or is the tray menu + settings window sufficient for “on par with Trimmy”?
2. Are we willing to ship an **optional KWin script** (installed alongside the app) if that’s what it takes for best-effort paste injection on Wayland?

---

## Sources worth reading (primary / authoritative)

- Plasma 6 widget porting notes: https://develop.kde.org/docs/plasma/widget/porting_kf6/
- Plasma QML API overview: https://develop.kde.org/docs/plasma/widget/plasma-qml-api/
- Plasma widget C++ API note (KDE Store constraints): https://develop.kde.org/docs/plasma/widget/c-api/
- Qt clipboard API (C++): https://doc.qt.io/qt-6/qclipboard.html
- KF6 global shortcuts API: https://api.kde.org/kglobalaccel-index.html
- KWin scripting API (shortcuts + DBus): https://develop.kde.org/docs/plasma/kwin/api/
- Wayland data-control protocol background (why clipboard managers are special):
  - https://wayland.app/protocols/wlr-data-control-unstable-v1
  - https://wayland.app/protocols/ext-data-control-v1
