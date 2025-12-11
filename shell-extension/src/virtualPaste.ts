import Clutter from 'gi://Clutter';

let virtualKeyboard: any | null = null;

function getVirtualKeyboard(): any {
    if (virtualKeyboard) {
        return virtualKeyboard;
    }
    const seat = Clutter.get_default_backend().get_default_seat();
    virtualKeyboard = seat.create_virtual_device(Clutter.InputDeviceType.KEYBOARD_DEVICE);
    return virtualKeyboard;
}

export function pasteCtrlV(): void {
    const vk = getVirtualKeyboard();
    const eventTime = (Clutter.get_current_event_time() || 0) * 1000;
    vk.notify_keyval(eventTime, Clutter.KEY_Control_L, Clutter.KeyState.PRESSED);
    vk.notify_keyval(eventTime, Clutter.KEY_v, Clutter.KeyState.PRESSED);
    vk.notify_keyval(eventTime, Clutter.KEY_v, Clutter.KeyState.RELEASED);
    vk.notify_keyval(eventTime, Clutter.KEY_Control_L, Clutter.KeyState.RELEASED);
}

export function pasteShiftInsert(): void {
    const vk = getVirtualKeyboard();
    const eventTime = (Clutter.get_current_event_time() || 0) * 1000;
    vk.notify_keyval(eventTime, Clutter.KEY_Shift_L, Clutter.KeyState.PRESSED);
    vk.notify_keyval(eventTime, Clutter.KEY_Insert, Clutter.KeyState.PRESSED);
    vk.notify_keyval(eventTime, Clutter.KEY_Insert, Clutter.KeyState.RELEASED);
    vk.notify_keyval(eventTime, Clutter.KEY_Shift_L, Clutter.KeyState.RELEASED);
}

export function pasteWithFallback(): void {
    try {
        pasteCtrlV();
    } catch (e) {
        logError(e);
        try {
            pasteShiftInsert();
        } catch (e2) {
            logError(e2);
        }
    }
}

