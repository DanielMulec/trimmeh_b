import Extension from 'resource:///org/gnome/shell/extensions/extension.js';
import {PanelIndicator} from './panel.js';
import * as ExtensionUtils from 'resource:///org/gnome/shell/misc/extensionUtils.js';
import {ClipboardWatcher} from './clipboard.js';
import {createWasmTrimAdapter} from './wasm.js';

export default class TrimmehExtension extends Extension {
    private watcher: ClipboardWatcher | null = null;
    private panelIndicator: PanelIndicator | null = null;

    async enable(): Promise<void> {
        const settings = this.getSettings();
        const trimmer = await createWasmTrimAdapter(this.dir.get_path());
        this.watcher = new ClipboardWatcher(trimmer, settings);
        this.watcher.enable();

        this.panelIndicator = new PanelIndicator(settings, this.watcher);
        this.panelIndicator.addToPanel();
    }

    disable(): void {
        this.watcher?.disable();
        this.watcher = null;
        this.panelIndicator?.destroy();
        this.panelIndicator = null;
    }
}
