# KDE / Plasma (WIP) — First Version Testing Manual

This document describes what you can test in the current **first KDE port** implementation (tray app), and how to test it.

Scope:
- KDE app lives in `trimmeh-kde/`
- Trimming logic is reused from `trimmeh-core-js/`
- Golden vectors are in `tests/trim-vectors.json`

## Quick start

### Prerequisites (Fedora 43 KDE)

If you haven’t set up a C++/Qt/KF6 build environment on this machine yet, install:

```sh
sudo dnf install \
  just \
  cmake \
  gcc-c++ \
  ninja-build \
  extra-cmake-modules \
  qt6-qtbase-devel \
  qt6-qtdeclarative-devel \
  kf6-kstatusnotifieritem-devel \
  kf6-kconfig-devel
```

The KDE build also bundles `trimmeh-core-js` via `npx`, so you need Node + `npx`:

- If you have unversioned Node packages:
  - `sudo dnf install nodejs npm`
- If Fedora offers versioned streams on your system:
  - install the stream + its npm/npx package (example naming): `nodejs24` + `nodejs24-npm`

Sanity check:
```sh
command -v just cmake npx
```

### Troubleshooting build issues

#### CMake can’t find Qt6

If you see an error mentioning `Qt6Config.cmake` / `qt6-config.cmake`, install Qt6 dev packages:

```sh
sudo dnf install qt6-qtbase-devel qt6-qtdeclarative-devel
```

#### CMake can’t find “KF6”

Fedora packages KDE Frameworks 6 as **per-framework** CMake packages (e.g. `KF6Config`, `KF6StatusNotifierItem`), not necessarily a single `KF6Config.cmake`.

Install the dev packages:

```sh
sudo dnf install extra-cmake-modules kf6-kstatusnotifieritem-devel kf6-kconfig-devel
```

If you still see an error mentioning `KF6Config.cmake` / `kf6-config.cmake`, wipe the build directory (stale CMake cache) and retry:

```sh
rm -rf build-kde
just kde-run
```

### 1) Run the vector parity check (recommended first)

This runs `tests/trim-vectors.json` through the embedded JS engine used by the KDE app.

```sh
just kde-check-vectors
```

Expected result:
- Exit code `0`
- No output

If a vector fails, you should see a message like:
`[box_gutter_strip] mismatch output: expected ..., got ...`

### 2) Run the tray app

```sh
just kde-run
```

Expected result:
- A tray icon appears (Status Notifier Item)
- Clicking it opens a menu with:
  - `Last: …`
  - `Auto-trim` toggle
  - `Aggressiveness` submenu (Low/Normal/High)
  - `Keep blank lines`, `Strip prompts`, `Strip box gutters`
  - `Paste Trimmed (High)`, `Paste Original`, `Restore last copy`
  - `Quit`

If you don’t see the tray icon:
- Ensure Plasma’s “System Tray” widget is present in your panel.
- Ensure it shows “Status Notifier Items”.

## Test checklist (manual behavioral tests)

You can do these tests with any app you can copy/paste in (Kate, KWrite, Konsole, etc).

If you have `wl-clipboard` installed, using `wl-copy`/`wl-paste` makes clipboard testing more deterministic.

### A) Auto-trim rewrites the clipboard

Setup:
- `Auto-trim`: ON
- `Aggressiveness`: Normal

Copy this exact text (including newlines):

```
echo one
echo two
```

Expected:
- Within ~80ms, clipboard becomes:
  `echo one echo two`

Tip (Wayland):
```sh
printf 'echo one\necho two\n' | wl-copy
sleep 0.2
wl-paste
```

### B) Aggressiveness affects whether we trim

1) Set `Aggressiveness` to **Low**
2) Copy:
   ```
   echo one
   echo two
   ```
Expected:
- Low: **no change** (stays two lines)

3) Set `Aggressiveness` to **Normal**
4) Copy the same input again

Expected:
- Normal: **flattens** to `echo one echo two`

### C) Prompt stripping toggle

With `Strip prompts` ON, copy:
```
$ sudo dnf install foo
```
Expected:
- Becomes: `sudo dnf install foo`

With `Strip prompts` OFF, repeat.
Expected:
- The `$` should remain.

### D) Box gutter stripping toggle

With `Strip box gutters` ON, copy:
```
│ sudo dnf upgrade &&
│ reboot
```
Expected:
- Becomes: `sudo dnf upgrade && reboot`

With `Strip box gutters` OFF, repeat.
Expected:
- The leading `│` characters should remain.

### E) Safety valve: big blobs aren’t touched

Default `max_lines` is 10 in the shared JS core.

Copy something with **more than 10 lines**.

Expected:
- Auto-trim should not change it.

Tip:
```sh
seq 1 20 | sed 's/^/line/' | wl-copy
sleep 0.2
wl-paste
```

### F) Restore last copy works and doesn’t immediately re-trim

This checks loop/race guards.

1) Trigger an auto-trim (e.g. test A).
2) Click tray menu → `Restore last copy`

Expected:
- Clipboard returns to the original multi-line text.
- It should **stay** multi-line (i.e. not immediately re-flatten itself).

Note:
- `Restore last copy` is only enabled after at least one trim occurred that produced a cached “last original”.

### G) Paste actions (current limitation)

In this first KDE version, “Paste Trimmed/Original” does **not** inject paste key events into the focused app yet.

Current behavior:
- It temporarily swaps the clipboard for ~400ms and then restores it.

How to test anyway:
- You can watch Klipper history to see the clipboard briefly change and revert.
- Or click `Paste Trimmed (High)` and immediately press `Ctrl+V` manually to paste during the swap window.

Planned:
- Implement reliable Wayland paste injection via `xdg-desktop-portal` RemoteDesktop.

## Persistence test (settings survive restarts)

1) Toggle a few settings (Auto-trim OFF, Aggressiveness Low, etc).
2) Click `Quit`.
3) Re-run `just kde-run`.

Expected:
- The same settings states are restored.
