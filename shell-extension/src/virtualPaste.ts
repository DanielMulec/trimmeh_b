import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';

let virtualKeyboard: any | null = null;

function getVirtualKeyboard(): any {
    if (virtualKeyboard) {
        return virtualKeyboard;
    }
    const seat = Clutter.get_default_backend().get_default_seat();
    virtualKeyboard = seat.create_virtual_device(Clutter.InputDeviceType.KEYBOARD_DEVICE);
    return virtualKeyboard;
}

function nowUsec(): number {
    // VirtualInputDevice APIs expect a monotonic timestamp in microseconds.
    return GLib.get_monotonic_time();
}

function currentModifierMask(): number {
    try {
        const res = (global as any).get_pointer?.();
        if (Array.isArray(res) && res.length >= 3) {
            return res[2] as number;
        }
    } catch (e) {
        // ignore
    }
    return 0;
}

async function waitForHotkeyModifiersUp(timeoutMs = 250): Promise<boolean> {
    // If we inject paste while the user is still holding the hotkey modifiers
    // (e.g. Super/Alt/Shift), apps receive Ctrl+Alt+Super+V which typically does nothing.
    const mask =
        Clutter.ModifierType.SUPER_MASK |
        Clutter.ModifierType.MOD1_MASK |
        Clutter.ModifierType.SHIFT_MASK;
    const deadlineUsec = GLib.get_monotonic_time() + timeoutMs * 1000;

    // Fast-path.
    if ((currentModifierMask() & mask) === 0) {
        return false;
    }

    return await new Promise<boolean>(resolve => {
        GLib.timeout_add(GLib.PRIORITY_DEFAULT, 5, () => {
            const mods = currentModifierMask();
            const timedOut = GLib.get_monotonic_time() >= deadlineUsec;
            if ((mods & mask) === 0 || timedOut) {
                resolve(timedOut);
                return GLib.SOURCE_REMOVE;
            }
            return GLib.SOURCE_CONTINUE;
        });
    });
}

function keyvalForChar(char: string): number {
    const cp = char.codePointAt(0);
    if (cp === undefined) {
        return 0;
    }
    return Clutter.unicode_to_keysym(cp);
}

export function pasteCtrlV(): void {
    const vk = getVirtualKeyboard();
    const t0 = nowUsec();
    const keyV =
        (typeof (Clutter as any).KEY_v === 'number' && (Clutter as any).KEY_v) ||
        (typeof (Clutter as any).KEY_V === 'number' && (Clutter as any).KEY_V) ||
        keyvalForChar('v');
    vk.notify_keyval(t0 + 0, Clutter.KEY_Control_L, Clutter.KeyState.PRESSED);
    vk.notify_keyval(t0 + 1, keyV, Clutter.KeyState.PRESSED);
    vk.notify_keyval(t0 + 2, keyV, Clutter.KeyState.RELEASED);
    vk.notify_keyval(t0 + 3, Clutter.KEY_Control_L, Clutter.KeyState.RELEASED);
}

export function pasteShiftInsert(): void {
    const vk = getVirtualKeyboard();
    const t0 = nowUsec();
    vk.notify_keyval(t0 + 0, Clutter.KEY_Shift_L, Clutter.KeyState.PRESSED);
    vk.notify_keyval(t0 + 1, Clutter.KEY_Insert, Clutter.KeyState.PRESSED);
    vk.notify_keyval(t0 + 2, Clutter.KEY_Insert, Clutter.KeyState.RELEASED);
    vk.notify_keyval(t0 + 3, Clutter.KEY_Shift_L, Clutter.KeyState.RELEASED);
}

export async function pasteWithFallback(): Promise<void> {
    const startUsec = GLib.get_monotonic_time();
    const timedOut = await waitForHotkeyModifiersUp();
    if (timedOut) {
        const mods = currentModifierMask();
        log(`Trimmeh: paste injection timed out waiting for modifiers; mods=${mods}`);
    } else {
        const waitedMs = Math.round((GLib.get_monotonic_time() - startUsec) / 1000);
        if (waitedMs >= 25) {
            log(`Trimmeh: paste injection delayed ${waitedMs}ms (waiting for modifiers)`);
        }
    }

    try {
        // Prefer Shift+Insert: works in terminals (VTE) and most GUI apps.
        pasteShiftInsert();
    } catch (e) {
        logError(e);
        try {
            pasteCtrlV();
        } catch (e2) {
            logError(e2);
        }
    }
}
