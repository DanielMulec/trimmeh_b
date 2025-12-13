import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import {PanelIndicator} from './panel.js';
import {ClipboardWatcher} from './clipboard.js';
import {createTrimAdapter} from './trimmer.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import Gio from 'gi://Gio';

export default class TrimmehExtension extends Extension {
    private watcher: ClipboardWatcher | null = null;
    private panelIndicator: PanelIndicator | null = null;
    private keybindingNames: string[] = [];

    async enable(): Promise<void> {
        const settings = this.getSettings();
        const trimmer = createTrimAdapter();
        this.watcher = new ClipboardWatcher(trimmer, settings);
        this.watcher.enable();

        this.addKeybindings(settings);

        this.panelIndicator = new PanelIndicator(settings, this.watcher, () => {
            this.openPreferences();
        });
        this.panelIndicator.addToPanel();
    }

    disable(): void {
        this.removeKeybindings();
        this.watcher?.disable();
        this.watcher = null;
        this.panelIndicator?.destroy();
        this.panelIndicator = null;
    }

    private addKeybindings(settings: Gio.Settings): void {
        const actionMode: any = (Shell as any)?.ActionMode;
        const mode =
            (actionMode?.NORMAL ?? 0) |
            (actionMode?.OVERVIEW ?? 0);
        const flags =
            (Meta as any)?.KeyBindingFlags?.IGNORE_AUTOREPEAT ??
            (Meta as any)?.KeyBindingFlags?.NONE ??
            0;

        const add = (name: string, handler: () => void) => {
            try {
                // GNOME 48/49 use: (name, settings, flags, modes, handler)
                (Main.wm as any).addKeybinding(name, settings, flags, mode, handler);
                (Main.wm as any).allowKeybinding?.(name, mode);
                this.keybindingNames.push(name);
            } catch (e) {
                // If GNOME ever changes the signature again, we'll see this.
                logError(e);
            }
        };

        add('paste-trimmed-hotkey', () => {
            this.watcher?.pasteTrimmed().catch(logError);
        });
        add('paste-original-hotkey', () => {
            this.watcher?.pasteOriginal().catch(logError);
        });
        add('toggle-auto-trim-hotkey', () => {
            const current = settings.get_boolean('enable-auto-trim');
            settings.set_boolean('enable-auto-trim', !current);
        });
    }

    private removeKeybindings(): void {
        for (const name of this.keybindingNames) {
            try {
                if ((Main.wm as any).removeKeybinding) {
                    (Main.wm as any).removeKeybinding(name);
                } else if (global.display?.remove_keybinding) {
                    global.display.remove_keybinding(name);
                }
            } catch (e) {
                logError(e);
            }
        }
        this.keybindingNames = [];
    }
}
