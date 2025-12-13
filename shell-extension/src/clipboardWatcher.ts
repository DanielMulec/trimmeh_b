import GLib from 'gi://GLib';
import type {TrimOptions, Trimmer} from './trimmer.js';

export interface SettingsLike {
    get_boolean(key: string): boolean;
    get_string(key: string): string;
    get_int(key: string): number;
}

export interface ClipboardLike {
    get_text(selection: number, cb: (text: string | null) => void): void;
    set_text(selection: number, text: string): void;
    connect_owner_change(cb: (selection: number) => void): number;
    disconnect(id: number): void;
}

type WriteKind = 'trim' | 'restore' | 'manual';

interface RestoreGuard {
    hash: string;
    expiresUsec: number;
}

interface SelectionState {
    gen: number;
    pendingGen: number | null;
    debounceId: number | null;
    lastWrittenHash: string | null;
    lastWriteKind: WriteKind | null;
    restoreGuard: RestoreGuard | null;
    lastOriginal: string | null;
    lastTrimmed: string | null;
}

export interface WatcherOptions {
    graceDelayMs?: number;
    pollIntervalMs?: number;
}

const DEFAULT_GRACE_DELAY_MS = 80;
const DEFAULT_POLL_INTERVAL_MS = 800;

export class ClipboardWatcher {
    protected enabled = false;
    private signals: number[] = [];
    private pollId: number | null = null;
    private states = new Map<number, SelectionState>();
    private graceDelayMs: number;
    private pollIntervalMs: number;

    constructor(
        protected clipboard: ClipboardLike,
        protected trimmer: Trimmer,
        protected settings: SettingsLike,
        opts: WatcherOptions = {},
    ) {
        this.graceDelayMs = opts.graceDelayMs ?? DEFAULT_GRACE_DELAY_MS;
        this.pollIntervalMs = opts.pollIntervalMs ?? DEFAULT_POLL_INTERVAL_MS;
    }

    /** Latest single-line preview of the last trim/paste action. */
    public lastSummary: string = '';
    /** Optional hook for UI to observe summary changes. */
    public onSummaryChanged: ((summary: string) => void) | null = null;

    enable(selections: number[] = []): void {
        this.enabled = true;
        try {
            const id = this.clipboard.connect_owner_change((selection: number) => {
                this.onOwnerChange(selection);
            });
            this.signals.push(id);
        } catch (e) {
            log(`Trimmeh: owner-change not available, falling back to polling: ${e}`);
            this.startPolling(selections);
        }

        selections.forEach(sel => this.onOwnerChange(sel));
    }

    disable(): void {
        this.enabled = false;
        this.signals.forEach(sig => this.clipboard.disconnect(sig));
        this.signals = [];

        this.states.forEach(state => {
            if (state.debounceId !== null) {
                GLib.source_remove(state.debounceId);
                state.debounceId = null;
            }
            state.pendingGen = null;
        });

        if (this.pollId !== null) {
            GLib.source_remove(this.pollId);
            this.pollId = null;
        }
    }

    restore(selection: number): void {
        const state = this.getState(selection);
        const original = state.lastOriginal;
        if (!original) {
            return;
        }

        this.internalWrite(selection, original, 'restore', true);
    }

    /**
     * One-shot: temporarily replace clipboard with trimmed text (High aggr),
     * call pasteFn to inject paste, then restore previous clipboard.
     */
    async pasteTrimmed(
        selection: number,
        pasteFn: () => void | Promise<void>,
        restoreDelayMs = 200,
    ): Promise<void> {
        const prevText = await this.readText(selection);
        if (!prevText) {
            return;
        }

        const opts = this.readOptions();
        const result = this.trimmer.trim(prevText, 'high', opts);
        const toPaste = result.changed ? result.output : prevText;

        if (toPaste !== prevText) {
            const state = this.getState(selection);
            state.lastOriginal = prevText;
            state.lastTrimmed = toPaste;
            this.internalWrite(selection, toPaste, 'manual');
        }

        try {
            await pasteFn();
        } catch (e) {
            logError(e);
        }

        GLib.timeout_add(GLib.PRIORITY_DEFAULT, restoreDelayMs, () => {
            if (this.enabled) {
                this.internalWrite(selection, prevText, 'restore', true);
            }
            return GLib.SOURCE_REMOVE;
        });
    }

    /**
     * One-shot: temporarily replace clipboard with original (untrimmed) text,
     * call pasteFn, then restore previous clipboard.
     */
    async pasteOriginal(
        selection: number,
        pasteFn: () => void | Promise<void>,
        restoreDelayMs = 200,
    ): Promise<void> {
        const prevText = await this.readText(selection);
        if (!prevText) {
            return;
        }

        const state = this.getState(selection);
        const original = state.lastOriginal ?? prevText;

        if (original !== prevText) {
            this.internalWrite(selection, original, 'manual');
        }

        try {
            await pasteFn();
        } catch (e) {
            logError(e);
        }

        if (original === prevText) {
            return;
        }

        GLib.timeout_add(GLib.PRIORITY_DEFAULT, restoreDelayMs, () => {
            if (this.enabled) {
                this.internalWrite(selection, prevText, 'restore', true);
            }
            return GLib.SOURCE_REMOVE;
        });
    }

    // ---- internals ----

    private startPolling(selections: number[]): void {
        if (this.pollId !== null) {
            GLib.source_remove(this.pollId);
        }
        this.pollId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, this.pollIntervalMs, () => {
            if (!this.enabled) {
                return GLib.SOURCE_REMOVE;
            }
            selections.forEach(sel => this.onOwnerChange(sel));
            return GLib.SOURCE_CONTINUE;
        });
    }

    private onOwnerChange(selection: number): void {
        if (!this.enabled) {
            return;
        }
        const state = this.getState(selection);
        state.gen += 1;
        state.pendingGen = state.gen;

        if (state.debounceId !== null) {
            GLib.source_remove(state.debounceId);
            state.debounceId = null;
        }

        const scheduledGen = state.pendingGen;
        state.debounceId = GLib.timeout_add(
            GLib.PRIORITY_DEFAULT,
            this.graceDelayMs,
            () => {
                state.debounceId = null;
                if (scheduledGen !== null) {
                    this.process(selection, scheduledGen).catch(logError);
                }
                return GLib.SOURCE_REMOVE;
            },
        );
    }

    private async process(selection: number, genAtSchedule: number): Promise<void> {
        if (!this.enabled) {
            return;
        }
        const state = this.getState(selection);
        if (state.pendingGen !== genAtSchedule) {
            return; // stale
        }

        const text = await this.readText(selection);
        if (!this.enabled || state.pendingGen !== genAtSchedule) {
            return; // stale after read or disabled
        }
        if (!text) {
            return;
        }

        // Restore guard: ignore owner-change caused by restore/manual flows.
        const nowUsec = GLib.get_monotonic_time();
        if (state.restoreGuard) {
            if (nowUsec > state.restoreGuard.expiresUsec) {
                state.restoreGuard = null;
            } else {
                const incomingHash = hashText(text);
                if (incomingHash === state.restoreGuard.hash) {
                    return;
                }
                state.restoreGuard = null;
            }
        }

        const incomingHash = hashText(text);
        if (state.lastWrittenHash && incomingHash === state.lastWrittenHash) {
            // Consume the self-write guard.
            state.lastWrittenHash = null;
            state.lastWriteKind = null;
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

        if (!this.enabled || state.pendingGen !== genAtSchedule) {
            return; // stale right before write
        }

        state.lastOriginal = text;
        this.internalWrite(selection, result.output, 'trim');
    }

    protected readOptions(): TrimOptions {
        return {
            keep_blank_lines: this.settings.get_boolean('keep-blank-lines'),
            strip_box_chars: this.settings.get_boolean('strip-box-chars'),
            trim_prompts: this.settings.get_boolean('trim-prompts'),
            max_lines: this.settings.get_int('max-lines'),
        };
    }

    protected readText(selection: number): Promise<string | null> {
        return new Promise(resolve => {
            this.clipboard.get_text(selection, (_text: string | null) => {
                resolve(_text);
            });
        });
    }

    protected getState(selection: number): SelectionState {
        let state = this.states.get(selection);
        if (!state) {
            state = {
                gen: 0,
                pendingGen: null,
                debounceId: null,
                lastWrittenHash: null,
                lastWriteKind: null,
                restoreGuard: null,
                lastOriginal: null,
                lastTrimmed: null,
            };
            this.states.set(selection, state);
        }
        return state;
    }

    protected internalWrite(
        selection: number,
        text: string,
        kind: WriteKind,
        withRestoreGuard = false,
    ): void {
        const state = this.getState(selection);
        const hash = hashText(text);
        state.lastWrittenHash = hash;
        state.lastWriteKind = kind;
        if (withRestoreGuard) {
            const expiresUsec = GLib.get_monotonic_time() + 1_500_000;
            state.restoreGuard = {hash, expiresUsec};
        }

        this.updateSummary(text);
        this.clipboard.set_text(selection, text);
    }

    protected updateSummary(text: string): void {
        const singleLine = text.replace(/\n+/g, ' ').trim();
        const limit = 100;
        let summary = singleLine;
        if (summary.length > limit) {
            const keep = limit - 1;
            const head = Math.floor(keep / 2);
            const tail = keep - head;
            summary = `${summary.slice(0, head)}…${summary.slice(summary.length - tail)}`;
        }
        this.lastSummary = summary;
        this.onSummaryChanged?.(summary);
    }
}

function hashText(text: string): string {
    return GLib.compute_checksum_for_string(GLib.ChecksumType.SHA256, text, -1);
}
