import Adw from 'gi://Adw';
import Gtk from 'gi://Gtk';
import {ExtensionPreferences} from 'resource:///org/gnome/Shell/Extensions/js/extensions/prefs.js';

export default class TrimmehPreferences extends ExtensionPreferences {
    fillPreferencesWindow(window: Adw.PreferencesWindow): void {
        const page = new Adw.PreferencesPage({title: 'Trimmeh'});
        const group = new Adw.PreferencesGroup({title: 'Settings'});
        group.add(new Adw.ActionRow({title: 'Preferences UI coming soon'}));
        page.add(group);
        window.add(page);
        window.set_default_size(520, 520);
    }
}
