// Minimal clipboard watcher; real behavior completed after wasm pipeline is wired.
import St from 'gi://St';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import type {TrimOptions, Trimmer} from './wasm.js';

export class ClipboardWatcher {
    private clipboard = St.Clipboard.get_default();
    private signals: number[] = [];
    private lastOriginal = new Map<number, string>();
    // Records the last write we performed per selection so we can skip
    // retrimming a manual restore while still trimming fresh copies.
    private lastWrite = new Map<number, {kind: 'restore' | 'trim', text: string}>();
    // Guards to ensure a restored payload is not immediately re-trimmed even if
    // GNOME emits multiple owner-change events or normalizes text.
    private restoreGuards = new Map<number, {hash: string, expires: number}>();
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
        const text = await this.readText(selection);
        if (!text) {
            return;
        }

        // If a restore just happened, skip until the clipboard changes away from
        // the restored payload (or the guard expires).
        const now = GLib.get_monotonic_time();
        const guard = this.restoreGuards.get(selection);
        if (guard) {
            if (now > guard.expires) {
                this.restoreGuards.delete(selection);
            } else {
                const hash = hashText(text);
                if (hash === guard.hash) {
                    log(`Trimmeh: skip owner-change after restore (selection=${selection})`);
                    return;
                }
                // Text changed away from restored payload; drop guard and proceed.
                this.restoreGuards.delete(selection);
            }
        }

        // Legacy single-shot skip; kept as belt-and-suspenders.
        const last = this.lastWrite.get(selection);
        if (last?.kind === 'restore') {
            this.lastWrite.delete(selection);
            return;
        }

        if (!this.settings.get_boolean('enable-auto-trim')) {
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
        this.lastWrite.set(selection, {kind: 'trim', text: result.output});
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
            const hash = hashText(original);
            const expires = GLib.get_monotonic_time() + 1_500_000; // ~1.5s
            this.restoreGuards.set(selection, {hash, expires});
            log(`Trimmeh: restore requested (selection=${selection}) hash=${hash}`);
            this.clipboard.set_text(selection, original);
            this.lastWrite.set(selection, {kind: 'restore', text: original});
        }
    }
}

function hashText(text: string): string {
    // SHA256 provides stable hashing without extra deps.
    return GLib.compute_checksum_for_string(GLib.ChecksumType.SHA256, text, -1);
}
