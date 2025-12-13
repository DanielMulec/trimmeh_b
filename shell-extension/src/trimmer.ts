import {trim as coreTrim} from '../../trimmeh-core-js/src/index.js';
import type {Aggressiveness, TrimOptions, TrimResult} from '../../trimmeh-core-js/src/index.js';

export type {Aggressiveness, TrimOptions} from '../../trimmeh-core-js/src/index.js';

export interface TrimResponse extends TrimResult {
    // Kept for backwards-compat with the old wasm adapter shape.
    hash_hex: string;
}

export interface Trimmer {
    trim(text: string, aggressiveness: Aggressiveness, options: TrimOptions): TrimResponse;
}

export function createTrimAdapter(): Trimmer {
    return {
        trim: (text: string, aggressiveness: Aggressiveness, options: TrimOptions): TrimResponse => {
            const res = coreTrim(text, aggressiveness, options);
            return {
                output: res.output,
                changed: res.changed,
                reason: res.reason,
                hash_hex: '',
            };
        },
    };
}

