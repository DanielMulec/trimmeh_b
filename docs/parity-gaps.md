# Trimmeh ↔ Trimmy Parity Gaps (as of 2025‑12‑11)

This document compares upstream **Trimmy** (macOS app in `upstream/Trimmy/`) with **Trimmeh** (GNOME/Wayland port in this repo).  
Goal: feature parity for “go‑live”, or explicit, well‑understood deviations.

Read this first if you are a future agent:
- Project goals/roles: `agents.md`, `README.md`.
- Core API/behavior contract: `docs/core-api.md`.
- Extension scaffolding/expected layout: `docs/extension-scaffold.md`.

## TL;DR
- **Core trimming heuristics:** parity achieved (Rust mirrors Swift almost line‑for‑line).
- **GNOME shell extension:** missing several UI/manual‑paste features from Trimmy.
- **CLI:** functional, but not 1:1 with TrimmyCLI flags/I/O/JSON schema.

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

### Status: ⚠️ Partial parity (auto‑trim works; manual paste UX missing)

#### 2.1 Implemented parity features
| Feature | Upstream reference | Trimmeh status |
|---|---|---|
| Auto‑trim clipboard when command‑like | `ClipboardMonitor.tick/trimClipboardIfNeeded` (`ClipboardMonitor.swift#L93-L107`) | Implemented via `ClipboardWatcher.onOwnerChange` in `shell-extension/src/clipboard.ts#L48-L94` |
| Aggressiveness levels Low/Normal/High | `Aggressiveness.swift` + settings panes | Implemented (`aggressiveness` GSettings) |
| Keep blank lines toggle | `AppSettings.preserveBlankLines` | Implemented (`keep-blank-lines`) |
| Remove box‑drawing chars toggle | `AppSettings.removeBoxDrawing` | Implemented (`strip-box-chars`) |
| Safety valve: skip > max lines | `TextCleaner.transformIfCommand` + watcher guard | Implemented in core (`max-lines`), wired through settings |
| Restore previous copy | `ClipboardMonitor.pasteOriginal` uses cached original (`ClipboardMonitor.swift#L216-L230`) | Implemented as “Restore last copy” panel item (`shell-extension/src/panel.ts#L33-L36`) + watcher cache (`shell-extension/src/clipboard.ts#L10-L132`) |

#### 2.2 Missing / divergent extension features (need for parity)

1. **“Paste Trimmed” (one‑shot trim‑and‑paste, restore clipboard)**
   - Upstream behavior: trims using High aggressiveness, injects paste into frontmost app, then restores clipboard.  
     Upstream refs: `ClipboardMonitor.pasteTrimmed` (`ClipboardMonitor.swift#L200-L214`), called by hotkeys/menu (`HotkeyManager.swift#L56-L102`, `MenuContentView.swift#L21-L45`).
   - Trimmeh status: **implemented** as a panel‑menu action using a GNOME virtual keyboard (`shell-extension/src/virtualPaste.ts`) and safe temporary clipboard swap (`shell-extension/src/clipboardWatcher.ts#pasteTrimmed`).

2. **“Paste Original” (one‑shot paste of untrimmed text)**
   - Upstream refs: `ClipboardMonitor.pasteOriginal` (`ClipboardMonitor.swift#L216-L231`), hotkeys/menu as above.
   - Trimmeh status: **implemented** as a panel‑menu action (`shell-extension/src/panel.ts#L33-L49`), restoring prior clipboard after paste (`shell-extension/src/clipboardWatcher.ts#pasteOriginal`).

3. **Global hotkeys for manual actions**
   - Upstream refs: `HotkeyManager` (`HotkeyManager.swift#L6-L111`) registers:
     - Paste Trimmed hotkey (default ⌥⌘T),
     - Paste Original hotkey (default ⌥⌘⇧T),
     - Toggle Auto‑Trim hotkey.
   - Trimmeh status: **not implemented**. GNOME side will need Shell keybindings + prefs UI.

4. **Menu/panel previews of last action**
   - Upstream refs: menu preview/strike‑through and stats in `MenuContentView.swift` (preview helpers around `#L51-L175`) and `ClipboardMonitor.struckOriginalPreview` (`ClipboardMonitor.swift#L233-L248`).
   - Trimmeh status: **not implemented**. Panel menu only shows toggles/actions.

5. **Auto‑trim visual state (icon dimming/feedback)**
   - Upstream refs: menu icon dims when auto‑trim off (see changelog 0.3.0; implementation in SwiftUI menu views).
   - Trimmeh status: **not implemented**. Could be as simple as toggling a style class or opacity in `shell-extension/src/panel.ts`.

6. **Grace‑delay / promised‑data handling**
   - Upstream refs: `ClipboardMonitor.tick` waits ~80ms before read/trim (`ClipboardMonitor.swift#L102-L105`).
   - Trimmeh status: **not implemented**. Might not be required on GNOME, but if clipboard data arrives late, add a short `GLib.timeout_add` deferral before `readText`.

7. **Optional rich‑text clipboard fallbacks**
   - Upstream refs: “extra clipboard fallbacks” toggle (0.3.0) + `readTextFromPasteboard` in `ClipboardMonitor.swift` (not shown in snippet due to truncation).
   - Trimmeh status: **not implemented**. St.Clipboard `get_text` usually yields plain UTF‑8; if parity issues show up with some apps, consider adding a portal/GTK clipboard fallback plus a GSettings toggle.

#### 2.3 Linux‑specific notes for paste features
Trimmeh uses GNOME Shell’s compositor‑side virtual keyboard to inject Ctrl+V, so no portal permission is required on GNOME 49 Wayland. If this ever regresses upstream, fall back to RemoteDesktop portal injection or a copy‑only UI.

Keep parity semantics:
- Manual actions always use **High aggressiveness** (upstream rule).
- Clipboard must be restored after on‑demand paste.
- Show user‑visible feedback if permission/portal is missing (analogous to Accessibility callout in upstream: `AccessibilityPermissionCallout.swift#L5-L48`).

---

## 3. CLI parity

### Status: ⚠️ Mostly parity, with a few gaps

| Feature | Upstream reference | Trimmeh status |
|---|---|---|
| High‑force flag `--force/-f` | `TrimmyCLI/main.swift#L40-L42` | Implemented (`trimmeh-cli/src/main.rs#L31-L33`) |
| Aggressiveness flag | `TrimmyCLI/main.swift#L44-L47` | Implemented (`trimmeh-cli/src/main.rs#L27-L29`) |
| Preserve blank lines | `TrimmyCLI/main.swift#L48-L51` | Implemented (`--preserve-blank-lines`) |
| Keep/remove box‑drawing | `TrimmyCLI/main.swift#L52-L55` | **Partially parity**: only `--keep-box-drawing` exists; no `--remove-box-drawing` alias (default already strips). |
| JSON output schema | `TrimmyCLI/main.swift#L70-L75` | **Differs**: Trimmeh emits `{original, trimmed, changed}` not `{original, trimmed, transformed}`. |
| File input via `--trim <file>` | `TrimmyCLI/main.swift#L36-L39` + `#L92-L109` | **Missing**: Trimmeh reads stdin only. |
| Exit codes (0/1/2/3) | `TrimmyCLI/main.swift#L63-L90` | **Slightly different**: exit‑2 on no change matches; JSON encode error not mapped to exit‑3 explicitly. |

Recommended parity tasks:
1. Add a file/`-` input option to `trimmeh-cli trim` (match `--trim <file>`).
2. Accept `--remove-box-drawing` as an alias (even if redundant).
3. Consider matching JSON key name (`transformed`) and JSON error exit code (3).

---

## 4. Non‑goals / expected permanent differences
These are upstream macOS‑specific and not required for Linux parity:
- Sparkle auto‑updates + appcast (`appcast.xml`, update panes).
- Launch‑at‑login toggle (macOS SMAppService).
- macOS About panel / notarization / signing pipelines.
- Telemetry hooks (Trimmeh policy: no network calls).

---

## 5. Suggested next implementation order (for future GPT)
1. **Manual “Paste Trimmed / Paste Original” flows** on Wayland (portal feasibility first).
2. **Global hotkeys** wired to those flows + auto‑trim toggle.
3. **Panel previews / feedback** (last action text, icon dimming).
4. CLI parity clean‑up (file input, JSON key/exit codes).
5. Optional clipboard fallbacks + grace delay only if real‑world bugs show up.
