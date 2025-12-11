import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import {PanelIndicator} from './panel.js';
import {ClipboardWatcher} from './clipboard.js';
import {createWasmTrimAdapter} from './wasm.js';
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
        const trimmer = await createWasmTrimAdapter(this.dir.get_path());
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
        const mode = Shell.ActionMode.ALL;
        const flags = Meta.KeyBindingFlags.IGNORE_AUTOREPEAT;

        const add = (name: string, handler: () => void) => {
            try {
                if ((Main.wm as any).addKeybinding) {
                    (Main.wm as any).addKeybinding(name, settings, flags, mode, handler);
                } else if (global.display?.add_keybinding) {
                    global.display.add_keybinding(name, settings, flags, handler);
                } else {
                    log(`Trimmeh: no keybinding API available for ${name}`);
                    return;
                }
                this.keybindingNames.push(name);
            } catch (e) {
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
