import Gio from 'gi://Gio';

import {DEFAULT_TRIM_OPTIONS, trim} from './dist/trimCore.js';

function readTextFile(path) {
    const file = Gio.File.new_for_path(path);
    const [, contents] = file.load_contents(null);
    const bytes = contents instanceof Uint8Array ? contents : Uint8Array.from(contents);
    return new TextDecoder('utf-8').decode(bytes);
}

function assertEq(actual, expected, msg) {
    if (actual !== expected) {
        throw new Error(`${msg ?? 'assertEq failed'}: expected '${expected}', got '${actual}'`);
    }
}

function run() {
    const json = readTextFile('tests/trim-vectors.json');
    const vectors = JSON.parse(json);

    for (const v of vectors) {
        const opts = Object.assign({}, DEFAULT_TRIM_OPTIONS, v.options || {});
        const res = trim(v.input, v.aggressiveness, opts);

        assertEq(res.output, v.expected.output, `${v.name}: output`);
        assertEq(Boolean(res.changed), Boolean(v.expected.changed), `${v.name}: changed`);

        if (Object.prototype.hasOwnProperty.call(v.expected, 'reason')) {
            assertEq(res.reason || null, v.expected.reason || null, `${v.name}: reason`);
        }

        print(`ok - ${v.name}`);
    }

    print('all trim core tests passed');
}

try {
    run();
} catch (e) {
    logError(e);
    imports.system.exit(1);
}
