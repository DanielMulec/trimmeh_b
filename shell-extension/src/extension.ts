import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import {QuickSettingsIndicator} from './indicator.js';
import * as ExtensionUtils from 'resource:///org/gnome/shell/misc/extensionUtils.js';
import {ClipboardWatcher} from './clipboard.js';
import {createWasmTrimAdapter} from './wasm.js';

export default class TrimmehExtension extends Extension {
    private watcher: ClipboardWatcher | null = null;
    private indicator: QuickSettingsIndicator | null = null;

    async enable(): Promise<void> {
        const settings = this.getSettings();
        const trimmer = await createWasmTrimAdapter(this.dir.get_path());
        this.watcher = new ClipboardWatcher(trimmer, settings);
        this.watcher.enable();

        this.indicator = new QuickSettingsIndicator(settings, this.watcher);
        this.indicator.enable();
    }

    disable(): void {
        this.watcher?.disable();
        this.watcher = null;
        this.indicator?.disable();
        this.indicator = null;
    }
}
