# Trimmeh KDE QA Checklist

Target: Plasma 6.5.4 (Wayland). Baseline: Plasma >= 6.4.

## Test helpers

Use the Klipper probe to set clipboard content (Wayland-safe):

```sh
./build-kde/trimmeh-kde-probe --set-stdin <<'EOF'
your text here
EOF
```

For single-line text:

```sh
./build-kde/trimmeh-kde-probe --set "echo hello"
```

## Manual checklist (quick regression + snippets)

Clipboard auto-trim
- Copy a multi-line shell snippet with pipes/backslashes; verify it auto-trims once.

```sh
./build-kde/trimmeh-kde-probe --set-stdin <<'EOF'
kubectl get pods \
  -n kube-system \
  | jq '.items[].metadata.name'
EOF
```
sti
- Copy a single-line command; verify no change and no loop.

```sh
./build-kde/trimmeh-kde-probe --set "echo hi"
```

- Copy > max-lines (11+ lines); verify no auto-trim.

```sh
seq 1 11 | sed 's/^/line /' | ./build-kde/trimmeh-kde-probe --set-stdin
```

- Toggle Auto-trim off; verify clipboard remains unchanged on new copies.

Prompt/box handling
- Copy a prompt-prefixed snippet ("$ sudo dnf install foo") and confirm prompt stripping when enabled.

```sh
./build-kde/trimmeh-kde-probe --set-stdin <<'EOF'
$ sudo dnf install foo
EOF
```

- Copy a box-gutter snippet (leading box drawing chars like "│" or "┃") and confirm removal when enabled.

```sh
./build-kde/trimmeh-kde-probe --set-stdin <<'EOF'
│ sudo dnf upgrade && \
│ reboot
EOF
```

Manual paste
- "Paste Trimmed" uses High aggressiveness, swaps clipboard, and restores.
- "Paste Original" uses cached original if last copy was auto-trimmed.
- "Restore last copy" restores last original and does not retrim.

Portal permission flow
- Deny portal prompt once; verify menu/prefs show permission message and paste hint.
- With permission denied/unavailable, "Paste Trimmed" should still swap clipboard and show "Paste now" hint.
- Pre-authorize (flatpak permission-set ... yes); restart app and ensure portal prompt is skipped.

Tray/UI
- Preview rows show trimmed + original previews (plain text).
- Visible whitespace markers (LF => "⏎", Tab => "⇥") appear in previews.
- “Removed: N chars” line appears when trimmed output is shorter.

```sh
printf 'echo\tone\nand two\n' | ./build-kde/trimmeh-kde-probe --set-stdin
```

- Shortcut hints appear in the menu when hotkeys are enabled.
- About/Update rows present (Update row hidden unless wired).

Settings persistence
- Change toggles + delays + hotkeys; restart app and confirm settings persist.
- Start at login toggles autostart desktop entry.

## Integration test plan (minimal)

1) Build
- cmake -S trimmeh-kde -B build-kde -DCMAKE_BUILD_TYPE=Debug
- cmake --build build-kde

2) Klipper D-Bus probe
- ./build-kde/trimmeh-kde-probe --once
- ./build-kde/trimmeh-kde-probe (confirm clipboardHistoryUpdated fires on new copies)

3) Core vectors (parity)
- Use tests/trim-vectors.json as the source of truth.
- Run the KDE vector harness:

```sh
./build-kde/trimmeh-kde-vectors
```

- Optional paths:

```sh
./build-kde/trimmeh-kde-vectors --vectors ../tests/trim-vectors.json --core ./build-kde/trimmeh-core.js
```

4) Clipboard watcher scenarios
- Burst changes: copy text A then B quickly; ensure only B is written back.

```sh
./build-kde/trimmeh-kde-probe --set "echo one"
./build-kde/trimmeh-kde-probe --set "echo two"
```

- Self-write guard: after auto-trim, confirm no repeat loop.
- Disable flow: exit app while debounce timer is pending; confirm no crash.

## Notes
- If portal paste injection fails, the expected behavior is a passive hint (manual paste) plus clipboard restore.
- The extra clipboard fallback toggle is best-effort; verify behavior with HTML-only clipboard content.
