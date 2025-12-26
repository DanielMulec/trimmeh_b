# Trimmeh KDE (Klipper DBus probe)

This is a minimal Phase 0 probe to validate Klipper DBus interaction on Plasma 6.5.4.

## Build

```sh
cmake -S trimmeh-kde -B build-kde -DCMAKE_BUILD_TYPE=Debug
cmake --build build-kde
```

## Run

```sh
./build-kde/trimmeh-kde-probe
```

Flags:
- `--once` prints the current clipboard once and exits.
- `--no-initial` skips the initial clipboard print and only logs signals.
