#include "settings_store.h"

#include <QSettings>

namespace {
constexpr const char kAutoTrimEnabled[] = "autoTrimEnabled";
constexpr const char kKeepBlankLines[] = "keepBlankLines";
constexpr const char kStripBoxChars[] = "stripBoxChars";
constexpr const char kTrimPrompts[] = "trimPrompts";
constexpr const char kMaxLines[] = "maxLines";
constexpr const char kAggressiveness[] = "aggressiveness";
constexpr const char kStartAtLogin[] = "startAtLogin";
}

Settings SettingsStore::load() const {
    Settings settings;
    QSettings store;
    settings.autoTrimEnabled = store.value(kAutoTrimEnabled, settings.autoTrimEnabled).toBool();
    settings.keepBlankLines = store.value(kKeepBlankLines, settings.keepBlankLines).toBool();
    settings.stripBoxChars = store.value(kStripBoxChars, settings.stripBoxChars).toBool();
    settings.trimPrompts = store.value(kTrimPrompts, settings.trimPrompts).toBool();
    settings.maxLines = store.value(kMaxLines, settings.maxLines).toInt();
    settings.aggressiveness = store.value(kAggressiveness, settings.aggressiveness).toString();
    settings.startAtLogin = store.value(kStartAtLogin, settings.startAtLogin).toBool();
    return settings;
}

void SettingsStore::save(const Settings &settings) const {
    QSettings store;
    store.setValue(kAutoTrimEnabled, settings.autoTrimEnabled);
    store.setValue(kKeepBlankLines, settings.keepBlankLines);
    store.setValue(kStripBoxChars, settings.stripBoxChars);
    store.setValue(kTrimPrompts, settings.trimPrompts);
    store.setValue(kMaxLines, settings.maxLines);
    store.setValue(kAggressiveness, settings.aggressiveness);
    store.setValue(kStartAtLogin, settings.startAtLogin);
    store.sync();
}
