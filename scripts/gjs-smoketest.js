#!/usr/bin/env gjs -m
// Minimal harness to sanity-check that wasm artifacts exist and are readable by gjs.
import GLib from 'gi://GLib';
import Gio from 'gi://Gio';
import System from 'system';

const base = GLib.get_current_dir();
const wasmPath = GLib.build_filenamev([base, 'shell-extension', 'wasm', 'trimmeh_core_bg.wasm']);
const gluePath = GLib.build_filenamev([base, 'shell-extension', 'wasm', 'trimmeh_core.js']);

polyfillFetch();

async function main() {
    const ok = [wasmPath, gluePath].every(checkFile);
    if (!ok) System.exit(1);

    const glueUri = GLib.filename_to_uri(gluePath, null);
    const wasmBytes = readFileBytes(wasmPath);
    const mod = await import(glueUri);
    const initSync = mod.initSync ?? mod.default?.initSync;
    if (typeof initSync !== 'function') {
        print('glue missing initSync()');
        System.exit(1);
    }
    initSync({ module: wasmBytes });
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

function readFileBytes(path) {
    const file = Gio.File.new_for_path(path);
    const [, contents] = file.load_contents(null);
    return contents instanceof Uint8Array ? contents : Uint8Array.from(contents);
}

function polyfillFetch() {
    if (typeof globalThis.fetch === 'function') return;
    globalThis.fetch = async (uri) => {
        const file = Gio.File.new_for_uri(uri);
        const [, contents] = file.load_contents(null);
        const buf = contents instanceof Uint8Array ? contents : Uint8Array.from(contents);
        return {
            ok: true,
            status: 200,
            arrayBuffer: async () => buf.buffer.slice(0),
        };
    };
}

main().catch(e => {
    print(`smoketest failed: ${e}`);
    System.exit(1);
});
