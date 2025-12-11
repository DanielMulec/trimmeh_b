import Gio from 'gi://Gio';
import GObject from 'gi://GObject';
import St from 'gi://St';
import * as PanelMenu from 'resource:///org/gnome/shell/ui/panelMenu.js';
import * as PopupMenu from 'resource:///org/gnome/shell/ui/popupMenu.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import {ClipboardWatcher} from './clipboard.js';

const PanelIndicatorClass = GObject.registerClass(
class TrimmehPanelIndicator extends PanelMenu.Button {
    private settings!: Gio.Settings;
    private watcher!: ClipboardWatcher;
    private autoItem!: PopupMenu.PopupSwitchMenuItem;
    private openPrefs?: () => void;

    _init(settings: Gio.Settings, watcher: ClipboardWatcher, openPrefs?: () => void) {
        super._init(0.0, 'Trimmeh');
        this.settings = settings;
        this.watcher = watcher;
        this.openPrefs = openPrefs;

        const icon = new St.Icon({ icon_name: 'edit-cut-symbolic', style_class: 'system-status-icon' });
        this.add_child(icon);

        this.autoItem = new PopupMenu.PopupSwitchMenuItem(
            'Auto trim clipboard',
            this.settings.get_boolean('enable-auto-trim'),
        );
        this.autoItem.connect('toggled', (_item, state: boolean) => {
            this.settings.set_boolean('enable-auto-trim', state);
        });

        const pasteTrimmedItem = new PopupMenu.PopupMenuItem('Paste Trimmed');
        pasteTrimmedItem.connect('activate', () => {
            this.watcher.pasteTrimmed(St.ClipboardType.CLIPBOARD).catch(logError);
        });

        const pasteOriginalItem = new PopupMenu.PopupMenuItem('Paste Original');
        pasteOriginalItem.connect('activate', () => {
            this.watcher.pasteOriginal(St.ClipboardType.CLIPBOARD).catch(logError);
        });

        const restoreItem = new PopupMenu.PopupMenuItem('Restore last copy');
        restoreItem.connect('activate', () => {
            this.watcher.restore(St.ClipboardType.CLIPBOARD);
        });

        const prefsItem = new PopupMenu.PopupMenuItem('Preferences…');
        prefsItem.connect('activate', () => {
            if (this.openPrefs) {
                this.openPrefs();
            } else {
                log('Trimmeh: preferences callback not set');
            }
        });

        this.menu.addMenuItem(this.autoItem);
        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this.menu.addMenuItem(pasteTrimmedItem);
        this.menu.addMenuItem(pasteOriginalItem);
        this.menu.addMenuItem(restoreItem);
        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this.menu.addMenuItem(prefsItem);

        this.settings.connect('changed::enable-auto-trim', () => {
            this.autoItem.setToggleState(this.settings.get_boolean('enable-auto-trim'));
        });
    }

    addToPanel(): void {
        Main.panel.addToStatusArea('trimmeh-panel', this);
    }

    destroy(): void {
        super.destroy();
    }
});

export {PanelIndicatorClass as PanelIndicator};
