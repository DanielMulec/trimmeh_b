import St from 'gi://St';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import {SystemIndicator, QuickToggle, QuickMenuToggle} from 'resource:///org/gnome/shell/ui/quickSettings.js';
import * as ExtensionUtils from 'resource:///org/gnome/shell/misc/extensionUtils.js';
import Gio from 'gi://Gio';
import {ClipboardWatcher} from './clipboard.js';

export class QuickSettingsIndicator extends SystemIndicator {
    private toggle: QuickToggle;
    private restoreButton: QuickMenuToggle;
    private settings: Gio.Settings;
    private watcher: ClipboardWatcher;
    private settingsChangedId: number | null = null;

    constructor(settings: Gio.Settings, watcher: ClipboardWatcher) {
        super();
        this.settings = settings;
        this.watcher = watcher;

        this.toggle = new QuickToggle({
            label: 'Trimmeh',
            checked: this.settings.get_boolean('enable-auto-trim'),
            iconName: 'edit-cut-symbolic',
            toggleMode: true,
        });
        this.toggle.connect('toggled', () => {
            this.settings.set_boolean('enable-auto-trim', this.toggle.checked);
        });

        this.restoreButton = new QuickMenuToggle({
            label: 'Restore last copy',
            iconName: 'edit-undo-symbolic',
        });
        this.restoreButton.connect('clicked', () => {
            this.watcher.restore(St.ClipboardType.CLIPBOARD);
        });

        const prefsButton = new QuickMenuToggle({
            label: 'Preferences…',
            iconName: 'emblem-system-symbolic',
        });
        prefsButton.connect('clicked', () => {
            ExtensionUtils.openPrefs();
        });

        this._addItems([this.toggle, this.restoreButton, prefsButton]);
        this.addIndicator();
    }

    enable(): void {
        Main.panel.statusArea.quickSettings.addExternalIndicator(this);
        this.settingsChangedId = this.settings.connect('changed::enable-auto-trim', () => {
            this.toggle.checked = this.settings.get_boolean('enable-auto-trim');
        });
    }

    disable(): void {
        if (this.settingsChangedId) {
            this.settings.disconnect(this.settingsChangedId);
            this.settingsChangedId = null;
        }
        this.destroy();
    }
}
