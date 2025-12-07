import St from 'gi://St';
import Gio from 'gi://Gio';
import GObject from 'gi://GObject';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as ExtensionUtils from 'resource:///org/gnome/shell/misc/extensionUtils.js';
import {SystemIndicator, QuickToggle, QuickMenuToggle} from 'resource:///org/gnome/shell/ui/quickSettings.js';
import {ClipboardWatcher} from './clipboard.js';

const IndicatorClass = GObject.registerClass(
class QuickSettingsIndicator extends SystemIndicator {
    private toggle!: QuickToggle;
    private restoreButton!: QuickMenuToggle;
    private prefsButton!: QuickMenuToggle;
    private settingsChangedId: number | null = null;
    private settings!: Gio.Settings;
    private watcher!: ClipboardWatcher;
    private added = false;

    _init(settings: Gio.Settings, watcher: ClipboardWatcher) {
        super._init();
        this.settings = settings;
        this.watcher = watcher;

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'edit-cut-symbolic';

        this.toggle = new QuickToggle({
            label: 'Trimmeh',
            checked: this.settings.get_boolean('enable-auto-trim'),
            iconName: 'edit-cut-symbolic',
            toggleMode: true,
        });
        this.toggle.connect('clicked', () => {
            this.settings.set_boolean('enable-auto-trim', this.toggle.checked);
        });

        this.restoreButton = new QuickMenuToggle({
            label: 'Restore last copy',
            iconName: 'edit-undo-symbolic',
        });
        this.restoreButton.connect('clicked', () => {
            this.watcher.restore(St.ClipboardType.CLIPBOARD);
        });

        this.prefsButton = new QuickMenuToggle({
            label: 'Preferences…',
            iconName: 'emblem-system-symbolic',
        });
        this.prefsButton.connect('clicked', () => {
            ExtensionUtils.openPrefs();
        });

        // GNOME Shell will read this array when we add the indicator.
        this.quickSettingsItems = [this.toggle, this.restoreButton, this.prefsButton];
    }

    addToPanel(): void {
        if (this.added)
            return;
        Main.panel.statusArea.quickSettings.addExternalIndicator(this);
        this.settingsChangedId = this.settings.connect('changed::enable-auto-trim', () => {
            this.toggle.checked = this.settings.get_boolean('enable-auto-trim');
        });
        this.added = true;
    }

    destroy(): void {
        if (this.settingsChangedId) {
            this.settings.disconnect(this.settingsChangedId);
            this.settingsChangedId = null;
        }
        super.destroy();
    }
});

export {IndicatorClass as QuickSettingsIndicator};
