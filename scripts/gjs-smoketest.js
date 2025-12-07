#!/usr/bin/env gjs
// Minimal harness to sanity-check that wasm artifacts exist and are readable by gjs.
import GLib from 'gi://GLib';
import Gio from 'gi://Gio';
import System from 'system';

const base = GLib.get_current_dir();
const wasmPath = GLib.build_filenamev([base, 'shell-extension', 'wasm', 'libtrimmeh_core.wasm']);
const gluePath = GLib.build_filenamev([base, 'shell-extension', 'wasm', 'trimmeh_core.js']);

function checkFile(path) {
    if (!GLib.file_test(path, GLib.FileTest.EXISTS)) {
        print(`missing: ${path}`);
        return false;
    }
    try {
        const f = Gio.File.new_for_path(path);
        f.load_contents(null);
        return true;
    } catch (e) {
        print(`cannot read ${path}: ${e}`);
        return false;
    }
}

const ok = checkFile(wasmPath) && checkFile(gluePath);
if (!ok) {
    System.exit(1);
}

print('wasm artifacts present; integrate wasm-bindgen loader next.');
