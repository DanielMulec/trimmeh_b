import Adw from 'gi://Adw';
import Gtk from 'gi://Gtk';
import Gio from 'gi://Gio';
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

        const info = new Adw.PreferencesGroup({title: 'Manual actions'});
        info.add(new Adw.ActionRow({
            title: 'Manual paste actions live in the top‑bar menu',
            subtitle: 'Use the Trimmeh panel menu for Paste Trimmed / Paste Original / Restore last copy.',
            activatable: false,
        }));

        page.add(toggles);
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
}
