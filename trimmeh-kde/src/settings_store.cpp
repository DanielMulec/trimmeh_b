#include "settings_store.h"

#include <QSettings>

namespace {
constexpr const char kAutoTrimEnabled[] = "autoTrimEnabled";
constexpr const char kKeepBlankLines[] = "keepBlankLines";
constexpr const char kStripBoxChars[] = "stripBoxChars";
constexpr const char kTrimPrompts[] = "trimPrompts";
constexpr const char kUseClipboardFallbacks[] = "useClipboardFallbacks";
constexpr const char kMaxLines[] = "maxLines";
constexpr const char kAggressiveness[] = "aggressiveness";
constexpr const char kStartAtLogin[] = "startAtLogin";
constexpr const char kPasteRestoreDelayMs[] = "pasteRestoreDelayMs";
constexpr const char kPasteInjectDelayMs[] = "pasteInjectDelayMs";
constexpr const char kPasteTrimmedHotkeyEnabled[] = "pasteTrimmedHotkeyEnabled";
constexpr const char kPasteOriginalHotkeyEnabled[] = "pasteOriginalHotkeyEnabled";
constexpr const char kToggleAutoTrimHotkeyEnabled[] = "toggleAutoTrimHotkeyEnabled";
constexpr const char kPasteTrimmedHotkey[] = "pasteTrimmedHotkey";
constexpr const char kPasteOriginalHotkey[] = "pasteOriginalHotkey";
constexpr const char kToggleAutoTrimHotkey[] = "toggleAutoTrimHotkey";
}

Settings SettingsStore::load() const {
    Settings settings;
    QSettings store;
    settings.autoTrimEnabled = store.value(kAutoTrimEnabled, settings.autoTrimEnabled).toBool();
    settings.keepBlankLines = store.value(kKeepBlankLines, settings.keepBlankLines).toBool();
    settings.stripBoxChars = store.value(kStripBoxChars, settings.stripBoxChars).toBool();
    settings.trimPrompts = store.value(kTrimPrompts, settings.trimPrompts).toBool();
    settings.useClipboardFallbacks = store.value(kUseClipboardFallbacks, settings.useClipboardFallbacks).toBool();
    settings.maxLines = store.value(kMaxLines, settings.maxLines).toInt();
    settings.aggressiveness = store.value(kAggressiveness, settings.aggressiveness).toString();
    settings.startAtLogin = store.value(kStartAtLogin, settings.startAtLogin).toBool();
    settings.pasteRestoreDelayMs = store.value(kPasteRestoreDelayMs, settings.pasteRestoreDelayMs).toInt();
    settings.pasteInjectDelayMs = store.value(kPasteInjectDelayMs, settings.pasteInjectDelayMs).toInt();
    settings.pasteTrimmedHotkeyEnabled = store.value(kPasteTrimmedHotkeyEnabled, settings.pasteTrimmedHotkeyEnabled).toBool();
    settings.pasteOriginalHotkeyEnabled = store.value(kPasteOriginalHotkeyEnabled, settings.pasteOriginalHotkeyEnabled).toBool();
    settings.toggleAutoTrimHotkeyEnabled = store.value(kToggleAutoTrimHotkeyEnabled, settings.toggleAutoTrimHotkeyEnabled).toBool();
    settings.pasteTrimmedHotkey = store.value(kPasteTrimmedHotkey, settings.pasteTrimmedHotkey).toString();
    settings.pasteOriginalHotkey = store.value(kPasteOriginalHotkey, settings.pasteOriginalHotkey).toString();
    settings.toggleAutoTrimHotkey = store.value(kToggleAutoTrimHotkey, settings.toggleAutoTrimHotkey).toString();
    return settings;
}

void SettingsStore::save(const Settings &settings) const {
    QSettings store;
    store.setValue(kAutoTrimEnabled, settings.autoTrimEnabled);
    store.setValue(kKeepBlankLines, settings.keepBlankLines);
    store.setValue(kStripBoxChars, settings.stripBoxChars);
    store.setValue(kTrimPrompts, settings.trimPrompts);
    store.setValue(kUseClipboardFallbacks, settings.useClipboardFallbacks);
    store.setValue(kMaxLines, settings.maxLines);
    store.setValue(kAggressiveness, settings.aggressiveness);
    store.setValue(kStartAtLogin, settings.startAtLogin);
    store.setValue(kPasteRestoreDelayMs, settings.pasteRestoreDelayMs);
    store.setValue(kPasteInjectDelayMs, settings.pasteInjectDelayMs);
    store.setValue(kPasteTrimmedHotkeyEnabled, settings.pasteTrimmedHotkeyEnabled);
    store.setValue(kPasteOriginalHotkeyEnabled, settings.pasteOriginalHotkeyEnabled);
    store.setValue(kToggleAutoTrimHotkeyEnabled, settings.toggleAutoTrimHotkeyEnabled);
    store.setValue(kPasteTrimmedHotkey, settings.pasteTrimmedHotkey);
    store.setValue(kPasteOriginalHotkey, settings.pasteOriginalHotkey);
    store.setValue(kToggleAutoTrimHotkey, settings.toggleAutoTrimHotkey);
    store.sync();
}
