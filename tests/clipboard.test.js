import GLib from 'gi://GLib';
import {FakeClipboard} from './fakeClipboard.js';
import {ClipboardWatcher} from './dist/clipboardWatcher.js';

const CLIPBOARD = 0;

class FakeSettings {
    constructor() {
        this.values = new Map([
            ['enable-auto-trim', true],
            ['aggressiveness', 'normal'],
            ['keep-blank-lines', false],
            ['strip-box-chars', true],
            ['trim-prompts', true],
            ['max-lines', 10],
        ]);
    }
    get_boolean(key) { return Boolean(this.values.get(key)); }
    get_string(key) { return String(this.values.get(key)); }
    get_int(key) { return Number(this.values.get(key)); }
    set_boolean(key, val) { this.values.set(key, val); }
}

const trimmer = {
    trim: (text) => {
        const out = text.replace(/\n+/g, ' ').replace(/\s+/g, ' ').trim();
        return {
            output: out,
            changed: out !== text,
            reason: undefined,
            hash_hex: '',
        };
    },
};

function sleep(ms) {
    return new Promise(resolve => {
        GLib.timeout_add(GLib.PRIORITY_DEFAULT, ms, () => {
            resolve();
            return GLib.SOURCE_REMOVE;
        });
    });
}

function assertEq(actual, expected, msg) {
    if (actual !== expected) {
        throw new Error(`${msg ?? 'assertEq failed'}: expected '${expected}', got '${actual}'`);
    }
}

async function testBasicTrim() {
    const clip = new FakeClipboard();
    const settings = new FakeSettings();
    const watcher = new ClipboardWatcher(clip, trimmer, settings, {graceDelayMs: 20});
    watcher.enable([CLIPBOARD]);

    clip.set_user_text(CLIPBOARD, 'echo one\necho two');
    await sleep(60);
    assertEq(clip.text(CLIPBOARD), 'echo one echo two', 'basic trim');
}

async function testBurstCoalescing() {
    const clip = new FakeClipboard();
    const settings = new FakeSettings();
    const watcher = new ClipboardWatcher(clip, trimmer, settings, {graceDelayMs: 30});
    watcher.enable([CLIPBOARD]);

    clip.set_user_text(CLIPBOARD, 'echo A\necho A2');
    await sleep(10);
    clip.set_user_text(CLIPBOARD, 'echo B\necho B2');
    await sleep(80);
    assertEq(clip.text(CLIPBOARD), 'echo B echo B2', 'burst coalescing uses latest');
}

async function testSelfWriteGuard() {
    const clip = new FakeClipboard();
    const settings = new FakeSettings();
    const watcher = new ClipboardWatcher(clip, trimmer, settings, {graceDelayMs: 20});
    watcher.enable([CLIPBOARD]);

    clip.set_user_text(CLIPBOARD, 'echo one\necho two');
    await sleep(60);
    const writesAfterTrim = clip.setCalls.length;
    await sleep(60);
    assertEq(clip.setCalls.length, writesAfterTrim, 'no self-write loop');
}

async function testDisableMidFlight() {
    const clip = new FakeClipboard();
    const settings = new FakeSettings();
    const watcher = new ClipboardWatcher(clip, trimmer, settings, {graceDelayMs: 50});
    watcher.enable([CLIPBOARD]);

    clip.set_user_text(CLIPBOARD, 'echo one\necho two');
    watcher.disable();
    await sleep(100);
    assertEq(clip.text(CLIPBOARD), 'echo one\necho two', 'disable cancels pending work');
}

async function testRestoreGuard() {
    const clip = new FakeClipboard();
    const settings = new FakeSettings();
    const watcher = new ClipboardWatcher(clip, trimmer, settings, {graceDelayMs: 20});
    watcher.enable([CLIPBOARD]);

    clip.set_user_text(CLIPBOARD, 'echo one\necho two');
    await sleep(60);
    watcher.restore(CLIPBOARD);
    await sleep(80);
    assertEq(clip.text(CLIPBOARD), 'echo one\necho two', 'restore not re-trimmed');
}

async function run() {
    const tests = [
        testBasicTrim,
        testBurstCoalescing,
        testSelfWriteGuard,
        testDisableMidFlight,
        testRestoreGuard,
    ];

    for (const t of tests) {
        await t();
        print(`ok - ${t.name}`);
    }
    print('all tests passed');
}

run().catch(e => {
    logError(e);
    imports.system.exit(1);
});

