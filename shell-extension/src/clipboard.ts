// Minimal clipboard watcher; real behavior completed after wasm pipeline is wired.
import St from 'gi://St';
import Gio from 'gi://Gio';
import type {TrimOptions, Trimmer} from './wasm.js';

export class ClipboardWatcher {
    private clipboard = St.Clipboard.get_default();
    private signals: number[] = [];
    private lastHash = new Map<number, string>();

    constructor(private trimmer: Trimmer, private settings: Gio.Settings) {}

    enable(): void {
        const handler = (_clip: unknown, selection: number) => {
            this.onOwnerChange(selection).catch(logError);
        };
        this.signals.push(this.clipboard.connect('owner-change', handler));
        // Prime both selections once.
        [St.ClipboardType.CLIPBOARD, St.ClipboardType.PRIMARY].forEach(sel => {
            this.onOwnerChange(sel).catch(logError);
        });
    }

    disable(): void {
        this.signals.forEach(sig => this.clipboard.disconnect(sig));
        this.signals = [];
        this.lastHash.clear();
    }

    private async onOwnerChange(selection: number): Promise<void> {
        if (!this.settings.get_boolean('enable-auto-trim')) {
            return;
        }
        const text = await this.readText(selection);
        if (!text) {
            return;
        }

        const opts = this.readOptions();
        const aggr = this.settings.get_string('aggressiveness') as TrimOptions['aggressiveness'];
        const result = this.trimmer.trim(text, aggr, opts);

        if (!result.changed) {
            return;
        }
        if (this.lastHash.get(selection) === result.hash_hex) {
            return;
        }

        this.lastHash.set(selection, result.hash_hex);
        this.clipboard.set_text(selection, result.output);
    }

    private readOptions(): TrimOptions {
        return {
            keep_blank_lines: this.settings.get_boolean('keep-blank-lines'),
            strip_box_chars: this.settings.get_boolean('strip-box-chars'),
            trim_prompts: this.settings.get_boolean('trim-prompts'),
            max_lines: this.settings.get_int('max-lines'),
        };
    }

    private readText(selection: number): Promise<string | null> {
        return new Promise(resolve => {
            this.clipboard.get_text(selection, (_clip: unknown, text: string | null) => {
                resolve(text);
            });
        });
    }
}
