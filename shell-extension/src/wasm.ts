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

// Placeholder adapter; will load wasm-bindgen output once the build exists.
export async function createWasmTrimAdapter(_basePath: string): Promise<Trimmer> {
    return {
        trim: (text: string): TrimResponse => ({
            output: text,
            changed: false,
            reason: undefined,
            hash_hex: '0',
        }),
    };
}
