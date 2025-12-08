// Minimal clipboard watcher; real behavior completed after wasm pipeline is wired.
import St from 'gi://St';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import type {TrimOptions, Trimmer} from './wasm.js';

export class ClipboardWatcher {
    private clipboard = St.Clipboard.get_default();
    private signals: number[] = [];
    private lastOriginal = new Map<number, string>();
    // Marks a selection whose next owner-change was triggered by a manual restore,
    // so we don't immediately retrim the original payload we just put back.
    private restoring = new Set<number>();
    private pollId: number | null = null;

    constructor(private trimmer: Trimmer, private settings: Gio.Settings) {}

    enable(): void {
        try {
            const handler = (_clip: unknown, selection: number) => {
                this.onOwnerChange(selection).catch(logError);
            };
            this.signals.push(this.clipboard.connect('owner-change', handler));
        } catch (e) {
            // St.Clipboard doesn't document owner-change; fall back to polling.
            log(`owner-change not available, falling back to polling: ${e}`);
            this.startPolling();
        }

        // Prime both selections once.
        [St.ClipboardType.CLIPBOARD, St.ClipboardType.PRIMARY].forEach(sel => {
            this.onOwnerChange(sel).catch(logError);
        });
    }

    disable(): void {
        this.signals.forEach(sig => this.clipboard.disconnect(sig));
        this.signals = [];
        if (this.pollId !== null) {
            GLib.source_remove(this.pollId);
            this.pollId = null;
        }
    }

    private async onOwnerChange(selection: number): Promise<void> {
        // If this owner-change was caused by a manual restore, skip trimming once.
        if (this.restoring.has(selection)) {
            this.restoring.delete(selection);
            return;
        }
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

        this.lastOriginal.set(selection, text);
        this.clipboard.set_text(selection, result.output);
    }

    private startPolling(): void {
        this.pollId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 800, () => {
            [St.ClipboardType.CLIPBOARD, St.ClipboardType.PRIMARY].forEach(sel => {
                this.onOwnerChange(sel).catch(logError);
            });
            return GLib.SOURCE_CONTINUE;
        });
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

    restore(selection: number): void {
        const original = this.lastOriginal.get(selection);
        if (original) {
            // Tag this selection so the next owner-change (triggered by set_text)
            // doesn't immediately retrim the just-restored original.
            this.restoring.add(selection);
            this.clipboard.set_text(selection, original);
        }
    }
}
