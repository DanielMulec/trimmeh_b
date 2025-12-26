# Trimmeh KDE (Klipper DBus)

This directory contains:
- `trimmeh-kde`: the KDE auto-trim app (Phase 1+).
- `trimmeh-kde-probe`: a minimal diagnostic probe for Klipper DBus.

## Build

Requires Node + `npx` (used to bundle `trimmeh-core-js`).

```sh
cmake -S trimmeh-kde -B build-kde -DCMAKE_BUILD_TYPE=Debug
cmake --build build-kde
```

## Run (app)

```sh
./build-kde/trimmeh-kde
```

## Run (probe)

```sh
./build-kde/trimmeh-kde-probe
```

Flags:
- `--once` prints the current clipboard once and exits.
- `--no-initial` skips the initial clipboard print and only logs signals.
- `--set <text>` sets the clipboard via Klipper and exits.
- `--set-stdin` reads stdin and sets the clipboard via Klipper, then exits.
