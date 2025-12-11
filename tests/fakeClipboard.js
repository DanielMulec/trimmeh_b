import GLib from 'gi://GLib';

export class FakeClipboard {
    constructor({emitCount = 1, readDelayMs = 0} = {}) {
        this._emitCount = emitCount;
        this._readDelayMs = readDelayMs;
        this._texts = new Map();
        this._handlers = new Map();
        this._nextId = 1;
        this.setCalls = [];
    }

    get_text(selection, cb) {
        const text = this._texts.get(selection) ?? null;
        if (this._readDelayMs > 0) {
            GLib.timeout_add(GLib.PRIORITY_DEFAULT, this._readDelayMs, () => {
                cb(text);
                return GLib.SOURCE_REMOVE;
            });
        } else {
            GLib.idle_add(GLib.PRIORITY_DEFAULT_IDLE, () => {
                cb(text);
                return GLib.SOURCE_REMOVE;
            });
        }
    }

    set_text(selection, text) {
        this._texts.set(selection, text);
        this.setCalls.push({selection, text});
        this._emitOwnerChange(selection);
    }

    connect_owner_change(cb) {
        const id = this._nextId++;
        this._handlers.set(id, cb);
        return id;
    }

    disconnect(id) {
        this._handlers.delete(id);
    }

    // Test helper: simulate a user copy without counting as watcher write.
    set_user_text(selection, text) {
        this._texts.set(selection, text);
        this._emitOwnerChange(selection);
    }

    text(selection) {
        return this._texts.get(selection) ?? null;
    }

    _emitOwnerChange(selection) {
        for (let i = 0; i < this._emitCount; i++) {
            GLib.idle_add(GLib.PRIORITY_DEFAULT_IDLE, () => {
                for (const cb of this._handlers.values()) {
                    try {
                        cb(selection);
                    } catch (e) {
                        logError(e);
                    }
                }
                return GLib.SOURCE_REMOVE;
            });
        }
    }
}

