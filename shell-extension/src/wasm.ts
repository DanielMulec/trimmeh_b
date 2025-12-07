import GLib from 'gi://GLib';
import Gio from 'gi://Gio';

export type Aggressiveness = 'low' | 'normal' | 'high';

export interface TrimOptions {
    keep_blank_lines: boolean;
    strip_box_chars: boolean;
    trim_prompts: boolean;
    max_lines: number;
}

export interface TrimResponse {
    output: string;
    changed: boolean;
    reason?: string;
    hash_hex: string;
}

export interface Trimmer {
    trim(text: string, aggressiveness: Aggressiveness, options: TrimOptions): TrimResponse;
}

const WASM_FILE = 'shell-extension/wasm/trimmeh_core_bg.wasm';
const GLUE_FILE = 'shell-extension/wasm/trimmeh_core.js';

export async function createWasmTrimAdapter(basePath: string): Promise<Trimmer> {
    try {
        ensureFetchPolyfill();
        const gluePath = GLib.build_filenamev([basePath, 'wasm', 'trimmeh_core.js']);
        const wasmPath = GLib.build_filenamev([basePath, 'wasm', 'trimmeh_core_bg.wasm']);
        const glueUri = GLib.filename_to_uri(gluePath, null);
        const wasmUri = GLib.filename_to_uri(wasmPath, null);

        const module: any = await import(glueUri);
        const init = module.default ?? module.init;
        if (typeof init !== 'function') {
            throw new Error('wasm-bindgen glue missing init()');
        }
        await init(wasmUri);

        const trimFn = module.trim_js ?? module.trim;
        if (typeof trimFn !== 'function') {
            throw new Error('wasm-bindgen glue missing trim_js()');
        }

        return {
            trim: (text: string, aggressiveness: Aggressiveness, options: TrimOptions): TrimResponse => {
                const aggrCode = aggressivenessToCode(aggressiveness);
                const res: any = trimFn(text, aggrCode, options);
                return {
                    output: res.output ?? text,
                    changed: Boolean(res.changed),
                    reason: res.reason ?? undefined,
                    hash_hex: res.hash_hex ?? '',
                };
            },
        };
    } catch (e) {
        log(`Trimmeh wasm adapter failed, falling back to no-op: ${e}`);
        return {
            trim: (text: string): TrimResponse => ({
                output: text,
                changed: false,
                reason: undefined,
                hash_hex: '',
            }),
        };
    }
}

function ensureFetchPolyfill() {
    if (typeof (globalThis as any).fetch === 'function') {
        return;
    }
    (globalThis as any).fetch = async (uri: string) => {
        const file = Gio.File.new_for_uri(uri);
        const [, contents] = file.load_contents(null);
        const buf = contents instanceof Uint8Array ? contents : Uint8Array.from(contents as unknown as number[]);
        return {
            ok: true,
            status: 200,
            arrayBuffer: async () => buf.buffer.slice(0),
        };
    };
}

function aggressivenessToCode(level: Aggressiveness): number {
    switch (level) {
        case 'low':
            return 0;
        case 'high':
            return 2;
        case 'normal':
        default:
            return 1;
    }
}
