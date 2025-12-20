# Trimmeh KDE (Plasma 6) — Testing Guide

This guide focuses on **what the current KDE tray app actually implements today**, and gives you **copy/paste test payloads** with expected outcomes.

## Preconditions

1. You are running **Plasma 6** (Wayland or X11).
2. Trimmeh KDE is built and running (tray scissors icon visible).
3. You have a place to paste results, e.g. **Kate**, **KWrite**, or a terminal.

### Build + run

From repo root:

```sh
just kde-run
```

If you ever hit a build error like `ccache: Permission denied`, run:

```sh
CCACHE_DISABLE=1 just kde-run
```

## Fast sanity: core trimming parity (automated)

This does **not** test clipboard integration, but it validates the embedded JS trimming core against `tests/trim-vectors.json`.

```sh
just kde-check-vectors
```

Expected:
- Exit code `0`
- No output

If it fails, it prints a mismatch like `[vector_name] mismatch output: expected ..., got ...`.

## Manual tests (clipboard + tray)

### 0) UI smoke test

1. Click the tray icon.
2. Confirm menu shows:
   - A disabled `Last: ...` line
   - Toggles: Auto-trim, Keep blank lines, Strip prompts, Strip box gutters
   - Aggressiveness submenu: Low / Normal / High
   - Actions: Restore last copy, Paste Trimmed (High), Paste Original, Quit

Expected:
- The menu opens instantly and doesn’t spawn a visible window.
- `Last: ...` updates after successful trims.

### 1) Baseline trim (flatten a multi-line shell command)

Ensure:
- `Auto-trim` **ON**
- `Aggressiveness` = **Normal**
- `Strip prompts` **ON**
- `Strip box gutters` **ON**

Copy this (exactly) into your clipboard:

```text
sudo dnf upgrade &&
reboot
```

Then paste into your editor.

Expected paste:

```text
sudo dnf upgrade && reboot
```

### 2) Prompt stripping (single line)

Ensure `Strip prompts` is **ON**.

Copy:

```text
$ sudo dnf install foo
```

Expected paste:

```text
sudo dnf install foo
```

Now disable `Strip prompts`, copy the same text again, and paste.

Expected paste (unchanged):

```text
$ sudo dnf install foo
```

### 3) Prompt stripping (majority rule)

Prompt stripping only activates when **a majority of non-empty lines** look like prompt commands.

Ensure `Strip prompts` is **ON**.

Copy:

```text
$ echo one
$ echo two
echo three
```

Expected paste (prompts removed on the prompt lines):

```text
echo one
echo two
echo three
```

Now copy:

```text
$ echo one
echo two
echo three
```

Expected paste: **no prompt stripping at all** (because not a majority):

```text
$ echo one
echo two
echo three
```

### 4) Box gutter stripping (single line)

Ensure `Strip box gutters` is **ON**.

Copy:

```text
│ sudo dnf upgrade
```

Expected paste:

```text
sudo dnf upgrade
```

Disable `Strip box gutters`, copy the same input again, and paste.

Expected paste (unchanged):

```text
│ sudo dnf upgrade
```

### 5) Backslash line continuation merge (multi-line command)

Ensure `Auto-trim` is **ON**.

Copy:

```text
python - <<'PY'
print("ok")\

PY
```

Expected paste (single line, backslash merge):

```text
python - <<'PY' print("ok") PY
```

### 6) Wrapped URL repair (whitespace/newline removal)

Copy:

```text
https://example.com/a/
b?c=d&e=f
```

Expected paste:

```text
https://example.com/a/b?c=d&e=f
```

### 7) Keep blank lines (High aggressiveness)

Set:
- `Aggressiveness` = **High**
- `Keep blank lines` = **ON**

Copy:

```text
echo first
echo second

echo third
```

Expected paste (still flattened, but preserves blank lines):

```text
echo first echo second

echo third
```

Now set `Keep blank lines` = **OFF**, copy the same input again.

Expected paste (blank line removed):

```text
echo first echo second echo third
```

### 8) “Max lines” safety (skips big clipboard payloads)

By default Trimmeh skips trimming if the clipboard has **more than 10 lines**.

With defaults (no config edits), copy this 11-line payload:

```text
1
2
3
4
5
6
7
8
9
10
11
```

Expected:
- Clipboard stays exactly as copied (no auto-trim)
- The tray menu’s `Last: ...` does **not** update because no trim happens

## Manual actions: what to expect today

### Restore last copy

This is an “undo” for the **last time auto-trim changed the clipboard**.

1. With `Auto-trim` ON, copy a payload from tests above that definitely gets trimmed.
2. Confirm the pasted result is trimmed.
3. In tray menu, click `Restore last copy`.
4. Paste again.

Expected:
- You get the **original** text back.

### Paste Trimmed (High) / Paste Original

Important: these actions **do not inject a paste keystroke yet**.

What they do right now:
- Temporarily replace the clipboard contents for ~**400ms**
- Then restore the previous clipboard

How to test anyway:
1. Put the cursor in a text field (Kate, terminal, browser input, etc.)
2. Open tray menu, click `Paste Trimmed (High)`
3. Immediately press `Ctrl+V` (you have a short time window)

Expected:
- If you paste fast enough, you get the trimmed version.
- If you miss the window, you paste the original (because clipboard was restored).

## What you cannot (meaningfully) test yet (and why)

1. **Real “paste injection” on Wayland**
   - Not implemented yet: the KDE port does not call xdg-desktop-portal RemoteDesktop / InputCapture (or other methods) to type/paste into the focused app.
   - Today’s “Paste …” actions are just a temporary clipboard swap, which is inherently timing-dependent.

2. **Global hotkeys / shortcuts**
   - Not implemented yet (planned via `KGlobalAccel` per `trimmeh-kde/README.md`).

3. **Config UI beyond the tray menu**
   - No KCM / settings dialog yet; some settings exist only in config storage (e.g. `max_lines`) but are not exposed in UI.

4. **Non-text clipboard formats**
   - Current watcher reads/writes `QClipboard::text()` only; rich text, images, etc. are not handled.

5. **Primary selection / middle-click clipboard**
   - Only `QClipboard::Clipboard` mode is handled (not `Selection`).

## Optional: inspect/edit settings via KDE tools

Settings are stored via KDE’s `KConfig` under file name `trimmeh-kde`, group `Trimmeh`.

If you have these tools available:

```sh
kreadconfig6 --file trimmeh-kde --group Trimmeh --key aggressiveness
kwriteconfig6 --file trimmeh-kde --group Trimmeh --key max_lines 20
```

(Restart Trimmeh after changing config keys externally to be safe.)

