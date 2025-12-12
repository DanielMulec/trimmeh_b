# Trimmeh ↔ Trimmy Parity Gaps (as of 2025‑12‑12)

This document compares upstream **Trimmy** (macOS app in `upstream/Trimmy/`) with **Trimmeh** (GNOME/Wayland port in this repo).  
Goal: feature parity for “go‑live”, or explicit, well‑understood deviations.

Read this first if you are a future agent:
- Project goals/roles: `agents.md`, `README.md`.
- Core API/behavior contract: `docs/core-api.md`.
- Extension scaffolding/expected layout: `docs/extension-scaffold.md`.

## TL;DR
- **Core trimming heuristics:** parity achieved (Rust mirrors Swift almost line‑for‑line).
- **GNOME shell extension:** auto‑trim + manual paste + hotkeys are implemented; remaining gaps are mostly UX polish and optional clipboard/paste fallbacks.
- **CLI:** parity achieved (with small additive compat).

---

## 1. Core heuristics parity

### Status: ✅ Parity
Trimmeh’s Rust core in `trimmeh-core/src/lib.rs` matches upstream `TextCleaner` + `CommandDetector`:

| Upstream feature | Upstream reference | Trimmeh implementation |
|---|---|---|
| Normalize newlines, max‑lines guard | `ClipboardMonitor.swift#L170-L174` + `TextCleaner.transformIfCommand` | `trimmeh-core/src/lib.rs#L59-L70` |
| Box‑drawing gutter stripping (majority leading/trailing, special cases) | `TextCleaner.stripBoxDrawingCharacters` (`TextCleaner.swift#L277-L362`) | `strip_box_drawing_characters` in `trimmeh-core/src/lib.rs#L229-L309` |
| Prompt stripping (majority rule, command‑shaped only) | `TextCleaner.stripPromptPrefixes` + helpers (`TextCleaner.swift#L365-L408`) | `strip_prompt_prefixes` + helpers in `trimmeh-core/src/lib.rs#L171-L227` |
| Wrapped URL repair | `TextCleaner.repairWrappedURL` (`TextCleaner.swift#L18-L52`) | `repair_wrapped_url` in `trimmeh-core/src/lib.rs#L303-L334` |
| Command detection scoring + aggressiveness thresholds | `TextCleaner.transformIfCommand` (`TextCleaner.swift#L85-L176`) | `transform_if_command` in `trimmeh-core/src/lib.rs#L335-L417` |
| Skip list‑like snippets unless High/manual | `TextCleaner.isLikelyList` (`TextCleaner.swift#L219-L242`) | `is_likely_list` in `trimmeh-core/src/lib.rs#L469-L508` |
| Skip source‑code‑looking snippets unless strong signals | `TextCleaner.isLikelySourceCode` (`TextCleaner.swift#L197-L217`) | `is_likely_source_code` in `trimmeh-core/src/lib.rs#L510-L518` |
| Don’t flatten 3+ standalone command lines without explicit joiners | `TextCleaner.transformIfCommand` guard (added in 0.6.1) | `transform_if_command` guard in `trimmeh-core/src/lib.rs#L372-L381` |
| Hyphen/word/path joins + backslash merges | `TextCleaner.flatten` (`TextCleaner.swift#L248-L274`) | `flatten` in `trimmeh-core/src/lib.rs#L520-L570` |

### Small intentional deviation
- **Prompt stripping is toggleable** in Trimmeh (`trim-prompts`), while upstream always strips prompts. This is an additive feature, not a gap.

---

## 2. GNOME shell extension parity

### Status: ⚠️ Mostly parity (core + manual paste work; some polish missing)

#### 2.1 Implemented parity features
| Feature | Upstream reference | Trimmeh status |
|---|---|---|
| Auto‑trim clipboard when command‑like | `ClipboardMonitor.tick/trimClipboardIfNeeded` (`ClipboardMonitor.swift#L93-L107`) | Implemented via `ClipboardWatcher.onOwnerChange` in `shell-extension/src/clipboard.ts#L48-L94` |
| Clipboard change detection | `NSPasteboard.changeCount` loop + debounce | Implemented via `St.Clipboard` owner‑change when available; falls back to polling on GNOME builds where the signal is missing (see `shell-extension/src/clipboardWatcher.ts`) |
| Aggressiveness levels Low/Normal/High | `Aggressiveness.swift` + settings panes | Implemented (`aggressiveness` GSettings) |
| Keep blank lines toggle | `AppSettings.preserveBlankLines` | Implemented (`keep-blank-lines`) |
| Remove box‑drawing chars toggle | `AppSettings.removeBoxDrawing` | Implemented (`strip-box-chars`) |
| Safety valve: skip > max lines | `TextCleaner.transformIfCommand` + watcher guard | Implemented in core (`max-lines`), wired through settings |
| Restore previous copy | `ClipboardMonitor.pasteOriginal` uses cached original (`ClipboardMonitor.swift#L216-L230`) | Implemented as “Restore last copy” panel item (`shell-extension/src/panel.ts#L33-L36`) + watcher cache (`shell-extension/src/clipboard.ts#L10-L132`) |
| Paste Trimmed / Paste Original | `ClipboardMonitor.pasteTrimmed` / `.pasteOriginal` | Implemented via safe temporary clipboard swap (`shell-extension/src/clipboardWatcher.ts`) + compositor virtual keyboard paste injection (`shell-extension/src/virtualPaste.ts`) |
| Global hotkeys | `HotkeyManager` (`HotkeyManager.swift#L6-L111`) | Implemented via GNOME Shell keybindings registered in `shell-extension/src/extension.ts` (GSettings keys `paste-trimmed-hotkey`, `paste-original-hotkey`, `toggle-auto-trim-hotkey`) |

#### 2.2 Remaining gaps / known differences

1. **Menu/panel previews of last action**
   - Upstream refs: menu preview/strike‑through and stats in `MenuContentView.swift` (preview helpers around `#L51-L175`) and `ClipboardMonitor.struckOriginalPreview` (`ClipboardMonitor.swift#L233-L248`).
   - Trimmeh status: **implemented (basic)** as a non‑interactive “Last” row showing an ellipsized preview (`shell-extension/src/panel.ts`, updated from watcher `lastSummary`). Strike‑through diff is not implemented yet.

2. **Paste injection reliability (Wayland)**
   - Upstream: uses macOS accessibility APIs to paste reliably into the frontmost app.
   - Trimmeh: uses GNOME Shell’s compositor‑side virtual keyboard. This is Wayland‑native but still best‑effort: some apps may ignore synthetic paste key events. To reduce failures, Trimmeh waits briefly for hotkey modifiers (Super/Alt/Shift) to be released before injecting paste.

3. **Auto‑trim visual state (icon dimming/feedback)**
   - Upstream refs: menu icon dims when auto‑trim off (see changelog 0.3.0; implementation in SwiftUI menu views).
   - Trimmeh status: **implemented** by reducing panel icon opacity when auto‑trim is off (`shell-extension/src/panel.ts`).

4. **Grace‑delay / promised‑data handling**
   - Upstream refs: `ClipboardMonitor.tick` waits ~80ms before read/trim (`ClipboardMonitor.swift#L102-L105`).
   - Trimmeh status: **implemented** in the race‑safe watcher with a configurable grace delay (`shell-extension/src/clipboardWatcher.ts`).

5. **Optional rich‑text clipboard fallbacks**
   - Upstream refs: “extra clipboard fallbacks” toggle (0.3.0) + `readTextFromPasteboard` in `ClipboardMonitor.swift` (not shown in snippet due to truncation).
   - Trimmeh status: **not implemented**. St.Clipboard `get_text` usually yields plain UTF‑8; if parity issues show up with some apps, consider adding a portal/GTK clipboard fallback plus a GSettings toggle.

#### 2.3 Linux‑specific notes for paste features
Trimmeh uses GNOME Shell’s compositor‑side virtual keyboard to inject paste key events (prefers Shift+Insert, falls back to Ctrl+V), so no portal permission is required on GNOME 49 Wayland. Because GNOME keybindings fire while modifiers are still held, Trimmeh waits briefly for Super/Alt/Shift to be released before injecting paste (best‑effort).

Keep parity semantics:
- Manual actions always use **High aggressiveness** (upstream rule).
- Clipboard must be restored after on‑demand paste.
- Show user‑visible feedback if permission/portal is missing (analogous to Accessibility callout in upstream: `AccessibilityPermissionCallout.swift#L5-L48`).

---

## 3. CLI parity

### Status: ✅ Parity (with small additive compat)

| Feature | Upstream reference | Trimmeh status |
|---|---|---|
| High‑force flag `--force/-f` | `TrimmyCLI/main.swift#L40-L42` | Implemented (`trimmeh-cli/src/main.rs#L31-L33`) |
| Aggressiveness flag | `TrimmyCLI/main.swift#L44-L47` | Implemented (`trimmeh-cli/src/main.rs#L27-L29`) |
| Preserve blank lines | `TrimmyCLI/main.swift#L48-L51` | Implemented (`--preserve-blank-lines`) |
| Keep/remove box‑drawing | `TrimmyCLI/main.swift#L52-L55` | Parity: `--keep-box-drawing` + alias `--remove-box-drawing` (default strips). |
| JSON output schema | `TrimmyCLI/main.swift#L70-L75` | Parity: emits `transformed` like upstream; keeps `changed` as additive alias. |
| File input via `--trim <file>` | `TrimmyCLI/main.swift#L36-L39` + `#L92-L109` | Parity: `trimmeh-cli trim --trim <file>` or `--trim -` for stdin. |
| Exit codes (0/1/2/3) | `TrimmyCLI/main.swift#L63-L90` | Parity: exit‑2 on no change; exit‑3 on JSON encode error. |

---

## 4. Non‑goals / expected permanent differences
These are upstream macOS‑specific and not required for Linux parity:
- Sparkle auto‑updates + appcast (`appcast.xml`, update panes).
- Launch‑at‑login toggle (macOS SMAppService).
- macOS About panel / notarization / signing pipelines.
- Telemetry hooks (Trimmeh policy: no network calls).

---

## 5. Suggested next implementation order (for future GPT)
1. **Preferences UI for hotkey rebinding** (optional parity polish).
2. **Strike‑through diff preview** in panel menu (optional polish).
3. Optional clipboard fallbacks only if real‑world bugs show up.
