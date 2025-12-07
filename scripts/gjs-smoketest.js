#!/usr/bin/env gjs
// Minimal harness to sanity-check that wasm artifacts exist and are readable by gjs.
import GLib from 'gi://GLib';
import Gio from 'gi://Gio';
import System from 'system';

const base = GLib.get_current_dir();
const wasmPath = GLib.build_filenamev([base, 'shell-extension', 'wasm', 'trimmeh_core_bg.wasm']);
const gluePath = GLib.build_filenamev([base, 'shell-extension', 'wasm', 'trimmeh_core.js']);

async function main() {
    const ok = [wasmPath, gluePath].every(checkFile);
    if (!ok) System.exit(1);

    const glueUri = GLib.filename_to_uri(gluePath, null);
    const wasmUri = GLib.filename_to_uri(wasmPath, null);
    const mod = await import(glueUri);
    const init = mod.default ?? mod.init;
    if (typeof init !== 'function') {
        print('glue missing init()');
        System.exit(1);
    }
    await init(wasmUri);
    const trim = mod.trim_js ?? mod.trim;
    if (typeof trim !== 'function') {
        print('glue missing trim_js()');
        System.exit(1);
    }
    const res = trim('$ echo hi', 1, {
        keep_blank_lines: false,
        strip_box_chars: true,
        trim_prompts: true,
        max_lines: 10,
    });
    print(`trimmed: ${JSON.stringify(res)}`);
}

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

main().catch(e => {
    print(`smoketest failed: ${e}`);
    System.exit(1);
});
