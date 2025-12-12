import Adw from 'gi://Adw';
import Gtk from 'gi://Gtk';
import Gio from 'gi://Gio';
import Gdk from 'gi://Gdk';
import {ExtensionPreferences} from 'resource:///org/gnome/Shell/Extensions/js/extensions/prefs.js';

export default class TrimmehPreferences extends ExtensionPreferences {
    fillPreferencesWindow(window: Adw.PreferencesWindow): void {
        const settings = this.getSettings();
        const page = new Adw.PreferencesPage({title: 'Trimmeh'});
        const toggles = new Adw.PreferencesGroup({title: 'Behavior'});

        toggles.add(this.switchRow('Enable auto trim', settings, 'enable-auto-trim'));
        toggles.add(this.switchRow('Strip shell prompts', settings, 'trim-prompts'));
        toggles.add(this.switchRow('Strip box gutters', settings, 'strip-box-chars'));
        toggles.add(this.switchRow('Keep blank lines', settings, 'keep-blank-lines'));

        const aggrRow = this.aggressivenessRow(settings);
        toggles.add(aggrRow);

        const maxLinesRow = new Adw.ActionRow({
            title: 'Maximum lines to process',
            subtitle: 'Skip blobs beyond this line count',
        });
        const spin = new Gtk.SpinButton({
            adjustment: new Gtk.Adjustment({
                lower: 1,
                upper: 1000,
                step_increment: 1,
                page_increment: 5,
                value: settings.get_int('max-lines'),
            }),
        });
        settings.bind('max-lines', spin, 'value', Gio.SettingsBindFlags.DEFAULT);
        maxLinesRow.add_suffix(spin);
        maxLinesRow.set_activatable_widget(spin);
        toggles.add(maxLinesRow);

        const hotkeys = new Adw.PreferencesGroup({title: 'Hotkeys'});
        hotkeys.add(this.keybindingRow(
            window,
            settings,
            'paste-trimmed-hotkey',
            'Paste Trimmed',
            'Temporarily swaps clipboard to a High-aggressiveness trimmed version, pastes, then restores.',
        ));
        hotkeys.add(this.keybindingRow(
            window,
            settings,
            'paste-original-hotkey',
            'Paste Original',
            'Temporarily swaps clipboard to the last untrimmed copy, pastes, then restores.',
        ));
        hotkeys.add(this.keybindingRow(
            window,
            settings,
            'toggle-auto-trim-hotkey',
            'Toggle Auto Trim',
            'Optional hotkey to toggle auto-trim on/off.',
        ));

        const info = new Adw.PreferencesGroup({title: 'Manual actions'});
        info.add(new Adw.ActionRow({
            title: 'Manual paste actions live in the top‑bar menu',
            subtitle: 'Use the Trimmeh panel menu for Paste Trimmed / Paste Original / Restore last copy.',
            activatable: false,
        }));

        page.add(toggles);
        page.add(hotkeys);
        page.add(info);
        window.add(page);
        window.set_default_size(520, 520);
    }

    private switchRow(title: string, settings: Gio.Settings, key: string): Adw.ActionRow {
        const row = new Adw.ActionRow({title});
        const sw = new Gtk.Switch({valign: Gtk.Align.CENTER});
        settings.bind(key, sw, 'active', Gio.SettingsBindFlags.DEFAULT);
        row.add_suffix(sw);
        row.set_activatable_widget(sw);
        return row;
    }

    private aggressivenessRow(settings: Gio.Settings): Adw.ActionRow {
        const strings = new Gtk.StringList();
        ['low', 'normal', 'high'].forEach(s => strings.append(s));
        const row = new Adw.ComboRow({
            title: 'Aggressiveness',
            model: strings,
        });
        const current = settings.get_string('aggressiveness');
        row.selected = ['low', 'normal', 'high'].indexOf(current);
        row.connect('notify::selected', () => {
            const value = strings.get_string(row.selected);
            if (value) settings.set_string('aggressiveness', value);
        });
        settings.connect('changed::aggressiveness', () => {
            const idx = ['low', 'normal', 'high'].indexOf(settings.get_string('aggressiveness'));
            row.selected = idx >= 0 ? idx : 1;
        });
        return row;
    }

    private keybindingRow(
        window: Adw.PreferencesWindow,
        settings: Gio.Settings,
        key: string,
        title: string,
        subtitle: string,
    ): Adw.ActionRow {
        const row = new Adw.ActionRow({title, subtitle});

        const setButton = new Gtk.Button({
            label: this.formatHotkey(this.getHotkey(settings, key)),
            valign: Gtk.Align.CENTER,
        });
        setButton.connect('clicked', () => {
            this.openHotkeyCaptureDialog(window, settings, key, title);
        });

        const clearButton = new Gtk.Button({
            label: 'Clear',
            valign: Gtk.Align.CENTER,
        });
        clearButton.connect('clicked', () => {
            this.setHotkey(settings, key, null);
        });

        const update = () => {
            setButton.set_label(this.formatHotkey(this.getHotkey(settings, key)));
        };
        settings.connect(`changed::${key}`, update);

        row.add_suffix(setButton);
        row.add_suffix(clearButton);
        row.set_activatable_widget(setButton);

        return row;
    }

    private getHotkey(settings: Gio.Settings, key: string): string | null {
        const values = ((settings as any).get_strv?.(key) ?? []) as string[];
        return values.length > 0 ? values[0] : null;
    }

    private setHotkey(settings: Gio.Settings, key: string, accelerator: string | null): void {
        const value = accelerator ? [accelerator] : [];
        (settings as any).set_strv?.(key, value);
    }

    private formatHotkey(accelerator: string | null): string {
        return accelerator?.length ? accelerator : 'Disabled';
    }

    private openHotkeyCaptureDialog(
        parent: Adw.PreferencesWindow,
        settings: Gio.Settings,
        key: string,
        title: string,
    ): void {
        const dialog = new Adw.Window({
            transient_for: parent,
            modal: true,
            title: `Set hotkey: ${title}`,
        });
        dialog.set_default_size(520, 160);

        const content = new Gtk.Box({
            orientation: Gtk.Orientation.VERTICAL,
            spacing: 12,
            margin_top: 18,
            margin_bottom: 18,
            margin_start: 18,
            margin_end: 18,
        });

        content.append(new Gtk.Label({
            label: 'Press the new shortcut now.\nEsc cancels. Backspace/Delete clears.',
            wrap: true,
            xalign: 0,
        }));

        const current = this.getHotkey(settings, key);
        content.append(new Gtk.Label({
            label: `Current: ${this.formatHotkey(current)}`,
            wrap: true,
            xalign: 0,
        }));

        dialog.set_content(content);

        const controller = new Gtk.EventControllerKey();
        controller.connect('key-pressed', (_c: unknown, keyval: number, _keycode: number, state: number) => {
            const mods = this.filterAcceleratorModifiers(state);

            if (keyval === Gdk.KEY_Escape) {
                dialog.close();
                return true;
            }

            if (keyval === Gdk.KEY_BackSpace || keyval === Gdk.KEY_Delete) {
                this.setHotkey(settings, key, null);
                dialog.close();
                return true;
            }

            if (this.isModifierKey(keyval)) {
                return true;
            }

            // Avoid capturing unmodified printable keys (would break normal typing).
            const requiredMods =
                Gdk.ModifierType.CONTROL_MASK |
                Gdk.ModifierType.MOD1_MASK |
                Gdk.ModifierType.SUPER_MASK;
            if ((mods & requiredMods) === 0) {
                return true;
            }

            const normalizedKeyval = Gdk.keyval_to_lower(keyval);
            const accel = Gtk.accelerator_name(normalizedKeyval, mods);
            if (!accel?.length) {
                return true;
            }

            this.setHotkey(settings, key, accel);
            dialog.close();
            return true;
        });
        dialog.add_controller(controller);

        dialog.present();
    }

    private filterAcceleratorModifiers(state: number): number {
        return state & (
            Gdk.ModifierType.SHIFT_MASK |
            Gdk.ModifierType.CONTROL_MASK |
            Gdk.ModifierType.MOD1_MASK |
            Gdk.ModifierType.SUPER_MASK |
            Gdk.ModifierType.META_MASK |
            Gdk.ModifierType.HYPER_MASK
        );
    }

    private isModifierKey(keyval: number): boolean {
        // Modifier-only presses shouldn't be bindable.
        return keyval === Gdk.KEY_Shift_L ||
            keyval === Gdk.KEY_Shift_R ||
            keyval === Gdk.KEY_Control_L ||
            keyval === Gdk.KEY_Control_R ||
            keyval === Gdk.KEY_Alt_L ||
            keyval === Gdk.KEY_Alt_R ||
            keyval === Gdk.KEY_Super_L ||
            keyval === Gdk.KEY_Super_R ||
            keyval === Gdk.KEY_Meta_L ||
            keyval === Gdk.KEY_Meta_R ||
            keyval === Gdk.KEY_Hyper_L ||
            keyval === Gdk.KEY_Hyper_R;
    }
}
