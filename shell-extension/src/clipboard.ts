import St from 'gi://St';
import Gio from 'gi://Gio';
import type {ClipboardLike} from './clipboardWatcher.js';
import {ClipboardWatcher as CoreWatcher} from './clipboardWatcher.js';
import type {Trimmer} from './trimmer.js';
import {pasteWithFallback} from './virtualPaste.js';

class StClipboardAdapter implements ClipboardLike {
    private clipboard = St.Clipboard.get_default();

    get_text(selection: number, cb: (text: string | null) => void): void {
        this.clipboard.get_text(selection, (_clip: unknown, text: string | null) => cb(text));
    }

    set_text(selection: number, text: string): void {
        this.clipboard.set_text(selection, text);
    }

    connect_owner_change(cb: (selection: number) => void): number {
        const handler = (_clip: unknown, selection: number) => cb(selection);
        return this.clipboard.connect('owner-change', handler);
    }

    disconnect(id: number): void {
        this.clipboard.disconnect(id);
    }
}

export class ClipboardWatcher extends CoreWatcher {
    constructor(trimmer: Trimmer, settings: Gio.Settings) {
        super(new StClipboardAdapter(), trimmer, settings);
    }

    enable(): void {
        super.enable([St.ClipboardType.CLIPBOARD, St.ClipboardType.PRIMARY]);
    }

    pasteTrimmed(selection: number = St.ClipboardType.CLIPBOARD): Promise<void> {
        return super.pasteTrimmed(selection, pasteWithFallback);
    }

    pasteOriginal(selection: number = St.ClipboardType.CLIPBOARD): Promise<void> {
        return super.pasteOriginal(selection, pasteWithFallback);
    }
}
